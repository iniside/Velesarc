// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainCBTRenderer.h"

#include "ArcTerrainDiagnostics.h"
#include "Render/ArcTerrainDensityTiers.h"
#include "Render/ArcTerrainHeightBounds.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "Math/TranslationMatrix.h"
#include "Modules/ModuleManager.h"
#include "RHIGlobals.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RendererInterface.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

DECLARE_GPU_STAT(ArcTerrainCBT);
DECLARE_GPU_STAT(ArcTerrainCBTMotionRefine);

DECLARE_CYCLE_STAT(TEXT("RT: CBT submit"), STAT_ArcTerrainCBTSubmit, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("RT: CBT counter readback drain"), STAT_ArcTerrainCBTReadbackDrain,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("RT: CBT classify dispatch setup"), STAT_ArcTerrainCBTClassifySetup,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("RT: CBT compact dispatch setup"), STAT_ArcTerrainCBTCompactSetup,
	STATGROUP_ArcTerrain);

DECLARE_DWORD_COUNTER_STAT(TEXT("CBT triangles live"), STAT_ArcTerrainCBTLive, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("CBT pool slots allocated this frame"), STAT_ArcTerrainCBTPoolAllocated,
	STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("CBT triangles drawn (needs arc.Terrain.CBT.Stats)"),
	STAT_ArcTerrainCBTDrawn, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("CBT triangles culled (needs arc.Terrain.CBT.Stats)"),
	STAT_ArcTerrainCBTCulled, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("CBT splits applied (needs arc.Terrain.CBT.Stats)"),
	STAT_ArcTerrainCBTSplitApplied, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("CBT merges applied (needs arc.Terrain.CBT.Stats)"),
	STAT_ArcTerrainCBTMergeApplied, STATGROUP_ArcTerrain);

// Default verbosity must be Log, not Display: every diagnostic here is UE_LOG(..., Log, ...) and a
// Display default silently suppresses all of them.
DEFINE_LOG_CATEGORY_STATIC(LogArcTerrainCBT, Log, All);

namespace ArcTerrainCBTRendererPrivate_9a07
{
	/** Threads per group — must match each shader's numthreads. */
	constexpr int32 ClassifyThreads = 64;
	constexpr int32 SplitMergeThreads = 64;
	constexpr int32 FaceThreads = 64;
	constexpr int32 PoolThreads = 64;
	constexpr int32 ReduceThreads = 64;
	constexpr int32 LinkThreads = 64;
	constexpr int32 CompactThreads = 64;

	/** Cap on the dispatched group count for the grid-strided reduction (per-dim limit / anti-TDR). */
	constexpr int32 MaxReduceGroups = 65535;

	// One indirect args block sizes both per-bisector passes, so their group sizes cannot drift apart:
	// a larger split/merge group would under-dispatch it and silently drop topology work.
	static_assert(ClassifyThreads == SplitMergeThreads,
		"The classify and split/merge passes share one indirect dispatch args block.");

	// The link args are sized from the published live count, which is one thread per drawable bisector —
	// exactly what the compaction wants, so the two passes share the block.
	static_assert(CompactThreads == LinkThreads,
		"The link and compaction passes share one indirect dispatch args block.");

	TAutoConsoleVariable<int32> CVarValidate(
		TEXT("arc.Terrain.CBT.Validate"),
		0,
		TEXT("DIAGNOSTIC: >0 runs a GPU pass asserting that every halfedge reference is mutual (the ")
		TEXT("neighbour's reference at the named edge points back). Failures accumulate in the pool ")
		TEXT("counter buffer. Costs one dispatch per frame."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarStats(
		TEXT("arc.Terrain.CBT.Stats"),
		0,
		TEXT("DIAGNOSTIC: >0 tallies the vote histogram, the applied topology writes, the split/merge ")
		TEXT("deferral reasons, the cross-face diamond counters, the admission cascade and the live-leaf ")
		TEXT("depth range on the GPU, and logs them at ~1 Hz (CBTStats/A..G) together with the resolved ")
		TEXT("predicate inputs and view state. Classify 0 against a non-zero ")
		TEXT("live count means the dispatch args were empty; depth min == max means one level; votes ")
		TEXT("split N against applied split 0 means the deferral counters name the blocker. It also gates ")
		TEXT("the GPU tallying behind four of the stat arcterrain counters — drawn, culled, splits applied, ")
		TEXT("merges applied — which read 0 while it is off; live triangles and pool slots allocated are ")
		TEXT("written by load-bearing passes and need no cvar. stat arcterrain does NOT turn this on."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarProbePredicate(
		TEXT("arc.Terrain.CBT.ProbePredicate"),
		0,
		TEXT("DIAGNOSTIC: >0 selects the classify permutation that dumps, at ~1 Hz on CBTStats/P*, the ")
		TEXT("WHOLE predicate input set of four elected leaves — the largest triangle of the frame, the ")
		TEXT("nearest and farthest leaf still reading TOO_COARSE at the subdivision ceiling, and the ")
		TEXT("nearest leaf whose merge is refused by a TOO_COARSE parent, that one with its parent's whole ")
		TEXT("side too — plus the near-plane fallback tallies. Off, the probe is not in the shader at all. Needs ")
		TEXT("arc.Terrain.CBT.Stats for the counter readback to be logged."),
		ECVF_RenderThreadSafe);

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarTestPublishDelayFrames(
		TEXT("arc.Terrain.CBT.TestPublishDelayFrames"),
		0,
		TEXT("TEST ONLY: delay one CBT publish packet by this many renderer BeginFrame calls."),
		ECVF_RenderThreadSafe);
#endif

	TAutoConsoleVariable<int32> CVarGPUTiming(
		TEXT("arc.Terrain.CBT.GPUTiming"),
		0,
		TEXT("DIAGNOSTIC: unavailable while CBT topology uses the cross-frame async pipeline. Values above ")
		TEXT("zero emit one warning and do not insert a graphics-queue join."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarLocalHeightBounds(
		TEXT("arc.Terrain.CBT.LocalHeightBounds"),
		1,
		TEXT("EXPERIMENTAL: >0 lets tessellation and draw culling replace the actor-wide height prism ")
		TEXT("with an exact resident atlas-slice range when the complete footprint is proven local. ")
		TEXT("Ambiguous, cross-ring, invalid and live-composited footprints retain the actor-wide range."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarMergePhaseCount(
		TEXT("arc.Terrain.CBT.MergePhaseCount"),
		1,
		TEXT("EXPERIMENTAL: amortizes complete unanimous CBT merge diamonds across deterministic frame ")
		TEXT("phases. Requested values are clamped to 1..16 and snapped down to 1, 2, 4, 8 or 16. ")
		TEXT("1 preserves the unscheduled merge behavior; pool pressure temporarily overrides phasing."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarBoundedTopologyWork(
		TEXT("arc.Terrain.CBT.BoundedTopologyWork"),
		1,
		TEXT("EXPERIMENTAL: >0 replaces the full-live topology writer with finite merge-window selection ")
		TEXT("and sparse split/stage/commit dispatches. Enabled by default with finite motion-tested budgets."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarMergeScanBudget(
		TEXT("arc.Terrain.CBT.MergeScanBudget"),
		16384,
		TEXT("Normal per-frame merge ordinal scan budget for bounded topology work. Resolved on the render ")
		TEXT("thread to 1..PoolCapacity."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarMergePressureScanBudget(
		TEXT("arc.Terrain.CBT.MergePressureScanBudget"),
		16384,
		TEXT("Finite merge ordinal scan budget used under CBT pool pressure by bounded topology work. ")
		TEXT("Resolved to Normal..PoolCapacity; pressure never dispatches an unbounded merge scan."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarSplitApplyBudget(
		TEXT("arc.Terrain.CBT.SplitApplyBudget"),
		8192,
		TEXT("Maximum split root requests admitted to bounded topology work per frame. Resolved on the ")
		TEXT("render thread to 1..PoolCapacity; conformity members are limited separately by allocation."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarSplitAllocationBudget(
		TEXT("arc.Terrain.CBT.SplitAllocationBudget"),
		32768,
		TEXT("Maximum even number of child slots reserved by bounded split work per frame. Resolved on ")
		TEXT("the render thread to an even value in 2..PoolCapacity."),
		ECVF_RenderThreadSafe);

	constexpr uint32 SplitPriorityBucketCount = 6u;
	constexpr uint32 TopologyWorkCounterCount = 3u;
	constexpr uint32 TopologyWindowMetadataCount = 8u;
	constexpr uint32 TopologyIndirectArgsCount = 15u;

	uint32 ResolveMergePhaseCount()
	{
		const int32 Requested = FMath::Clamp(CVarMergePhaseCount.GetValueOnRenderThread(), 1, 16);
		if (Requested >= 16)
		{
			return 16u;
		}
		if (Requested >= 8)
		{
			return 8u;
		}
		if (Requested >= 4)
		{
			return 4u;
		}
		return Requested >= 2 ? 2u : 1u;
	}

	TAutoConsoleVariable<float> CVarMaxEdgePixelsOverride(
		TEXT("arc.Terrain.CBT.MaxEdgePixelsOverride"), 0.0f,
		TEXT("Maximum projected terrain-triangle edge in pixels. 0 uses the AArcTerrain value; values above ")
		TEXT("0 override it for budget sweeps."),
		ECVF_RenderThreadSafe);

	constexpr float DefaultMaxEdgePixels = 24.0f;

	float ResolveMaxEdgePixels(const FArcTerrainCBTBaseMesh& BaseMesh)
	{
		const float Override = CVarMaxEdgePixelsOverride.GetValueOnRenderThread();
		const float Requested = FMath::IsFinite(Override) && Override > 0.0f
			? Override
			: BaseMesh.MaxTriangleEdgePixels;
		return FMath::IsFinite(Requested) ? FMath::Max(Requested, 1.0f) : DefaultMaxEdgePixels;
	}

	TAutoConsoleVariable<int32> CVarLatencyLog(
		TEXT("arc.Terrain.CBT.LatencyLog"),
		0,
		TEXT("DIAGNOSTIC: unavailable while CBT topology uses the cross-frame async pipeline. Values above ")
		TEXT("zero emit one warning and do not insert a cross-graph join."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarMotionRefineIterations(
		TEXT("arc.Terrain.CBT.MotionRefineIterations"),
		0,
		TEXT("Number of extra CBT refinement rounds requested after fast submitted-view motion. Clamped to ")
		TEXT("0..4 on the render thread. 0 disables motion refinement."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarLatencyTurnDegrees(
		TEXT("arc.Terrain.CBT.LatencyTurnDegrees"), 5.0f,
		TEXT("Single submitted-view angular displacement in degrees that starts/extends a latency episode."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarLatencyTurnDegreesPerSecond(
		TEXT("arc.Terrain.CBT.LatencyTurnDegreesPerSecond"), 90.0f,
		TEXT("Submitted-view angular speed in degrees/second that starts/extends a latency episode."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarLatencyMoveCm(
		TEXT("arc.Terrain.CBT.LatencyMoveCm"), 1000.0f,
		TEXT("Single submitted-view displacement in cm treated as a teleport-sized latency trigger."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarLatencyMoveCmPerSecond(
		TEXT("arc.Terrain.CBT.LatencyMoveCmPerSecond"), 5000.0f,
		TEXT("Submitted-view linear speed in cm/second that starts/extends a latency episode."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarLatencyQuietFrames(
		TEXT("arc.Terrain.CBT.LatencyQuietFrames"), 2,
		TEXT("Truly consecutive source frames with zero visible splittable debt required for convergence."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarLatencyMaxFrames(
		TEXT("arc.Terrain.CBT.LatencyMaxFrames"), 60,
		TEXT("Maximum sampled source-frame span before a latency episode is reported as timed out."),
		ECVF_RenderThreadSafe);

	enum EArcTerrainCBTLatencyCounter : int32
	{
		LatencyCounter_VisibleSplittable = 0,
		LatencyCounter_CappedTier0 = 1,
		LatencyCounter_CappedTier1 = 2,
		LatencyCounter_CappedTier2 = 3,
		LatencyCounter_CappedTier3 = 4,
		LatencyCounter_WorstAreaRatioBits = 5,
		LatencyCounter_WorstErrorRatioBits = 6,
		LatencyCounter_AreaLevelEstimate = 7,
		LatencyCounter_SplitApplied = 8,
		LatencyCounter_SplitDeferred = 9,
		LatencyCounter_SplitRefused = 10,
		LatencyCounter_PoolFull = 11,
		LatencyCounter_LiveBisectors = 12,
		LatencyCounter_MotionRefineApplied = 13,
		LatencyCounter_MotionRefineDeferred = 14,
		LatencyCounter_MotionRefineRefused = 15,
		LatencyCounter_WorstEdgeRatioBits = 16,
		LatencyCounter_Num = 17,
	};

	constexpr uint8 LatencyCause_Turn = 1u << 0u;
	constexpr uint8 LatencyCause_Move = 1u << 1u;
	constexpr uint8 LatencyCause_Teleport = 1u << 2u;
	constexpr uint32 LatencyReadbackBytes = LatencyCounter_Num * sizeof(uint32);

	const TCHAR* LatencyCauseName(uint8 Cause)
	{
		if ((Cause & LatencyCause_Teleport) != 0u)
		{
			return (Cause & LatencyCause_Turn) != 0u ? TEXT("turn+teleport") : TEXT("teleport");
		}
		if ((Cause & (LatencyCause_Turn | LatencyCause_Move))
			== (LatencyCause_Turn | LatencyCause_Move))
		{
			return TEXT("turn+move");
		}
		return (Cause & LatencyCause_Turn) != 0u ? TEXT("turn") : TEXT("move");
	}

	float FloatFromBits(uint32 Bits)
	{
		float Value = 0.0f;
		FMemory::Memcpy(&Value, &Bits, sizeof(float));
		return Value;
	}

	/** Mirrors ARC_CBT_COUNTER_* in Shaders/ArcTerrainCBTCounters.ush, which owns the index space and
	 *  documents which slots survive the per-frame reset. */
	enum EArcTerrainCBTCounter : int32
	{
		Counter_AllocCursor = 0,
		Counter_AllocRefused = 1,
		Counter_NonMutualRefs = 2,
		Counter_LiveBisectors = 3,
		Counter_VoteSplit = 4,
		Counter_VoteMerge = 5,
		Counter_VoteKeep = 6,
		Counter_ClassifyThreads = 7,
		Counter_DepthMin = 8,
		Counter_DepthMax = 9,
		Counter_DepthSum = 10,
		Counter_SplitApplied = 11,
		Counter_MergeApplied = 12,
		Counter_SplitChained = 13,
		Counter_SplitTwinMerging = 14,
		Counter_SplitLostElection = 15,
		Counter_SplitNonMutual = 16,
		Counter_SplitMaxDepth = 17,
		Counter_MergeRoot = 18,
		Counter_MergeSiblingDeeper = 19,
		Counter_MergeSiblingMismatch = 20,
		Counter_MergeFarDeeper = 21,
		Counter_MergeFarSiblingDeeper = 22,
		Counter_MergeFarMismatch = 23,
		Counter_MergeNotUnanimous = 24,
		Counter_MergeLostElection = 25,
		Counter_SibMismatchSameFace = 26,
		Counter_SibMismatchDiffFace = 27,
		Counter_FarMismatchSameFace = 28,
		Counter_FarMismatchDiffFace = 29,
		Counter_SplitTwinSameFace = 30,
		Counter_SplitTwinDiffFace = 31,
		Counter_SplitDiffFaceDeferred = 32,
		Counter_SplitDiffFaceRefused = 33,
		Counter_FaceAllocRefused = 34,
		Counter_BoundaryLeaves = 35,
		Counter_BoundaryMismatch = 36,
		Counter_MergePartnerEqual = 37,
		Counter_MergePartnerOneDeeper = 38,
		Counter_MergePartnerTwoDeeper = 39,
		Counter_MergePartnerShallower = 40,
		Counter_MergeSiblingNoRef = 41,
		Counter_MergeFarSiblingNoRef = 42,
		Counter_LeafDeeperThanNeighbor = 43,
		Counter_LeafDeepestNeighbor = 44,
		Counter_LeafSiblingNoRef = 45,
		Counter_LeafSiblingNotKin = 46,
		Counter_SeedPopulated = 47,
		Counter_AttachPairs = 48,
		Counter_RefineSplits = 49,
		Counter_RefineDeferred = 50,
		Counter_RefineRefused = 51,
		Counter_SeedCursor = 52,
		Counter_SeedAllocRefused = 53,
		Counter_SplitPoolFull = 54,
		Counter_SMThreads = 55,
		Counter_SMSeenSplit = 56,
		Counter_SMSeenMerge = 57,
		Counter_SMSeenKeep = 58,
		Counter_SMSeenOther = 59,
		Counter_CmdEchoSplit = 60,
		Counter_CmdEchoMerge = 61,
		Counter_CmdEchoLost = 62,
		/** Four slots, indexed by the leaf's height tier. */
		Counter_MaxDepthCoarse = 63,
		Counter_MaxDepthByError = 67,
		Counter_MaxDepthByArea = 68,
		Counter_MaxDepthErrorPixelsX100 = 69,
		Counter_MaxDepthErrorCmMax = 70,
		Counter_MaxDepthDistMetersMin = 71,
		Counter_MaxDepthDistMetersMax = 72,
		Counter_MaxDepthAreaPixelsMax = 73,
		Counter_LeafTiersMixed = 74,
		Counter_ParentCoarseArea = 75,
		Counter_ParentCoarseError = 76,
		Counter_ParentKeep = 77,
		Counter_ParentFine = 78,
		Counter_SplitChainLen2 = 79,
		Counter_SplitChainLen3Plus = 80,
		Counter_SplitChainOverflow = 81,
		Counter_SplitChainBadFace = 82,
		Counter_FrustumCulled = 83,
		Counter_DrawnBisectors = 84,
		Counter_PrismCulledFine = 85,
		Counter_SplitTwinStaleMerge = 86,
		Counter_ClipWFallback = 87,
		Counter_CeilFallback = 88,
		Counter_CeilProjected = 89,
		Counter_MergeNonMutual = 90,
		Counter_SplitTwinMergeUnsure = 91,
		Counter_ProbeKeyA = 92,
		Counter_ProbeKeyB = 93,
		Counter_ProbeKeyC = 94,
		Counter_ProbeKeyD = 257,
		Counter_DistanceCulled = 337,
		Counter_CoarseByEdge = 338,
		Counter_CoarseByHeight = 339,
		Counter_CoarseByArea = 340,
		Counter_HeightUnknown = 341,
		Counter_HeightOverAtCap = 342,
		Counter_MaxEdgeBits = 343,
		Counter_MaxSampledErrorBits = 344,
		Counter_ParentCoarseEdge = 345,
		Counter_PrismGuardedSplit = 346,
		Counter_PrismGuardedKeep = 347,
		Counter_PrismGuardedFine = 348,
		Counter_HeightLocalCulled = 349,
		Counter_HeightLocalGuarded = 350,
		Counter_HeightFillCulled = 351,
		Counter_HeightFillGuarded = 352,
		Counter_HeightGlobalSpanCulled = 353,
		Counter_HeightGlobalSpanGuarded = 354,
		Counter_HeightGlobalUnsafeCulled = 355,
		Counter_HeightGlobalUnsafeGuarded = 356,
		Counter_MergePhaseCandidate = 357,
		Counter_MergePhaseSelected = 358,
		Counter_MergePhaseDeferred = 359,
		Counter_MergePhaseCount = 360,
		Counter_MergeFramePhase = 361,
		Counter_MergePressureOverride = 362,
		Counter_MergeFastResolved = 363,
		Counter_MergeFastFallback = 364,
		Counter_MergeFastProjectedSelected = 365,
		Counter_MergeFastProjectedDeferred = 366,
		Counter_MergeFastKeyMismatch = 367,
		Counter_TopologySplitAppended = 368,
		Counter_TopologySplitOverflow = 369,
		Counter_TopologyScanOrdinals = 370,
		Counter_TopologyFastRejects = 371,
		Counter_TopologyFullGathers = 372,
		Counter_TopologySelectedDiamonds = 373,
		Counter_TopologySelectedMembers = 374,
		Counter_TopologyCursorStart = 375,
		Counter_TopologyCursorEnd = 376,
		Counter_TopologyCursorWrapped = 377,
		Counter_TopologyFullWrapNoSelection = 378,
		Counter_TopologyNormalBudget = 379,
		Counter_TopologyPressureBudget = 380,
		Counter_TopologyActiveBudget = 381,
		Counter_TopologyPressureActive = 382,
		Counter_TopologySelectedOverflow = 383,
		Counter_TopologySplitConsumed = 384,
		Counter_TopologySplitLostEntry = 385,
		Counter_TopologySplitSelectedDeferred = 386,
		Counter_TopologySelectedNonLive = 387,
		Counter_TopologySelectedClaimed = 388,
		Counter_TopologySelectedMissingBit = 389,
		Counter_TopologyStagedDiamonds = 390,
		Counter_TopologyStagedMembers = 391,
		Counter_TopologyCommittedDiamonds = 392,
		Counter_TopologyCommittedMembers = 393,
		Counter_TopologyStaleCommit = 394,
		Counter_TopologySplitDuplicate = 395,
		Counter_TopologySplitCanonical = 396,
		Counter_TopologySplitAdmitted = 397,
		Counter_TopologySplitDeferred = 398,
		Counter_TopologySplitGuardedQuota = 399,
		Counter_TopologySplitGuardedAdmitted = 400,
		Counter_TopologySplitRawBucketBase = 401,
		Counter_TopologySplitCanonicalBucketBase = 407,
		Counter_TopologySplitAdmittedBucketBase = 413,
		Counter_TopologySplitPrepared = 419,
		Counter_TopologySplitPrepareFailed = 420,
		Counter_TopologySplitReserved = 421,
		Counter_TopologySplitBudgetDeferred = 422,
		Counter_TopologySplitPhysicalRefused = 423,
		Counter_TopologySplitAppliedDescriptor = 424,
		Counter_TopologySplitReleasedDescriptor = 425,
		Counter_TopologySplitAllocatedSlots = 426,
		Counter_TopologySplitMalformed = 427,
		Counter_TopologySplitHeldClaims = 428,
		Counter_ResidentTier1Count = 429,
		Counter_ResidentTier2Count = 430,
		Counter_ResidentTier1MaxDepth = 431,
		Counter_ResidentTier2MaxDepth = 432,
		Counter_ResidentCapMismatch = 433,
		Counter_FillCount = 434,
		Counter_FillAtOrAboveCap = 435,
		Counter_FillCapMismatch = 436,
		Counter_FillCeilingVoteViolation = 437,
	};

	/** Mirrors the ARC_CBT_PROBE_* record layout in Shaders/ArcTerrainCBTCounters.ush. */
	enum EArcTerrainCBTProbeField : int32
	{
		Probe_KeyFirst = 0,
		Probe_Face = 1,
		Probe_Depth = 2,
		Probe_NodeLo = 3,
		Probe_NodeHi = 4,
		Probe_Flags = 5,
		Probe_Verdict = 6,
		Probe_Corners = 7,
		Probe_Points = 13,
		Probe_ClipW = 22,
		Probe_MinClipW = 25,
		Probe_DistCm = 26,
		Probe_Area3DCm2 = 27,
		Probe_AreaPixels = 28,
		Probe_ErrorCm = 29,
		Probe_ErrorPixels = 30,
		Probe_Heights = 31,
		Probe_Deviation = 35,
		Probe_EffTarget = 38,
		Probe_EdgePixels = 39,
		Probe_LegacyErrorCm = 40,
		Probe_Visibility = 41,
		Probe_LeafTier = 42,
		Probe_LeafCap = 43,
		Probe_DiscoveryEdgePixels = 44,
		Probe_DiscoveryTargetPixels = 45,
		Probe_HeightSource = 46,
		Probe_HeightTier = 47,
		Probe_HeightSectorX = 48,
		Probe_HeightSectorY = 49,
		Probe_HeightSlice = 50,
		Probe_HeightMinCm = 51,
		Probe_HeightMaxCm = 52,
		Probe_KeyLast = 53,
		Probe_Stride = 54,

		/** Record D's tail, past the shared leaf body. */
		Probe_D_SagCmPerM2 = 54,
		Probe_D_TargetErrorPixels = 55,
		Probe_D_TargetEdgePixels = 56,
		Probe_D_ParentDepth = 57,
		Probe_D_ParentVerdict = 58,
		Probe_D_ParentFlags = 59,
		Probe_D_ParentVisibility = 60,
		Probe_D_ParentLeafTier = 61,
		Probe_D_ParentLeafCap = 62,
		Probe_D_ParentDistCm = 63,
		Probe_D_ParentAreaPixels = 64,
		Probe_D_ParentEffTarget = 65,
		Probe_D_ParentErrorCm = 66,
		Probe_D_ParentErrorPixels = 67,
		Probe_D_ParentEdgePixels = 68,
		Probe_D_ParentDiscoveryEdgePixels = 69,
		Probe_D_ParentDiscoveryTargetPixels = 70,
		Probe_D_ParentSagCmPerM2 = 71,
		Probe_D_ParentHeightSource = 72,
		Probe_D_ParentHeightTier = 73,
		Probe_D_ParentHeightSectorX = 74,
		Probe_D_ParentHeightSectorY = 75,
		Probe_D_ParentHeightSlice = 76,
		Probe_D_ParentHeightMinCm = 77,
		Probe_D_ParentHeightMaxCm = 78,
		Probe_D_Stride = 79,
	};

	constexpr int32 ProbeBaseA = 95;
	constexpr int32 ProbeBaseB = ProbeBaseA + Probe_Stride;
	constexpr int32 ProbeBaseC = ProbeBaseB + Probe_Stride;
	constexpr int32 ProbeBaseD = static_cast<int32>(Counter_ProbeKeyD) + 1;

	constexpr uint32 Probe_FlagFallback = 0x1u;
	constexpr uint32 Probe_FlagAtCeiling = 0x2u;
	constexpr uint32 Probe_FlagFillOnly = 0x4u;
	constexpr uint32 Probe_FlagResident0 = 0x8u;
	constexpr uint32 Probe_FlagResident1 = 0x10u;
	constexpr uint32 Probe_FlagResident2 = 0x20u;
	constexpr uint32 Probe_FlagResidentMid = 0x40u;
	constexpr uint32 Probe_FlagErrorMeasured = 0x80u;
	constexpr uint32 Probe_FlagTessCulled = 0x100u;
	constexpr uint32 Probe_FlagPrismGuarded = 0x200u;
	constexpr uint32 Probe_VisibilityOrdinary = 0u;
	constexpr uint32 Probe_VisibilityPrismCulled = 1u;
	constexpr uint32 Probe_VisibilityPrismGuarded = 2u;
	constexpr uint32 Probe_HeightSourceUnknown = 0u;
	constexpr uint32 Probe_HeightSourceLocal = 1u;
	constexpr uint32 Probe_HeightSourceFill = 2u;
	constexpr uint32 Probe_HeightSourceGlobalSpan = 3u;
	constexpr uint32 Probe_HeightSourceGlobalUnsafe = 4u;

	static_assert(static_cast<int32>(Counter_MaxDepthCoarse) + ArcTerrain::MaxTiers
			== static_cast<int32>(Counter_MaxDepthByError),
		"the diagnostic-tier ceiling-coarse block must be exactly MaxTiers wide; the shader walks it with a "
		"literal bound and would report a neighbouring counter as a tier column");

	static_assert(static_cast<int32>(Counter_MaxDepthDistMetersMax) + 1
			== static_cast<int32>(Counter_MaxDepthAreaPixelsMax),
		"the ceiling-coarse extrema block is not contiguous");

	static_assert(static_cast<int32>(Counter_ProbeKeyC) + 1 == ProbeBaseA,
		"the probe records must start right after the three election keys");

	static_assert(ProbeBaseC + static_cast<int32>(Probe_Stride) == static_cast<int32>(Counter_ProbeKeyD),
		"record D's election key must sit right after record C");

	// Catches the half of the mirror C++ can see; the .ush copy of the index space cannot be checked here.
	static_assert(ProbeBaseD + static_cast<int32>(Probe_D_Stride)
		== static_cast<int32>(Counter_DistanceCulled),
		"the distance-cull counter must sit immediately after the final probe record");
	static_assert(static_cast<int32>(Counter_FillCeilingVoteViolation) + 1
		== FArcTerrainCBTBuffers::PoolCounterNum,
		"ArcTerrainCBTCounters.ush and FArcTerrainCBTBuffers::PoolCounterNum disagree on the counter count");

	/** The record stores every float with asuint(); this is the read back. */
	float ProbeFloat(const uint32* Counters, int32 Base, int32 Field)
	{
		float Value = 0.0f;
		FMemory::Memcpy(&Value, &Counters[Base + Field], sizeof(float));
		return Value;
	}

	int32 ProbeInt(const uint32* Counters, int32 Base, int32 Field)
	{
		int32 Value = 0;
		FMemory::Memcpy(&Value, &Counters[Base + Field], sizeof(int32));
		return Value;
	}

	const TCHAR* ProbeVisibilityName(uint32 Visibility)
	{
		switch (Visibility)
		{
		case Probe_VisibilityOrdinary: return TEXT("ordinary");
		case Probe_VisibilityPrismCulled: return TEXT("prism-culled");
		case Probe_VisibilityPrismGuarded: return TEXT("prism-guarded");
		default: return TEXT("INVALID");
		}
	}

	const TCHAR* ProbeHeightSourceName(uint32 Source)
	{
		switch (Source)
		{
		case Probe_HeightSourceUnknown: return TEXT("not-tested");
		case Probe_HeightSourceLocal: return TEXT("local");
		case Probe_HeightSourceFill: return TEXT("fill");
		case Probe_HeightSourceGlobalSpan: return TEXT("global-span");
		case Probe_HeightSourceGlobalUnsafe: return TEXT("global-unsafe");
		default: return TEXT("INVALID");
		}
	}

	constexpr uint32 CounterReadbackBytes =
		static_cast<uint32>(FArcTerrainCBTBuffers::PoolCounterNum) * sizeof(uint32);

	/** Seconds between stats lines and between repeats of the no-main-view warning. */
	constexpr double StatsLogIntervalSeconds = 1.0;
	constexpr double NoMainViewWarnIntervalSeconds = 10.0;

	uint64 TimingPercentile(const TArray<uint64>& Samples, double Percentile)
	{
		if (Samples.IsEmpty())
		{
			return 0;
		}
		TArray<uint64> Sorted = Samples;
		Sorted.Sort();
		const int32 Index = FMath::Clamp(
			FMath::CeilToInt(Percentile * static_cast<double>(Sorted.Num())) - 1,
			0, Sorted.Num() - 1);
		return Sorted[Index];
	}

	template <typename TShaderClass>
	FRDGPassRef AddInlineTimedIndirectComputePass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& PassName,
		const TShaderRef<TShaderClass>& ComputeShader,
		typename TShaderClass::FParameters* Parameters,
		FRDGBufferRef IndirectArgsBuffer,
		uint32 IndirectArgsOffset,
		FRHIRenderQuery* BeginTimestampQuery,
		FRHIRenderQuery* EndTimestampQuery)
	{
		check(BeginTimestampQuery != nullptr && EndTimestampQuery != nullptr);
		checkf(IndirectArgsBuffer->Desc.Usage & BUF_DrawIndirect,
			TEXT("The buffer %s was not flagged for indirect draw parameters"), IndirectArgsBuffer->Name);
		FComputeShaderUtils::ValidateIndirectArgsBuffer(IndirectArgsBuffer, IndirectArgsOffset);
		ClearUnusedGraphResources(ComputeShader, Parameters, { IndirectArgsBuffer });

		return GraphBuilder.AddPass(
			Forward<FRDGEventName>(PassName), Parameters, ERDGPassFlags::Compute,
			[Parameters, ComputeShader, IndirectArgsBuffer, IndirectArgsOffset,
				BeginTimestampQuery, EndTimestampQuery](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
			{
				IndirectArgsBuffer->MarkResourceAsUsed();
				GDynamicRHI->RHIEndRenderQuery_TopOfPipe(RHICmdList, BeginTimestampQuery);
				FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *Parameters,
					IndirectArgsBuffer->GetIndirectRHICallBuffer(), IndirectArgsOffset);
				GDynamicRHI->RHIEndRenderQuery_TopOfPipe(RHICmdList, EndTimestampQuery);
			});
	}

	template <typename TShaderClass>
	FRDGPassRef AddInlineTimedComputePass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& PassName,
		const TShaderRef<TShaderClass>& ComputeShader,
		typename TShaderClass::FParameters* Parameters,
		FIntVector GroupCount,
		FRHIRenderQuery* BeginTimestampQuery,
		FRHIRenderQuery* EndTimestampQuery)
	{
		check(BeginTimestampQuery != nullptr && EndTimestampQuery != nullptr);
		ClearUnusedGraphResources(ComputeShader, Parameters);
		return GraphBuilder.AddPass(
			Forward<FRDGEventName>(PassName), Parameters, ERDGPassFlags::Compute,
			[Parameters, ComputeShader, GroupCount, BeginTimestampQuery, EndTimestampQuery]
			(FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
			{
				GDynamicRHI->RHIEndRenderQuery_TopOfPipe(RHICmdList, BeginTimestampQuery);
				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *Parameters, GroupCount);
				GDynamicRHI->RHIEndRenderQuery_TopOfPipe(RHICmdList, EndTimestampQuery);
			});
	}

	/** The first view of the family that a player is actually looking through. */
	const FSceneView* FindMainView(const FSceneViewFamily& ViewFamily)
	{
		for (const FSceneView* View : ViewFamily.Views)
		{
			if (View != nullptr && !View->bIsSceneCapture && !View->bIsSceneCaptureCube
				&& !View->bIsReflectionCapture && !View->bIsPlanarReflection && !View->bIsVirtualTexture)
			{
				return View;
			}
		}
		return nullptr;
	}
}

// ---------------------------------------------------------------------------
// Compute shaders
// ---------------------------------------------------------------------------

/** Per-bisector dispatch sizing from the published live count; also resets the allocation cursor. */
class FArcTerrainCBTDispatchArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCBTDispatchArgsCS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCBTDispatchArgsCS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, NodeCounter)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDispatchArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
		SHADER_PARAMETER(uint32, ThreadsPerGroup)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTDispatchArgsCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTDispatchArgs.usf", "FillDispatchArgsCS", SF_Compute);

/**
 * The tessellation predicate's whole binding surface — the C++ half of the contract in
 * ArcTerrainTessellationPredicate.ush. Replacing the metric means replacing that file, this struct and
 * FillTessPredicateParameters; no pass that owns topology has to be edited, because a pass only
 * INCLUDEs this block and never names a member of it.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainTessPredicateParameters, )
	SHADER_PARAMETER_TEXTURE(Texture2DArray<float>, HeightAtlas)
	SHADER_PARAMETER_SAMPLER(SamplerState, HeightSamplerBilinear)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, FaceTexelOrigins)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, HeightRing)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float2>, HeightSliceBounds)
	SHADER_PARAMETER(FUintVector4, HeightRingOffset)
	SHADER_PARAMETER(FIntVector4, HeightRingRadius)
	SHADER_PARAMETER(FIntVector4, HeightRingCenterX)
	SHADER_PARAMETER(FIntVector4, HeightRingCenterY)
	SHADER_PARAMETER(FIntVector4, HeightCumulativeShift)
	SHADER_PARAMETER(uint32, NumHeightTiers)
	SHADER_PARAMETER(uint32, HeightFillSlice)
	SHADER_PARAMETER(uint32, HeightSliceBoundsCount)
	SHADER_PARAMETER(uint32, LocalHeightBoundsEnabled)
	SHADER_PARAMETER(float, BaseHeightCm)
	SHADER_PARAMETER(FIntPoint, CameraTexel)
	SHADER_PARAMETER(FVector2f, CameraTexelFrac)
	SHADER_PARAMETER(uint32, FaceQuads)
	SHADER_PARAMETER(int32, SectorQuadsShift)
	SHADER_PARAMETER(float, InvTileResolution)
	SHADER_PARAMETER(float, TexelToLocalCm)
	SHADER_PARAMETER(float, ViewHeightLocalCm)
	SHADER_PARAMETER(FMatrix44f, EyeLocalToClip)
	SHADER_PARAMETER(FVector2f, ViewportSize)
	SHADER_PARAMETER(float, PixelsPerRadian)
	SHADER_PARAMETER(float, TargetTrianglePixels)
	SHADER_PARAMETER(float, MaxTriangleEdgePixels)
	SHADER_PARAMETER(float, MergeThresholdFraction)
	SHADER_PARAMETER(float, TargetErrorPixels)
	SHADER_PARAMETER(float, FlatAreaRelief)
	SHADER_PARAMETER(float, FlatReliefSagCmPerMeterSq)
	SHADER_PARAMETER(float, FrustumCullMargin)
	SHADER_PARAMETER(float, TessFrustumCullMargin)
	SHADER_PARAMETER(FVector2f, ConservativeHeightRangeCm)
	SHADER_PARAMETER(int32, ResidentCapDepth)
END_SHADER_PARAMETER_STRUCT()

/** One vote per live bisector against the frozen heap. */
class FArcTerrainCBTClassifyCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCBTClassifyCS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCBTClassifyCS, FGlobalShader);

public:
	/** arc.Terrain.CBT.ProbePredicate. A permutation, not a uniform: the probe writes a whole record per
	 *  elected leaf and the whole emitter has to be absent from the shader the frame it is off. */
	class FProbePredicateDim : SHADER_PERMUTATION_BOOL("ARC_CBT_PROBE_PREDICATE");
	class FBoundedTopologyWorkDim : SHADER_PERMUTATION_BOOL("ARC_CBT_BOUNDED_TOPOLOGY_WORK");
	using FPermutationDomain = TShaderPermutationDomain<FProbePredicateDim, FBoundedTopologyWorkDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorNeighbors)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorCommands)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RawSplitRequests)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, SplitPriorityBySlot)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkCounters)
		SHADER_PARAMETER(uint32, PoolDepth)
		SHADER_PARAMETER(uint32, PoolCapacity)
		SHADER_PARAMETER(uint32, MaxSubdivisionDepth)
		SHADER_PARAMETER(uint32, StatsEnabled)
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcTerrainTessPredicateParameters, TessPredicate)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ARC_CBT_CLASSIFY_THREADS"), ArcTerrainCBTRendererPrivate_9a07::ClassifyThreads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTClassifyCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTClassify.usf", "ClassifyCS", SF_Compute);

namespace ArcTerrainCBTRendererPrivate_9a07
{
	/** The two buffers the predicate binds. Created ONCE per frame and shared by every pass that includes
	 *  the block: the seed frame fills 3 + 4 * MaxSubdivisionDepth parameter structs, and creating and
	 *  uploading the face origins for each of them is the whole per-face grid uploaded ~100 times. */
	struct FArcTerrainTessPredicateResources
	{
		FRDGBufferSRVRef FaceTexelOrigins = nullptr;
		FRDGBufferSRVRef HeightRing = nullptr;
		FRDGBufferSRVRef HeightSliceBounds = nullptr;
		ArcTerrainAtlas::FArcTerrainHeightRingScalars HeightRingScalars;
		uint32 HeightSliceBoundsCount = 0u;
	};

	struct FArcTerrainDistanceCullParameters
	{
		bool bRequested = false;
		double RequestedRadiusCm = 0.0;
		uint32 bEnabled = 0u;
		float RadiusSq = 0.0f;
		FVector3f Metric = FVector3f::ZeroVector;
		FIntPoint CameraTexel = FIntPoint::ZeroValue;
		FVector2f CameraTexelFrac = FVector2f::ZeroVector;
	};

	float RoundNonNegativeDoubleUpToFloat(double Value)
	{
		float Rounded = static_cast<float>(Value);
		if (Value > 0.0 && static_cast<double>(Rounded) < Value)
		{
			uint32 Bits = 0u;
			FMemory::Memcpy(&Bits, &Rounded, sizeof(float));
			++Bits;
			FMemory::Memcpy(&Rounded, &Bits, sizeof(float));
		}
		return Rounded;
	}

	FArcTerrainDistanceCullParameters ResolveDistanceCullParameters(
		const FArcTerrainCBTBaseMesh& BaseMesh, const FArcTerrainCBTMainView& MainView)
	{
		FArcTerrainDistanceCullParameters Parameters;
		Parameters.bRequested = BaseMesh.bEnableDistanceCulling;
		Parameters.RequestedRadiusCm = BaseMesh.DistanceCullRadius;
		if (!BaseMesh.bEnableDistanceCulling || !FMath::IsFinite(BaseMesh.DistanceCullRadius)
			|| BaseMesh.DistanceCullRadius < 0.0)
		{
			return Parameters;
		}

		const double RadiusSqDouble = BaseMesh.DistanceCullRadius * BaseMesh.DistanceCullRadius;
		if (!FMath::IsFinite(RadiusSqDouble)
			|| RadiusSqDouble > static_cast<double>(TNumericLimits<float>::Max()))
		{
			return Parameters;
		}

		const FVector WorldX = BaseMesh.LocalToWorld.TransformVector(FVector::XAxisVector);
		const FVector WorldY = BaseMesh.LocalToWorld.TransformVector(FVector::YAxisVector);
		const double Mxx = FVector::DotProduct(WorldX, WorldX);
		const double Mxy = FVector::DotProduct(WorldX, WorldY);
		const double Myy = FVector::DotProduct(WorldY, WorldY);
		const double Determinant = Mxx * Myy - Mxy * Mxy;
		if (!FMath::IsFinite(Mxx) || !FMath::IsFinite(Mxy) || !FMath::IsFinite(Myy)
			|| !FMath::IsFinite(Determinant) || Mxx <= 0.0 || Myy <= 0.0 || Determinant <= 0.0)
		{
			return Parameters;
		}

		const FVector3f Metric(static_cast<float>(Mxx), static_cast<float>(Myy), static_cast<float>(Mxy));
		const double FloatMetricDeterminant = static_cast<double>(Metric.X) * static_cast<double>(Metric.Y)
			- static_cast<double>(Metric.Z) * static_cast<double>(Metric.Z);
		if (!FMath::IsFinite(Metric.X) || !FMath::IsFinite(Metric.Y) || !FMath::IsFinite(Metric.Z)
			|| !FMath::IsFinite(FloatMetricDeterminant) || Metric.X <= 0.0f || Metric.Y <= 0.0f
			|| FloatMetricDeterminant <= 0.0)
		{
			return Parameters;
		}

		const double TexelToLocalCm = static_cast<double>(BaseMesh.TexelToLocalCm);
		if (!FMath::IsFinite(TexelToLocalCm) || TexelToLocalCm <= 0.0)
		{
			return Parameters;
		}
		const FVector CullViewOrigin = MainView.bFrozen ? MainView.FrozenViewOrigin : MainView.ViewOrigin;
		const FVector CullViewOriginLocal = BaseMesh.LocalToWorld.Inverse().TransformPosition(CullViewOrigin);
		const double CameraTexelX = FMath::Floor(CullViewOriginLocal.X / TexelToLocalCm);
		const double CameraTexelY = FMath::Floor(CullViewOriginLocal.Y / TexelToLocalCm);
		if (!FMath::IsFinite(CullViewOriginLocal.X) || !FMath::IsFinite(CullViewOriginLocal.Y)
			|| !FMath::IsFinite(CameraTexelX) || !FMath::IsFinite(CameraTexelY)
			|| CameraTexelX < static_cast<double>(MIN_int32)
			|| CameraTexelX > static_cast<double>(MAX_int32)
			|| CameraTexelY < static_cast<double>(MIN_int32)
			|| CameraTexelY > static_cast<double>(MAX_int32))
		{
			return Parameters;
		}

		const double CameraFracX = CullViewOriginLocal.X / TexelToLocalCm - CameraTexelX;
		const double CameraFracY = CullViewOriginLocal.Y / TexelToLocalCm - CameraTexelY;
		if (!FMath::IsFinite(CameraFracX) || !FMath::IsFinite(CameraFracY))
		{
			return Parameters;
		}

		Parameters.bEnabled = 1u;
		Parameters.RadiusSq = RoundNonNegativeDoubleUpToFloat(RadiusSqDouble);
		Parameters.Metric = Metric;
		Parameters.CameraTexel = FIntPoint(static_cast<int32>(CameraTexelX), static_cast<int32>(CameraTexelY));
		Parameters.CameraTexelFrac =
			FVector2f(static_cast<float>(CameraFracX), static_cast<float>(CameraFracY));
		return Parameters;
	}

	FArcTerrainTessPredicateResources CreateTessPredicateResources(FRDGBuilder& GraphBuilder,
		const FArcTerrainCBTBaseMesh& BaseMesh)
	{
		FArcTerrainTessPredicateResources Resources;

		FRDGBufferRef FaceOriginsRDG = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(FIntPoint), FMath::Max(BaseMesh.FaceTexelOrigins.Num(), 1)),
			TEXT("ArcTerrainCBT.FaceTexelOrigins"));
		GraphBuilder.QueueBufferUpload(FaceOriginsRDG, MakeArrayView(BaseMesh.FaceTexelOrigins));

		// The ring is residency-cadence DATA uploaded on the frame's graph; it changes only when the
		// atlas admits or evicts, and at MaxResidentSectors entries the upload is a few hundred bytes. A
		// residency empty on EVERY tier still needs a one-element buffer to bind — a zero-length buffer
		// desc is an RDG check — but every tier publishes a negative radius, so the range test answers
		// every lookup with the fill slice and nothing reads it.
		TArray<uint32> RingSlices;
		ArcTerrainAtlas::PackHeightRings(BaseMesh.HeightRings, RingSlices, Resources.HeightRingScalars);
		if (RingSlices.IsEmpty())
		{
			RingSlices.Add(0u);
		}
		FRDGBufferRef HeightRingRDG = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), RingSlices.Num()),
			TEXT("ArcTerrainCBT.HeightRing"));
		GraphBuilder.QueueBufferUpload(HeightRingRDG, MakeArrayView(RingSlices));

		Resources.HeightSliceBoundsCount = static_cast<uint32>(
			BaseMesh.AtlasResources->HeightSliceBounds.Num());
		TArray<FVector2f> HeightSliceBounds = BaseMesh.AtlasResources->HeightSliceBounds;
		if (HeightSliceBounds.IsEmpty())
		{
			HeightSliceBounds.Add(ArcTerrainTileAtlasRender::InvalidHeightRange());
		}
		FRDGBufferRef HeightSliceBoundsRDG = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(FVector2f), HeightSliceBounds.Num()),
			TEXT("ArcTerrainCBT.HeightSliceBounds"));
		GraphBuilder.QueueBufferUpload(HeightSliceBoundsRDG, MakeArrayView(HeightSliceBounds));

		Resources.FaceTexelOrigins =
			GraphBuilder.CreateSRV(FRDGBufferSRVDesc(FaceOriginsRDG, PF_R32G32_UINT));
		Resources.HeightRing = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(HeightRingRDG, PF_R32_UINT));
		Resources.HeightSliceBounds =
			GraphBuilder.CreateSRV(FRDGBufferSRVDesc(HeightSliceBoundsRDG, PF_G32R32F));
		return Resources;
	}

	/**
	 * Everything the metric needs, resolved into the eye-relative terrain-local frame it works in. The
	 * caller supplies a base mesh and a main view and learns nothing else about how a triangle is scored.
	 * The struct is filled per PASS even though the resources are shared, because
	 * ClearUnusedGraphResources nulls the caller's struct in place.
	 */
	enum class EClipSource : uint8
	{
		Live,
		Cull
	};

	void FillTessPredicateParameters(const FArcTerrainCBTBaseMesh& BaseMesh,
		const FArcTerrainCBTMainView& MainView, const FArcTerrainTessPredicateResources& Resources,
		EClipSource ClipSource, FArcTerrainTessPredicateParameters& OutParameters)
	{
		const FMatrix WorldToLocal = BaseMesh.LocalToWorld.Inverse();
		const FVector ViewOriginLocal = WorldToLocal.TransformPosition(MainView.ViewOrigin);

		// The camera enters the metric as an INTEGER texel index plus a sub-texel remainder, both resolved
		// in double here. The shader then measures an edge as a difference of integers, so two corners
		// 100 cm apart stay 100 cm apart 2000 km from the terrain origin — the float form quantized both
		// endpoints to 32 cm and made the difference flicker frame to frame, oscillating split against merge.
		const double TexelToLocalCm = static_cast<double>(BaseMesh.TexelToLocalCm);
		const double CameraTexelX = FMath::Floor(ViewOriginLocal.X / TexelToLocalCm);
		const double CameraTexelY = FMath::Floor(ViewOriginLocal.Y / TexelToLocalCm);

		OutParameters.HeightAtlas = BaseMesh.AtlasResources->HeightAtlas->GetRHI();
		OutParameters.HeightSamplerBilinear =
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		OutParameters.FaceTexelOrigins = Resources.FaceTexelOrigins;
		OutParameters.HeightRing = Resources.HeightRing;
		OutParameters.HeightSliceBounds = Resources.HeightSliceBounds;
		OutParameters.HeightRingOffset = Resources.HeightRingScalars.Offset;
		OutParameters.HeightRingRadius = Resources.HeightRingScalars.Radius;
		OutParameters.HeightRingCenterX = Resources.HeightRingScalars.CenterX;
		OutParameters.HeightRingCenterY = Resources.HeightRingScalars.CenterY;
		OutParameters.HeightCumulativeShift = Resources.HeightRingScalars.CumulativeShift;
		OutParameters.NumHeightTiers = Resources.HeightRingScalars.NumTiers;
		OutParameters.HeightFillSlice = Resources.HeightRingScalars.FillSlice;
		OutParameters.HeightSliceBoundsCount = Resources.HeightSliceBoundsCount;
		OutParameters.LocalHeightBoundsEnabled =
			CVarLocalHeightBounds.GetValueOnRenderThread() > 0 ? 1u : 0u;
		OutParameters.BaseHeightCm = BaseMesh.AtlasResources->Layout.BaseHeightCm;
		OutParameters.CameraTexel = FIntPoint(
			static_cast<int32>(FMath::Clamp(CameraTexelX, -2147483648.0, 2147483647.0)),
			static_cast<int32>(FMath::Clamp(CameraTexelY, -2147483648.0, 2147483647.0)));
		OutParameters.CameraTexelFrac = FVector2f(
			static_cast<float>(ViewOriginLocal.X / TexelToLocalCm - CameraTexelX),
			static_cast<float>(ViewOriginLocal.Y / TexelToLocalCm - CameraTexelY));
		OutParameters.FaceQuads = static_cast<uint32>(BaseMesh.FaceQuads);
		OutParameters.SectorQuadsShift = BaseMesh.SectorQuadsShift;
		OutParameters.InvTileResolution = 1.0f / static_cast<float>(BaseMesh.SectorQuads + 3);
		OutParameters.TexelToLocalCm = BaseMesh.TexelToLocalCm;
		OutParameters.ViewHeightLocalCm = static_cast<float>(ViewOriginLocal.Z);
		// Composed in double and handed over as float: every factor carries the terrain's absolute extent,
		// and only the product — which maps an EYE-RELATIVE local position — has a small translation row.
		// PreViewTranslation and TranslatedWorldToClip are one pair and are taken from one source; the
		// eye-relative anchor stays live in both cases, because the corner positions the shader feeds this
		// matrix are built from CameraTexel and ViewHeightLocalCm, which are the live view origin.
		const bool bFrozenClip = ClipSource == EClipSource::Cull && MainView.bFrozen;
		const FVector ClipPreViewTranslation =
			bFrozenClip ? MainView.FrozenPreViewTranslation : MainView.PreViewTranslation;
		const FMatrix& ClipTranslatedWorldToClip =
			bFrozenClip ? MainView.FrozenTranslatedWorldToClip : MainView.TranslatedWorldToClip;
		const FMatrix EyeLocalToClip = FTranslationMatrix(ViewOriginLocal) * BaseMesh.LocalToWorld
			* FTranslationMatrix(ClipPreViewTranslation) * ClipTranslatedWorldToClip;
		OutParameters.EyeLocalToClip = FMatrix44f(EyeLocalToClip);
		OutParameters.ViewportSize = MainView.ViewportSize;
		OutParameters.PixelsPerRadian = MainView.PixelsPerRadian;
		OutParameters.TargetTrianglePixels = FMath::Max(BaseMesh.TargetTrianglePixels, 1.0f);
		OutParameters.MaxTriangleEdgePixels = ResolveMaxEdgePixels(BaseMesh);
		OutParameters.MergeThresholdFraction = FMath::Clamp(BaseMesh.MergeThresholdFraction, 0.05f, 0.9f);
		OutParameters.TargetErrorPixels = FMath::Max(BaseMesh.TargetErrorPixels, 1.0f);
		OutParameters.FlatAreaRelief = FMath::Clamp(BaseMesh.FlatAreaRelief, 1.0f, 4096.0f);
		OutParameters.FlatReliefSagCmPerMeterSq =
			FMath::Clamp(BaseMesh.FlatReliefSagCmPerMeterSq, 0.0001f, 1000.0f);
		// The draw's margin (compaction, ArcTerrain_FrustumCullTriangle) and the verdict's
		// (ArcTerrain_ClassifyTriangleProbe) are separate knobs on one struct: both passes bind both, and
		// each function names the one it belongs to.
		OutParameters.FrustumCullMargin = FMath::Clamp(BaseMesh.FrustumCullMargin, 0.0f, 4.0f);
		OutParameters.TessFrustumCullMargin = FMath::Clamp(BaseMesh.TessFrustumCullMargin, 0.0f, 4.0f);
		OutParameters.ConservativeHeightRangeCm = ArcTerrain::ResolveConservativeHeightRangeCm(
			BaseMesh.ConservativeMinHeightCm, BaseMesh.ConservativeMaxHeightCm);
		OutParameters.ResidentCapDepth = ArcTerrain::ResolveMaxSubdivisionDepth(BaseMesh.FaceQuads);
	}
}

/** The topology write pass' binding surface, shared by its two entry points. */
BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTSplitMergeParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CBTHeap)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorNeighbors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorCommands)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorPromote)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, LatencyCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FaceLiveCount)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FaceRoots)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, NumFaces)
	SHADER_PARAMETER(uint32, MaxSubdivisionDepth)
	SHADER_PARAMETER(uint32, MergePhaseCount)
	SHADER_PARAMETER(uint32, MergeFramePhase)
	SHADER_PARAMETER(uint32, StatsEnabled)
	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTMergeWindowArgsParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WindowMetadata)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkIndirectArgs)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, MergeScanCursor)
	SHADER_PARAMETER(uint32, NormalScanBudget)
	SHADER_PARAMETER(uint32, PressureScanBudget)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, StatsEnabled)

END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTMergeSelectParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorNeighbors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorCommands)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint4>, SelectedDiamonds)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WindowMetadata)
	SHADER_PARAMETER(uint32, SelectedCapacity)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, StatsEnabled)
	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTSplitRawArgsParameters, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkIndirectArgs)
	SHADER_PARAMETER(uint32, PoolCapacity)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTSplitAdmissionParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorNeighbors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorCommands)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RawSplitRequests)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, SplitPriorityBySlot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, SplitBucketCandidates)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, SplitBucketCounts)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkCounters)
	SHADER_PARAMETER(uint32, SplitApplyBudget)
	SHADER_PARAMETER(uint32, PriorityBucket)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, StatsEnabled)
	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTSplitFinalizeArgsParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, SplitBucketCandidates)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, SplitBucketCounts)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, AdmittedSplitRequests)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WindowMetadata)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkIndirectArgs)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, MergeScanCursor)
	SHADER_PARAMETER(uint32, SelectedCapacity)
	SHADER_PARAMETER(uint32, SplitApplyBudget)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, StatsEnabled)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTSplitPrepareParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorNeighbors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorCommands)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, LatencyCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, AdmittedSplitRequests)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, SplitPriorityBySlot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint4>, PreparedSplitDescriptors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkCounters)
	SHADER_PARAMETER(uint32, SplitApplyBudget)
	SHADER_PARAMETER(uint32, PriorityBucket)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, NumFaces)
	SHADER_PARAMETER(uint32, MaxSubdivisionDepth)
	SHADER_PARAMETER(uint32, MergePhaseCount)
	SHADER_PARAMETER(uint32, MergeFramePhase)
	SHADER_PARAMETER(uint32, StatsEnabled)
	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTSplitReserveParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint4>, PreparedSplitDescriptors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkCounters)
	SHADER_PARAMETER(uint32, SplitApplyBudget)
	SHADER_PARAMETER(uint32, SplitAllocationBudget)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, MaxSubdivisionDepth)
	SHADER_PARAMETER(uint32, StatsEnabled)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTSplitApplyParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CBTHeap)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorNeighbors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorCommands)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, LatencyCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FaceLiveCount)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint4>, PreparedSplitDescriptors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkCounters)
	SHADER_PARAMETER(uint32, SplitApplyBudget)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, NumFaces)
	SHADER_PARAMETER(uint32, MaxSubdivisionDepth)
	SHADER_PARAMETER(uint32, MergePhaseCount)
	SHADER_PARAMETER(uint32, MergeFramePhase)
	SHADER_PARAMETER(uint32, StatsEnabled)
	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTSplitClaimAuditParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CBTHeap)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTMergeStageParameters, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CBTHeap)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorCommands)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorPromote)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FaceLiveCount)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint4>, SelectedDiamonds)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkCounters)
	SHADER_PARAMETER(uint32, SelectedCapacity)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, NumFaces)
	SHADER_PARAMETER(uint32, StatsEnabled)
	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTMergeCommitSparseParameters, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorNeighbors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorPromote)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FaceRoots)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint4>, SelectedDiamonds)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, WorkCounters)
	SHADER_PARAMETER(uint32, SelectedCapacity)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, StatsEnabled)
	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

#define ARC_TERRAIN_DECLARE_SPLITMERGE_CS(ClassName)                                                   \
	class ClassName : public FGlobalShader                                                             \
	{                                                                                                  \
		DECLARE_GLOBAL_SHADER(ClassName);                                                              \
		SHADER_USE_PARAMETER_STRUCT(ClassName, FGlobalShader);                                         \
	public:                                                                                            \
		class FLatencyAuditDim : SHADER_PERMUTATION_BOOL("ARC_CBT_LATENCY_AUDIT");                    \
		using FPermutationDomain = TShaderPermutationDomain<FLatencyAuditDim>;                          \
		using FParameters = FArcTerrainCBTSplitMergeParameters;                                        \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)     \
		{                                                                                              \
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);                \
		}                                                                                              \
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, \
			FShaderCompilerEnvironment& OutEnvironment)                                                \
		{                                                                                              \
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);                    \
			OutEnvironment.SetDefine(TEXT("ARC_CBT_SM_THREADS"),                                       \
				ArcTerrainCBTRendererPrivate_9a07::SplitMergeThreads);                                 \
		}                                                                                              \
	};

/** Decides and stages the frame's topology. */
ARC_TERRAIN_DECLARE_SPLITMERGE_CS(FArcTerrainCBTSplitMergeCS)
/** Rebuilds the parent record of every slot a merge kept. */
ARC_TERRAIN_DECLARE_SPLITMERGE_CS(FArcTerrainCBTMergeCommitCS)

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTSplitMergeCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTSplitMerge.usf", "SplitMergeCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTMergeCommitCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTSplitMerge.usf", "MergeCommitCS", SF_Compute);

#define ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS(ClassName, ParameterType, EntryName)                     \
	class ClassName : public FGlobalShader                                                             \
	{                                                                                                  \
		DECLARE_GLOBAL_SHADER(ClassName);                                                              \
		SHADER_USE_PARAMETER_STRUCT(ClassName, FGlobalShader);                                         \
	public:                                                                                            \
		using FParameters = ParameterType;                                                             \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)     \
		{                                                                                              \
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);                \
		}                                                                                              \
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, \
			FShaderCompilerEnvironment& OutEnvironment)                                                \
		{                                                                                              \
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);                    \
			OutEnvironment.SetDefine(TEXT("ARC_CBT_TOPOLOGY_THREADS"),                               \
				ArcTerrainCBTRendererPrivate_9a07::SplitMergeThreads);                                 \
		}                                                                                              \
	};                                                                                                 \
	IMPLEMENT_GLOBAL_SHADER(ClassName, "/Plugin/ArcTerrain/ArcTerrainCBTTopologyWork.usf", EntryName, \
		SF_Compute);

ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS(FArcTerrainCBTMergeWindowArgsCS,
	FArcTerrainCBTMergeWindowArgsParameters, "MergeWindowArgsCS")
ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS(FArcTerrainCBTMergeSelectCS,
	FArcTerrainCBTMergeSelectParameters, "MergeSelectCS")
ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS(FArcTerrainCBTSplitRawArgsCS,
	FArcTerrainCBTSplitRawArgsParameters, "SplitRawArgsCS")
ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS(FArcTerrainCBTSplitAdmissionCS,
	FArcTerrainCBTSplitAdmissionParameters, "SplitAdmissionCS")
ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS(FArcTerrainCBTSplitFinalizeArgsCS,
	FArcTerrainCBTSplitFinalizeArgsParameters, "SplitFinalizeArgsCS")
ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS(FArcTerrainCBTSplitReserveCS,
	FArcTerrainCBTSplitReserveParameters, "SplitReserveCS")
ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS(FArcTerrainCBTSplitClaimAuditCS,
	FArcTerrainCBTSplitClaimAuditParameters, "SplitClaimAuditCS")
ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS(FArcTerrainCBTMergeStageCS,
	FArcTerrainCBTMergeStageParameters, "MergeStageCS")
ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS(FArcTerrainCBTMergeCommitSparseCS,
	FArcTerrainCBTMergeCommitSparseParameters, "MergeCommitSparseCS")

class FArcTerrainCBTSplitPrepareCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCBTSplitPrepareCS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCBTSplitPrepareCS, FGlobalShader);

public:
	class FLatencyAuditDim : SHADER_PERMUTATION_BOOL("ARC_CBT_LATENCY_AUDIT");
	using FPermutationDomain = TShaderPermutationDomain<FLatencyAuditDim>;
	using FParameters = FArcTerrainCBTSplitPrepareParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ARC_CBT_TOPOLOGY_THREADS"),
			ArcTerrainCBTRendererPrivate_9a07::SplitMergeThreads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTSplitPrepareCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTTopologyWork.usf", "SplitPrepareCS", SF_Compute);

class FArcTerrainCBTSplitApplyCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCBTSplitApplyCS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCBTSplitApplyCS, FGlobalShader);

public:
	class FLatencyAuditDim : SHADER_PERMUTATION_BOOL("ARC_CBT_LATENCY_AUDIT");
	using FPermutationDomain = TShaderPermutationDomain<FLatencyAuditDim>;
	using FParameters = FArcTerrainCBTSplitApplyParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ARC_CBT_TOPOLOGY_THREADS"),
			ArcTerrainCBTRendererPrivate_9a07::SplitMergeThreads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTSplitApplyCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTTopologyWork.usf", "SplitApplyCS", SF_Compute);

#undef ARC_TERRAIN_DECLARE_TOPOLOGY_WORK_CS

/**
 * The base mesh passes' whole binding surface, shared by their four entry points. It carries the
 * tessellation predicate too: the mesh is brought to the metric's triangulation inside the same frame the
 * refine loop first runs, rather than resolving over as many frames as it has levels.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTBaseMeshParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapSnapshot)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CBTHeap)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorNeighbors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorCommands)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorPromote)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, LatencyCounters)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FaceLiveCount)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FaceRoots)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, FaceNeighbors)
	SHADER_PARAMETER(uint32, PoolDepth)
	SHADER_PARAMETER(uint32, PoolCapacity)
	SHADER_PARAMETER(uint32, NumFaces)
	SHADER_PARAMETER(uint32, MaxSubdivisionDepth)
	SHADER_PARAMETER(uint32, MergePhaseCount)
	SHADER_PARAMETER(uint32, MergeFramePhase)
	SHADER_PARAMETER(uint32, StatsEnabled)
	SHADER_PARAMETER_STRUCT_INCLUDE(FArcTerrainTessPredicateParameters, TessPredicate)
END_SHADER_PARAMETER_STRUCT()

#define ARC_TERRAIN_DECLARE_BASEMESH_CS(ClassName, EntryName)                                          \
	class ClassName : public FGlobalShader                                                             \
	{                                                                                                  \
		DECLARE_GLOBAL_SHADER(ClassName);                                                              \
		SHADER_USE_PARAMETER_STRUCT(ClassName, FGlobalShader);                                         \
	public:                                                                                            \
		class FLatencyAuditDim : SHADER_PERMUTATION_BOOL("ARC_CBT_LATENCY_AUDIT");                    \
		using FPermutationDomain = TShaderPermutationDomain<FLatencyAuditDim>;                          \
		using FParameters = FArcTerrainCBTBaseMeshParameters;                                          \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)     \
		{                                                                                              \
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);                \
		}                                                                                              \
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, \
			FShaderCompilerEnvironment& OutEnvironment)                                                \
		{                                                                                              \
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);                    \
			OutEnvironment.SetDefine(TEXT("ARC_CBT_FACE_THREADS"),                                     \
				ArcTerrainCBTRendererPrivate_9a07::FaceThreads);                                       \
			OutEnvironment.SetDefine(TEXT("ARC_CBT_POOL_THREADS"),                                     \
				ArcTerrainCBTRendererPrivate_9a07::PoolThreads);                                       \
		}                                                                                              \
	};                                                                                                 \
	IMPLEMENT_GLOBAL_SHADER(ClassName, "/Plugin/ArcTerrain/ArcTerrainCBTBaseMesh.usf", EntryName, SF_Compute);

ARC_TERRAIN_DECLARE_BASEMESH_CS(FArcTerrainCBTSeedRootsCS, "SeedRootsCS")
ARC_TERRAIN_DECLARE_BASEMESH_CS(FArcTerrainCBTAttachCS, "AttachCS")
ARC_TERRAIN_DECLARE_BASEMESH_CS(FArcTerrainCBTRefineVoteCS, "RefineVoteCS")
ARC_TERRAIN_DECLARE_BASEMESH_CS(FArcTerrainCBTRefineApplyCS, "RefineApplyCS")

#undef ARC_TERRAIN_DECLARE_BASEMESH_CS

/** Sum-reduction of the occupancy heap, one dispatch per level. */
class FArcTerrainCBTReduceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCBTReduceCS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCBTReduceCS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CBTHeap)
		SHADER_PARAMETER(uint32, PoolDepth)
		SHADER_PARAMETER(uint32, Level)
		SHADER_PARAMETER(uint32, ThreadStride)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ARC_CBT_REDUCE_THREADS"), ArcTerrainCBTRendererPrivate_9a07::ReduceThreads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTReduceCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTReduce.usf", "ReduceCS", SF_Compute);

/** Publish the reduced live count to NodeCounter, DrawArgs and the link dispatch args. */
class FArcTerrainCBTBatchCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCBTBatchCS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCBTBatchCS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CBTHeap)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, NodeCounter)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDrawArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWLinkDispatchArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
		SHADER_PARAMETER(uint32, PoolDepth)
		SHADER_PARAMETER(uint32, IndexCount)
		SHADER_PARAMETER(uint32, ThreadsPerGroup)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTBatchCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTBatch.usf", "BatchCS", SF_Compute);

/** Build the drawn list for ONE view by dropping live bisectors outside its planar radius or frustum. */
class FArcTerrainCBTCompactCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCBTCompactCS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCBTCompactCS, FGlobalShader);

public:
	class FLatencyAuditDim : SHADER_PERMUTATION_BOOL("ARC_CBT_LATENCY_AUDIT");
	using FPermutationDomain = TShaderPermutationDomain<FLatencyAuditDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapRead)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, BisectorPoolRead)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, RWVisibleBisectors)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDrawArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, LatencyCounters)
		SHADER_PARAMETER(uint32, PoolDepth)
		SHADER_PARAMETER(uint32, NumFaces)
		SHADER_PARAMETER(uint32, StatsEnabled)
		SHADER_PARAMETER(uint32, DistanceCullEnabled)
		SHADER_PARAMETER(float, DistanceCullRadiusSq)
		SHADER_PARAMETER(FVector3f, DistanceCullMetric)
		SHADER_PARAMETER(FIntPoint, DistanceCullCameraTexel)
		SHADER_PARAMETER(FVector2f, DistanceCullCameraTexelFrac)
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcTerrainTessPredicateParameters, TessPredicate)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ARC_CBT_COMPACT_THREADS"),
			ArcTerrainCBTRendererPrivate_9a07::CompactThreads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTCompactCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTCompact.usf", "CompactCS", SF_Compute);

/** Halfedge fixup and its optional mutual-reference assertion. */
BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCBTLinkParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CBTHeapRead)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorPool)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BisectorNeighbors)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BisectorPromote)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
	SHADER_PARAMETER(uint32, PoolDepth)
	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

class FArcTerrainCBTLinkCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCBTLinkCS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCBTLinkCS, FGlobalShader);

public:
	using FParameters = FArcTerrainCBTLinkParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ARC_CBT_LINK_THREADS"), ArcTerrainCBTRendererPrivate_9a07::LinkThreads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTLinkCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTLink.usf", "LinkCS", SF_Compute);

class FArcTerrainCBTValidateCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCBTValidateCS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCBTValidateCS, FGlobalShader);

public:
	using FParameters = FArcTerrainCBTLinkParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ARC_CBT_LINK_THREADS"), ArcTerrainCBTRendererPrivate_9a07::LinkThreads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTValidateCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTLink.usf", "ValidateCS", SF_Compute);

// ---------------------------------------------------------------------------
// FArcTerrainCBTRendererExtension
// ---------------------------------------------------------------------------

FArcTerrainCBTRendererExtension::FTopologyWorkBudgets
FArcTerrainCBTRendererExtension::ResolveTopologyWorkBudgets(uint32 PoolCapacity)
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;

	check(PoolCapacity >= 2u && PoolCapacity <= static_cast<uint32>(MAX_int32));
	const int32 Capacity = static_cast<int32>(PoolCapacity);
	const uint32 NormalBudget = static_cast<uint32>(FMath::Clamp(
		CVarMergeScanBudget.GetValueOnRenderThread(), 1, Capacity));
	const uint32 PressureBudget = static_cast<uint32>(FMath::Clamp(
		CVarMergePressureScanBudget.GetValueOnRenderThread(), static_cast<int32>(NormalBudget), Capacity));

	FTopologyWorkBudgets Result;
	Result.NormalScanBudget = NormalBudget;
	Result.PressureScanBudget = PressureBudget;
	Result.SelectedCapacity = FMath::Max(NormalBudget, PressureBudget);
	Result.SplitApplyBudget = static_cast<uint32>(FMath::Clamp(
		CVarSplitApplyBudget.GetValueOnRenderThread(), 1, Capacity));
	const int32 EvenAllocationCapacity = Capacity & ~1;
	Result.SplitAllocationBudget = static_cast<uint32>(FMath::Clamp(
		CVarSplitAllocationBudget.GetValueOnRenderThread(), 2, EvenAllocationCapacity) & ~1);
	return Result;
}

FArcTerrainCBTRendererExtension::FTopologyWorkResources
FArcTerrainCBTRendererExtension::CreateTopologyWorkResources(FRDGBuilder& GraphBuilder,
	uint32 PoolCapacity, const FTopologyWorkBudgets& Budgets, ERDGPassFlags ComputePassFlags) const
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;

	check(PoolCapacity >= 1u && PoolCapacity <= static_cast<uint32>(MAX_int32));
	check(Budgets.NormalScanBudget >= 1u && Budgets.NormalScanBudget <= PoolCapacity);
	check(Budgets.PressureScanBudget >= Budgets.NormalScanBudget
		&& Budgets.PressureScanBudget <= PoolCapacity);
	check(Budgets.SelectedCapacity == FMath::Max(
		Budgets.NormalScanBudget, Budgets.PressureScanBudget));
	check(Budgets.SplitApplyBudget >= 1u && Budgets.SplitApplyBudget <= PoolCapacity);
	check(Budgets.SplitAllocationBudget >= 2u && Budgets.SplitAllocationBudget <= PoolCapacity
		&& (Budgets.SplitAllocationBudget & 1u) == 0u);

	FTopologyWorkResources Resources;
	Resources.RawSplitRequests = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), static_cast<int32>(PoolCapacity)),
		TEXT("ArcTerrainCBT.RawSplitRequests"));
	Resources.SplitPriorityBySlot = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), static_cast<int32>(PoolCapacity)),
		TEXT("ArcTerrainCBT.SplitPriorityBySlot"));
	Resources.SplitBucketCandidates = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32),
			static_cast<int32>(SplitPriorityBucketCount * Budgets.SplitApplyBudget)),
		TEXT("ArcTerrainCBT.SplitBucketCandidates"));
	Resources.SplitBucketCounts = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), SplitPriorityBucketCount),
		TEXT("ArcTerrainCBT.SplitBucketCounts"));
	Resources.AdmittedSplitRequests = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), static_cast<int32>(Budgets.SplitApplyBudget)),
		TEXT("ArcTerrainCBT.AdmittedSplitRequests"));
	Resources.PreparedSplitDescriptors = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4u, Budgets.SplitApplyBudget),
		TEXT("ArcTerrainCBT.PreparedSplitDescriptors"));
	Resources.SelectedDiamonds = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4u, Budgets.SelectedCapacity),
		TEXT("ArcTerrainCBT.SelectedDiamonds"));
	Resources.WorkCounters = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), TopologyWorkCounterCount),
		TEXT("ArcTerrainCBT.TopologyWorkCounters"));
	Resources.WindowMetadata = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), TopologyWindowMetadataCount),
		TEXT("ArcTerrainCBT.MergeWindowMetadata"));
	Resources.IndirectArgs = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc(TopologyIndirectArgsCount),
		TEXT("ArcTerrainCBT.TopologyWorkArgs"));
	Resources.MergeScanCursor = Buffers.RegisterMergeScanCursor(GraphBuilder);
	AddClearUAVPass(GraphBuilder,
		GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Resources.SplitPriorityBySlot, PF_R32_UINT)), 0u,
		ComputePassFlags);
	AddClearUAVPass(GraphBuilder,
		GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Resources.SplitBucketCounts, PF_R32_UINT)), 0u,
		ComputePassFlags);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Resources.PreparedSplitDescriptors), 0xFFFFFFFFu,
		ComputePassFlags);
	AddClearUAVPass(GraphBuilder,
		GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Resources.WorkCounters, PF_R32_UINT)), 0u,
		ComputePassFlags);
	AddClearUAVPass(GraphBuilder,
		GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Resources.WindowMetadata, PF_R32_UINT)), 0u,
		ComputePassFlags);
	AddClearUAVPass(GraphBuilder,
		GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Resources.IndirectArgs, PF_R32_UINT)), 0u,
		ComputePassFlags);
	return Resources;
}

TGlobalResource<FArcTerrainCBTRendererExtension> GArcTerrainCBTRendererExtension;

void FArcTerrainCBTRendererExtension::RegisterExtension()
{
	if (bRegistered)
	{
		return;
	}
	bRegistered = true;
	PreRenderDelegateHandle =
		GEngine->GetPreRenderDelegateEx().AddRaw(this, &FArcTerrainCBTRendererExtension::BeginFrame);
	PostRenderDelegateHandle =
		GEngine->GetPostRenderDelegateEx().AddRaw(this, &FArcTerrainCBTRendererExtension::EndFrame);
	PostOpaqueRenderDelegateHandle = FModuleManager::GetModuleChecked<IRendererModule>(TEXT("Renderer"))
		.RegisterPostOpaqueRenderDelegate(
		FPostOpaqueRenderDelegate::CreateRaw(this, &FArcTerrainCBTRendererExtension::PostOpaque_RenderThread));
	ViewExtension = FSceneViewExtensions::NewExtension<FArcTerrainCBTViewExtension>();
}

void FArcTerrainCBTRendererExtension::UnregisterExtension()
{
	check(IsInRenderingThread());
	UE::TScopeLock Lock(Mutex);
	if (!bRegistered)
	{
		return;
	}

	if (GEngine != nullptr)
	{
		GEngine->GetPreRenderDelegateEx().Remove(PreRenderDelegateHandle);
		GEngine->GetPostRenderDelegateEx().Remove(PostRenderDelegateHandle);
	}
	if (PostOpaqueRenderDelegateHandle.IsValid())
	{
		FModuleManager::GetModuleChecked<IRendererModule>(TEXT("Renderer"))
			.RemovePostOpaqueRenderDelegate(PostOpaqueRenderDelegateHandle);
	}
	PreRenderDelegateHandle.Reset();
	PostRenderDelegateHandle.Reset();
	PostOpaqueRenderDelegateHandle.Reset();
	ViewExtension.Reset();
	ArmedTopologyUpdate = FArmedTopologyUpdate();
	PendingTopologyIdentity = FArcTerrainRenderIdentity();
	PendingTopologySourceFrame = MAX_uint32;
	bTopologyUpdatePending = false;
	bRebindUpdatePending = false;
	bRegistered = false;
	bInFrame = false;
}

void FArcTerrainCBTViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder&, FSceneViewFamily& InViewFamily)
{
	// A hit-proxy pass is a SECOND FSceneRenderer over the same camera, and it broadcasts the same
	// pre-render delegate the pool advances from (SceneHitProxyRendering.cpp:673). Refusing it here keeps
	// it from taking the frame's one submission: its own gather binds nothing, and the viewport's gather
	// would then find the frame already spent and draw the whole live set for as long as the mouse tests.
	if (InViewFamily.EngineShowFlags.HitProxies)
	{
		return;
	}

	const FSceneView* const View = ArcTerrainCBTRendererPrivate_9a07::FindMainView(InViewFamily);
	if (View == nullptr)
	{
		return;
	}

	FArcTerrainCBTMainView MainView;
	MainView.SceneKey = InViewFamily.Scene;
	MainView.ViewKey = View;
	MainView.CaptureFrame = GFrameNumberRenderThread;
	MainView.ViewOrigin = View->ViewMatrices.GetViewOrigin();
	MainView.ViewDirection = View->GetViewDirection().GetSafeNormal();
	MainView.ViewportSize = FVector2f(
		static_cast<float>(View->UnscaledViewRect.Width()), static_cast<float>(View->UnscaledViewRect.Height()));

	// A projected offset of one radian at the view centre spans ViewportHeight * 0.5 * M[1][1] pixels,
	// and M[1][1] is 1 / tan(HalfFOVY) for every perspective projection. An orthographic view carries a
	// world-unit scale there instead, which no angular metric can read — it leaves bValid false and the
	// tree holds the triangulation the last perspective view asked for.
	const float ProjYScale = static_cast<float>(View->ViewMatrices.GetViewToClip().M[1][1]);
	MainView.PixelsPerRadian = 0.5f * MainView.ViewportSize.Y * ProjYScale;
	MainView.TranslatedWorldToClip = View->ViewMatrices.GetTranslatedWorldToClip();
	MainView.PreViewTranslation = View->ViewMatrices.GetPreViewTranslation();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// FreezeRendering swaps the frozen matrices ONTO the view only for the visibility phase
	// (SceneVisibility.cpp Activate/Restore around the gather), and this extension runs ahead of that
	// window — so View->ViewMatrices above is the live camera and the snapshot is still in the view state.
	const FViewMatrices* const FrozenMatrices =
		View->State != nullptr ? View->State->GetFrozenViewMatrices() : nullptr;
	if (FrozenMatrices != nullptr)
	{
		MainView.FrozenTranslatedWorldToClip = FrozenMatrices->GetTranslatedWorldToClip();
		MainView.FrozenPreViewTranslation = FrozenMatrices->GetPreViewTranslation();
		MainView.FrozenViewOrigin = FrozenMatrices->GetViewOrigin();
		MainView.bFrozen = true;
	}
#endif

	MainView.bValid = MainView.ViewportSize.X > 0.0f && MainView.ViewportSize.Y > 0.0f
		&& MainView.PixelsPerRadian > 0.0f && View->ViewMatrices.IsPerspectiveProjection();

	// A family whose only view is orthographic never validates one, and the tree then holds the two
	// depth-1 triangles per face the base mesh passes root every frame — coarse, never empty.
	static bool bLoggedFirstMainView = false;
	if (MainView.bValid && !bLoggedFirstMainView)
	{
		bLoggedFirstMainView = true;
		UE_LOGF(LogArcTerrainCBT, Log,"Main view captured: rect %dx%d — the bisector tree now follows the camera.",
			View->UnscaledViewRect.Width(), View->UnscaledViewRect.Height());
	}

	GArcTerrainCBTRendererExtension.SetMainView_RenderThread(MoveTemp(MainView));
}

void FArcTerrainCBTRendererExtension::SetMainView_RenderThread(FArcTerrainCBTMainView&& View)
{
	if (!AcceptedIdentity.IsValid() || View.SceneKey != AcceptedIdentity.SceneKey)
	{
		return;
	}

	// Entitlement is announced even by a family whose view no angular metric can read (an orthographic
	// editor viewport), or the pool would never root its faces there.
	EligibleFamilyFrame = GFrameNumberRenderThread;

	if (View.bValid)
	{
		MainView = MoveTemp(View);
	}
}

bool FArcTerrainCBTRendererExtension::IsCulledView_RenderThread(const FSceneView& View)
{
	// The shadow gather runs on a task (ShadowSetup.cpp:5593, ETaskTag::EParallelRenderingThread) and sets
	// this frustum around every call it makes, so answering it before any member is read is what keeps the
	// view state a render-thread-only structure. The thread test covers the rest of that class: off the
	// render thread the answer is always no, and no is always safe — it draws the whole live set.
	if (View.GetDynamicMeshElementsShadowCullFrustum() != nullptr || !IsInRenderingThread())
	{
		return false;
	}

	// BeginFrame compacts against this family's main view. A gather after the frame's post-opaque topology
	// submission would otherwise claim a list culled for an earlier family, so it is refused here and draws
	// the whole live set. Together with the capture frame this makes the gather's answer and the pass's input
	// the same main view rather than two reads of a sticky field.
	//
	// The pointer alone would admit a view that reused a freed address, so the origin the compaction runs
	// against has to agree too.
	// The gather runs INSIDE FreezeRendering's Activate/Restore window, so a frozen view presents its
	// snapshot origin here while the capture, which runs ahead of that window, holds the live one.
	const FVector GatherViewOrigin = View.ViewMatrices.GetViewOrigin();

	const bool bCulledView = MainView.bValid
		&& MainView.CaptureFrame == GFrameNumberRenderThread
		&& LastSubmittedFrame != GFrameNumberRenderThread
		&& MainView.ViewKey == &View
		&& (GatherViewOrigin == MainView.ViewOrigin
			|| (MainView.bFrozen && GatherViewOrigin == MainView.FrozenViewOrigin));

	if (bCulledView)
	{
		LastCulledBindFrame = GFrameNumberRenderThread;
		if (ArcTerrainCBTRendererPrivate_9a07::CVarLatencyLog.GetValueOnRenderThread() > 0)
		{
			for (FLatencyReadbackSlot& Slot : LatencyReadbacks)
			{
				if (Slot.bPending && Slot.Metadata.SourceFrame == GFrameNumberRenderThread)
				{
					Slot.Metadata.bListBoundObserved = true;
				}
			}
		}
	}
	return bCulledView;
}

bool FArcTerrainCBTRendererExtension::IsDrawPacketAccepted_RenderThread(
	const FArcTerrainRenderIdentity& Identity, const FSceneInterface* Scene) const
{
	return IsInParallelRenderingThread() && AcceptedIdentity.IsValid() && Identity == AcceptedIdentity
		&& Scene == AcceptedIdentity.SceneKey && ActiveBaseMesh.Identity == AcceptedIdentity
		&& Buffers.IsInitialized();
}

bool FArcTerrainCBTRendererExtension::RegisterBindingSource_RenderThread(
	const FArcTerrainRenderIdentity& Identity, const FArcTerrainCBTBindingSourceToken& SourceToken)
{
	check(IsInRenderingThread());
	if (!SourceToken.IsValid() || !IsIdentityLive_RenderThread(Identity))
	{
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] source register rejected proxy=%llu slot=%u token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p reason=invalid-source-or-identity",
			SourceToken.ProxySerial, static_cast<uint32>(SourceToken.SlotIndex),
			Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
			Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
			static_cast<int32>(Identity.WorldType), Identity.SceneKey);
		return false;
	}

	bool bSourceRegistered = false;
	const TFunction<void(TOptional<FCandidateState>&, const TCHAR*)> RegisterWithCandidate =
		[&Identity, &SourceToken, &bSourceRegistered](
		TOptional<FCandidateState>& Candidate, const TCHAR* Lane)
	{
		if (Candidate.IsSet() && Candidate->BaseMesh.Identity == Identity)
		{
			const bool bAlreadyRegistered = Candidate->LiveBindingSources.Contains(SourceToken);
			Candidate->LiveBindingSources.Add(SourceToken);
			bSourceRegistered = true;
			UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] source register proxy=%llu slot=%u lane=%ls action=%ls sources=%d ready=yes token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
				SourceToken.ProxySerial, static_cast<uint32>(SourceToken.SlotIndex), Lane,
				bAlreadyRegistered ? TEXT("already-registered") : TEXT("added"),
				Candidate->LiveBindingSources.Num(),
				Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
				Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
				static_cast<int32>(Identity.WorldType), Identity.SceneKey);
		}
	};
	RegisterWithCandidate(RuntimeCandidate, TEXT("runtime"));
	RegisterWithCandidate(ParkedEditorCandidate, TEXT("parked-editor"));
#if !UE_BUILD_SHIPPING
	RegisterWithCandidate(DelayedTestCandidate, TEXT("delayed-test"));
#endif
	if (!bSourceRegistered && Identity == AcceptedIdentity)
	{
		UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] source register no-op proxy=%llu slot=%u reason=already-accepted-recreate token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			SourceToken.ProxySerial, static_cast<uint32>(SourceToken.SlotIndex),
			Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
			Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
			static_cast<int32>(Identity.WorldType), Identity.SceneKey);
		return false;
	}
	if (!bSourceRegistered)
	{
		ensureMsgf(false,
			TEXT("[CBTOwner] non-accepted binding source has no exact candidate: proxy=%llu slot=%u token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p"),
			SourceToken.ProxySerial, static_cast<uint32>(SourceToken.SlotIndex),
			Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
			Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
			static_cast<int32>(Identity.WorldType), Identity.SceneKey);
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] source register rejected proxy=%llu slot=%u reason=no-exact-candidate token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			SourceToken.ProxySerial, static_cast<uint32>(SourceToken.SlotIndex),
			Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
			Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
			static_cast<int32>(Identity.WorldType), Identity.SceneKey);
	}
	return bSourceRegistered;
}

void FArcTerrainCBTRendererExtension::UnregisterBindingSource_RenderThread(
	const FArcTerrainRenderIdentity& Identity, const FArcTerrainCBTBindingSourceToken& SourceToken)
{
	check(IsInRenderingThread());
	if (!Identity.IsValid() || !SourceToken.IsValid())
	{
		return;
	}

	bool bSourceRemoved = false;
	const TFunction<void(TOptional<FCandidateState>&, const TCHAR*)> UnregisterFromCandidate =
		[&Identity, &SourceToken, &bSourceRemoved](
		TOptional<FCandidateState>& Candidate, const TCHAR* Lane)
	{
		if (Candidate.IsSet() && Candidate->BaseMesh.Identity == Identity)
		{
			const int32 RemovedCount = Candidate->LiveBindingSources.Remove(SourceToken);
			bSourceRemoved |= RemovedCount > 0;
			UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] source unregister proxy=%llu slot=%u lane=%ls action=%ls sources=%d ready=%ls token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
				SourceToken.ProxySerial, static_cast<uint32>(SourceToken.SlotIndex), Lane,
				RemovedCount > 0 ? TEXT("removed") : TEXT("not-present"),
				Candidate->LiveBindingSources.Num(), Candidate->IsReady() ? TEXT("yes") : TEXT("no"),
				Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
				Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
				static_cast<int32>(Identity.WorldType), Identity.SceneKey);
		}
	};
	UnregisterFromCandidate(RuntimeCandidate, TEXT("runtime"));
	UnregisterFromCandidate(ParkedEditorCandidate, TEXT("parked-editor"));
#if !UE_BUILD_SHIPPING
	UnregisterFromCandidate(DelayedTestCandidate, TEXT("delayed-test"));
#endif
	if (!bSourceRemoved)
	{
		UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] source unregister no-op proxy=%llu slot=%u accepted=%ls token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			SourceToken.ProxySerial, static_cast<uint32>(SourceToken.SlotIndex),
			Identity == AcceptedIdentity ? TEXT("yes") : TEXT("no"),
			Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
			Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
			static_cast<int32>(Identity.WorldType), Identity.SceneKey);
	}
}

void FArcTerrainCBTRendererExtension::RecordProxyDiagnostic_RenderThread(bool bAccepted) const
{
	if (bAccepted)
	{
		ProxyAcceptedCount.fetch_add(1, std::memory_order_relaxed);
	}
	else
	{
		ProxyRejectedCount.fetch_add(1, std::memory_order_relaxed);
	}
}

void FArcTerrainCBTRendererExtension::GetProxyDiagnosticCounts_RenderThread(
	uint64& OutAccepted, uint64& OutRejected) const
{
	OutAccepted = ProxyAcceptedCount.load(std::memory_order_relaxed);
	OutRejected = ProxyRejectedCount.load(std::memory_order_relaxed);
}

void FArcTerrainCBTRendererExtension::ReleaseRHI()
{
	UE::TScopeLock Lock(Mutex);
	ReleaseActivePool();
	CounterReadback.Reset();
	RetiredCounterReadbacks.Reset();
	ResetCandidateRegistry();
	RhiEpoch.fetch_add(1, std::memory_order_acq_rel);
	for (FLatencyReadbackSlot& Slot : LatencyReadbacks)
	{
		Slot.Readback.Reset();
		Slot.Metadata = FLatencySampleMetadata();
		Slot.bPending = false;
	}
	RetiredLatencyReadbacks.Reset();
	bLatencyRetirementBlocked = false;
	for (FGPUTimingSlot& Slot : GPUTimingSlots)
	{
		for (FRHIPooledRenderQuery& Query : Slot.Queries)
		{
			Query.ReleaseQuery();
		}
		for (FRHIPooledRenderQuery& Query : Slot.InlineQueries)
		{
			Query.ReleaseQuery();
		}
		Slot.CounterReadback.Reset();
		Slot.bPending = false;
		Slot.BoundaryCount = 0;
		Slot.bCounterReadbackExpected = false;
		Slot.bInlineDispatchTiming = false;
	}
	GPUTimingQueryPool.SafeRelease();
}

void FArcTerrainCBTRendererExtension::ReleaseActivePool()
{
	if (AcceptedIdentity.IsValid())
	{
		UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] release active token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			AcceptedIdentity.Owner.Key.WorldInstanceToken, AcceptedIdentity.Owner.Key.TerrainInstanceId,
			AcceptedIdentity.Owner.OwnerEpoch, AcceptedIdentity.ResourceRevision, AcceptedIdentity.RhiEpoch,
			static_cast<int32>(AcceptedIdentity.WorldType), AcceptedIdentity.SceneKey);
	}
	InvalidateGPUTimingGeneration();
	Buffers.Release();
	ActiveBaseMesh = FArcTerrainCBTBaseMesh();
	AcceptedIdentity = FArcTerrainRenderIdentity();
	bPoolInitPending = false;
	bRefinePending = false;
	LastSubmittedFrame = MAX_uint32;
	BoundTerrainId = 0;
	if (bCounterReadbackPending && CounterReadback.IsValid())
	{
		RetiredCounterReadbacks.Add(MoveTemp(CounterReadback));
	}
	else
	{
		CounterReadback.Reset();
	}
	bCounterReadbackPending = false;
	CounterStats = FCounterStats();
	PendingCounterDistanceCull = FDistanceCullStatsState();
	NextLatencyReadbackSlot = 0;
	PendingLatencyGapRanges.Reset();
	++LatencyLoggerGeneration;
	DrainLatencyReadbacks(GFrameNumberRenderThread, false);
	bLatencyLoggerWasEnabled = false;
	LastSubmittedMotionView = FArcTerrainCBTMainView();
	LastSubmittedMotionTime = 0.0;
	bHasLastSubmittedMotionView = false;
	bLatencyMotionArmed = false;
	LatencyEpisode = FLatencyEpisode();
	ResetOwnerDependentState();
}

void FArcTerrainCBTRendererExtension::ResetCandidateRegistry()
{
	RuntimeCandidate.Reset();
	ParkedEditorCandidate.Reset();
#if !UE_BUILD_SHIPPING
	DelayedTestCandidate.Reset();
	DelayedTestCandidateFrames = 0;
#endif
}

void FArcTerrainCBTRendererExtension::ResetOwnerDependentState()
{
	MainView = FArcTerrainCBTMainView();
	ArmedTopologyUpdate = FArmedTopologyUpdate();
	PendingTopologyIdentity = FArcTerrainRenderIdentity();
	PendingTopologySourceFrame = MAX_uint32;
	bTopologyUpdatePending = false;
	bRebindUpdatePending = false;
	LastSubmittedMotionView = FArcTerrainCBTMainView();
	LastSubmittedMotionTime = 0.0;
	bHasLastSubmittedMotionView = false;
	EligibleFamilyFrame = MAX_uint32;
	LastSubmittedFrame = MAX_uint32;
	LastCulledBindFrame = MAX_uint32;
	LastEyeLocalToClip = FMatrix44f::Identity;
	ProxyAcceptedCount.store(0, std::memory_order_relaxed);
	ProxyRejectedCount.store(0, std::memory_order_relaxed);
	bLoggedFirstSubmit = false;
}

void FArcTerrainCBTRendererExtension::ResetGPUTimingAggregate()
{
	GPUTimingAggregate = FGPUTimingAggregate();
	GPUTimingDroppedSamples = 0;
}

void FArcTerrainCBTRendererExtension::InvalidateGPUTimingGeneration()
{
	++GPUTimingGeneration;
	ResetGPUTimingAggregate();
}

void FArcTerrainCBTRendererExtension::UpdateGPUTimingMode()
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;

	const uint8 SanitizedMode = static_cast<uint8>(FMath::Clamp(
		CVarGPUTiming.GetValueOnRenderThread(), 0, 2));
	if (SanitizedMode != GPUTimingMode)
	{
		InvalidateGPUTimingGeneration();
		GPUTimingMode = SanitizedMode;
	}
}

int32 FArcTerrainCBTRendererExtension::AcquireGPUTimingSlot(uint32 SourceFrame, uint8 Mode,
	uint8 BoundaryCount, bool bShaderStats, bool bBulkRefine, uint8 MotionRefineIterations,
	bool bInlineDispatchTiming)
{
	if (Mode == 0)
	{
		return INDEX_NONE;
	}
	check(!bInlineDispatchTiming || Mode == 2);
	if (!GSupportsTimestampRenderQueries)
	{
		if (!bWarnedGPUTimingUnsupported)
		{
			bWarnedGPUTimingUnsupported = true;
			UE_LOGF(LogArcTerrainCBT, Warning,
				"arc.Terrain.CBT.GPUTiming is unavailable: this RHI does not support absolute "
				"timestamp render queries. Timing stays disabled; no blocking fallback is used.");
		}
		return INDEX_NONE;
	}

	if (!GPUTimingQueryPool.IsValid())
	{
		GPUTimingQueryPool = RHICreateRenderQueryPool(RQT_AbsoluteTime);
		if (!GPUTimingQueryPool.IsValid())
		{
			if (!bWarnedGPUTimingUnsupported)
			{
				bWarnedGPUTimingUnsupported = true;
				UE_LOGF(LogArcTerrainCBT, Warning,
					"arc.Terrain.CBT.GPUTiming could not create an absolute timestamp query pool. "
					"Timing stays disabled; no blocking fallback is used.");
			}
			return INDEX_NONE;
		}
	}

	int32 SlotIndex = INDEX_NONE;
	for (int32 Offset = 0; Offset < GPUTimingRingSize; ++Offset)
	{
		const int32 Candidate = (NextGPUTimingSlot + Offset) % GPUTimingRingSize;
		if (!GPUTimingSlots[Candidate].bPending)
		{
			SlotIndex = Candidate;
			NextGPUTimingSlot = (Candidate + 1) % GPUTimingRingSize;
			break;
		}
	}
	if (SlotIndex == INDEX_NONE)
	{
		++GPUTimingDroppedSamples;
		return INDEX_NONE;
	}

	FGPUTimingSlot& Slot = GPUTimingSlots[SlotIndex];
	check(BoundaryCount > 0 && BoundaryCount <= GPUTimingMode2BoundaryCount);
	for (int32 BoundaryIndex = 0; BoundaryIndex < BoundaryCount; ++BoundaryIndex)
	{
		Slot.Queries[BoundaryIndex] = GPUTimingQueryPool->AllocateQuery();
		if (Slot.Queries[BoundaryIndex].GetQuery() == nullptr)
		{
			for (int32 ReleaseIndex = 0; ReleaseIndex <= BoundaryIndex; ++ReleaseIndex)
			{
				Slot.Queries[ReleaseIndex].ReleaseQuery();
			}
			++GPUTimingDroppedSamples;
			return INDEX_NONE;
		}
	}
	const int32 InlineQueryCount = bInlineDispatchTiming ? GPUTimingInlineQueryCount : 0;
	for (int32 QueryIndex = 0; QueryIndex < InlineQueryCount; ++QueryIndex)
	{
		Slot.InlineQueries[QueryIndex] = GPUTimingQueryPool->AllocateQuery();
		if (Slot.InlineQueries[QueryIndex].GetQuery() == nullptr)
		{
			for (int32 ReleaseIndex = 0; ReleaseIndex < BoundaryCount; ++ReleaseIndex)
			{
				Slot.Queries[ReleaseIndex].ReleaseQuery();
			}
			for (int32 ReleaseIndex = 0; ReleaseIndex <= QueryIndex; ++ReleaseIndex)
			{
				Slot.InlineQueries[ReleaseIndex].ReleaseQuery();
			}
			++GPUTimingDroppedSamples;
			return INDEX_NONE;
		}
	}

	Slot.SourceFrame = SourceFrame;
	Slot.TerrainId = BoundTerrainId;
	Slot.Generation = GPUTimingGeneration;
	Slot.Mode = Mode;
	Slot.BoundaryCount = BoundaryCount;
	Slot.MotionRefineIterations = MotionRefineIterations;
	Slot.bShaderStats = bShaderStats;
	Slot.bBulkRefine = bBulkRefine;
	Slot.bCounterReadbackExpected = Mode == 2 && bShaderStats;
	Slot.bInlineDispatchTiming = bInlineDispatchTiming;
	if (Slot.bCounterReadbackExpected && !Slot.CounterReadback.IsValid())
	{
		Slot.CounterReadback = MakeUnique<FRHIGPUBufferReadback>(
			*FString::Printf(TEXT("ArcTerrainCBT.GPUTimingCounters%d"), SlotIndex));
	}
	Slot.bPending = true;
	return SlotIndex;
}

void FArcTerrainCBTRendererExtension::AddGPUTimingBoundary(FRDGBuilder& GraphBuilder,
	int32 SlotIndex, int32 BoundaryIndex, const TCHAR* BoundaryName)
{
	if (SlotIndex == INDEX_NONE)
	{
		return;
	}
	check(SlotIndex >= 0 && SlotIndex < GPUTimingRingSize);
	FGPUTimingSlot& Slot = GPUTimingSlots[SlotIndex];
	check(Slot.bPending && BoundaryIndex >= 0 && BoundaryIndex < Slot.BoundaryCount);
	FRHIRenderQuery* const TimestampQuery = Slot.Queries[BoundaryIndex].GetQuery();
	check(TimestampQuery != nullptr);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ArcTerrainCBT.Timestamp.%s", BoundaryName),
		ERDGPassFlags::None,
		[TimestampQuery](FRHICommandList& RHICmdList)
		{
			RHICmdList.EndRenderQuery(TimestampQuery);
		});
}

void FArcTerrainCBTRendererExtension::DrainGPUTimingSamples(uint32 DrainFrame)
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;

	for (FGPUTimingSlot& Slot : GPUTimingSlots)
	{
		if (!Slot.bPending || Slot.BoundaryCount == 0)
		{
			continue;
		}

		uint64 FinalTimestamp = 0;
		if (!RHIGetRenderQueryResult(
			Slot.Queries[Slot.BoundaryCount - 1].GetQuery(), FinalTimestamp, false))
		{
			continue;
		}
		if (Slot.bCounterReadbackExpected
			&& (!Slot.CounterReadback.IsValid() || !Slot.CounterReadback->IsReady()))
		{
			continue;
		}

		TStaticArray<uint64, GPUTimingMode2BoundaryCount> Timestamps;
		bool bAllReady = true;
		for (int32 BoundaryIndex = 0; BoundaryIndex < Slot.BoundaryCount; ++BoundaryIndex)
		{
			if (!RHIGetRenderQueryResult(
				Slot.Queries[BoundaryIndex].GetQuery(), Timestamps[BoundaryIndex], false))
			{
				bAllReady = false;
				break;
			}
		}
		if (!bAllReady)
		{
			continue;
		}
		TStaticArray<uint64, GPUTimingInlineQueryCount> InlineTimestamps{};
		if (Slot.bInlineDispatchTiming)
		{
			for (int32 QueryIndex = 0; QueryIndex < GPUTimingInlineQueryCount; ++QueryIndex)
			{
				if (!RHIGetRenderQueryResult(
					Slot.InlineQueries[QueryIndex].GetQuery(), InlineTimestamps[QueryIndex], false))
				{
					bAllReady = false;
					break;
				}
			}
		}
		if (!bAllReady)
		{
			continue;
		}

		TArray<uint32> CorrelatedCounters;
		bool bHasCorrelatedCounters = false;
		if (Slot.bCounterReadbackExpected)
		{
			CorrelatedCounters.SetNumUninitialized(FArcTerrainCBTBuffers::PoolCounterNum);
			const uint32* const Counters = static_cast<const uint32*>(
				Slot.CounterReadback->Lock(CounterReadbackBytes));
			if (Counters != nullptr)
			{
				FMemory::Memcpy(CorrelatedCounters.GetData(), Counters, CounterReadbackBytes);
				bHasCorrelatedCounters = true;
			}
			Slot.CounterReadback->Unlock();
		}

		for (int32 BoundaryIndex = 0; BoundaryIndex < Slot.BoundaryCount; ++BoundaryIndex)
		{
			Slot.Queries[BoundaryIndex].ReleaseQuery();
		}
		if (Slot.bInlineDispatchTiming)
		{
			for (FRHIPooledRenderQuery& Query : Slot.InlineQueries)
			{
				Query.ReleaseQuery();
			}
		}
		Slot.bPending = false;

		const uint8 ExpectedBoundaryCount = Slot.Mode == 2
			? GPUTimingMode2BoundaryCount : GPUTimingMode1BoundaryCount;
		const bool bCurrentSample = Slot.Generation == GPUTimingGeneration
			&& Slot.Mode == GPUTimingMode
			&& Slot.BoundaryCount == ExpectedBoundaryCount
			&& Slot.TerrainId != 0
			&& Slot.TerrainId == BoundTerrainId;
		const bool bSampleHasInlineDispatchTiming = Slot.bInlineDispatchTiming;
		Slot.BoundaryCount = 0;
		Slot.bCounterReadbackExpected = false;
		Slot.bInlineDispatchTiming = false;
		if (!bCurrentSample)
		{
			continue;
		}

		bool bMonotonic = true;
		for (int32 BoundaryIndex = 1; BoundaryIndex < ExpectedBoundaryCount; ++BoundaryIndex)
		{
			bMonotonic = bMonotonic && Timestamps[BoundaryIndex] >= Timestamps[BoundaryIndex - 1];
		}
		if (bSampleHasInlineDispatchTiming)
		{
			for (int32 DispatchIndex = 0; DispatchIndex < GPUTimingInlineDispatchCount; ++DispatchIndex)
			{
				bMonotonic = bMonotonic
					&& InlineTimestamps[DispatchIndex * 2 + 1] >= InlineTimestamps[DispatchIndex * 2];
			}
		}
		if (!bMonotonic)
		{
			continue;
		}

		const uint64 TotalMicroseconds =
			Timestamps[ExpectedBoundaryCount - 1] - Timestamps[0];
		TStaticArray<uint64, GPUTimingStageCount> StageMicroseconds{};
		TStaticArray<uint64, GPUTimingInlineDispatchCount> DispatchMicroseconds{};
		TStaticArray<uint64, GPUTimingInlineGroupCount> DispatchGroupMicroseconds{};
		uint64 TopologyMicroseconds = 0;
		uint64 TopologyActiveMicroseconds = 0;
		if (Slot.Mode == 2)
		{
			for (int32 StageIndex = 0; StageIndex < GPUTimingStageCount; ++StageIndex)
			{
				StageMicroseconds[StageIndex] =
					Timestamps[StageIndex + 1] - Timestamps[StageIndex];
			}
			for (int32 StageIndex = GPUTimingTopologyFirstStage;
				StageIndex < GPUTimingTopologyFirstStage + GPUTimingTopologyStageCount; ++StageIndex)
			{
				TopologyMicroseconds += StageMicroseconds[StageIndex];
			}
		}
		if (bSampleHasInlineDispatchTiming)
		{
			for (int32 DispatchIndex = 0; DispatchIndex < GPUTimingInlineDispatchCount; ++DispatchIndex)
			{
				DispatchMicroseconds[DispatchIndex] = InlineTimestamps[DispatchIndex * 2 + 1]
					- InlineTimestamps[DispatchIndex * 2];
				TopologyActiveMicroseconds += DispatchMicroseconds[DispatchIndex];
			}
			DispatchGroupMicroseconds[static_cast<int32>(EGPUTimingInlineGroup::MergeSelect)] =
				DispatchMicroseconds[static_cast<int32>(EGPUTimingInlineDispatch::MergeSelect)];
			for (int32 PriorityOffset = 0; PriorityOffset < GPUTimingSplitPriorityPassCount;
				++PriorityOffset)
			{
				DispatchGroupMicroseconds[static_cast<int32>(EGPUTimingInlineGroup::SplitAdmission)] +=
					DispatchMicroseconds[
						static_cast<int32>(EGPUTimingInlineDispatch::SplitAdmissionPriority1)
						+ PriorityOffset];
				DispatchGroupMicroseconds[static_cast<int32>(EGPUTimingInlineGroup::SplitPrepare)] +=
					DispatchMicroseconds[
						static_cast<int32>(EGPUTimingInlineDispatch::SplitPreparePriority6)
						+ PriorityOffset];
			}
			DispatchGroupMicroseconds[static_cast<int32>(EGPUTimingInlineGroup::SplitReserve)] =
				DispatchMicroseconds[static_cast<int32>(EGPUTimingInlineDispatch::SplitReserve)];
			DispatchGroupMicroseconds[static_cast<int32>(EGPUTimingInlineGroup::SplitApply)] =
				DispatchMicroseconds[static_cast<int32>(EGPUTimingInlineDispatch::SplitApply)];
			DispatchGroupMicroseconds[static_cast<int32>(EGPUTimingInlineGroup::MergeStage)] =
				DispatchMicroseconds[static_cast<int32>(EGPUTimingInlineDispatch::MergeStage)];
		}
		if (GPUTimingAggregate.bActive && !GPUTimingAggregate.TotalMicroseconds.IsEmpty()
			&& (!GPUTimingAggregate.TopologyActiveMicroseconds.IsEmpty())
				!= bSampleHasInlineDispatchTiming)
		{
			ResetGPUTimingAggregate();
		}
		const double Now = FPlatformTime::Seconds();
		if (!GPUTimingAggregate.bActive)
		{
			GPUTimingAggregate.bActive = true;
			GPUTimingAggregate.StartTimeSeconds = Now;
			GPUTimingAggregate.FirstSourceFrame = Slot.SourceFrame;
		}
		GPUTimingAggregate.FirstSourceFrame = FMath::Min(
			GPUTimingAggregate.FirstSourceFrame, Slot.SourceFrame);
		GPUTimingAggregate.LastSourceFrame = FMath::Max(
			GPUTimingAggregate.LastSourceFrame, Slot.SourceFrame);
		GPUTimingAggregate.MaxReadbackLag = FMath::Max(
			GPUTimingAggregate.MaxReadbackLag, DrainFrame - Slot.SourceFrame);
		GPUTimingAggregate.TotalMicroseconds.Add(TotalMicroseconds);
		if (Slot.Mode == 2)
		{
			GPUTimingAggregate.TopologyMicroseconds.Add(TopologyMicroseconds);
			for (int32 StageIndex = 0; StageIndex < GPUTimingStageCount; ++StageIndex)
			{
				GPUTimingAggregate.StageMicroseconds[StageIndex].Add(StageMicroseconds[StageIndex]);
			}
			if (bSampleHasInlineDispatchTiming)
			{
				GPUTimingAggregate.TopologyActiveMicroseconds.Add(TopologyActiveMicroseconds);
				for (int32 DispatchIndex = 0; DispatchIndex < GPUTimingInlineDispatchCount; ++DispatchIndex)
				{
					GPUTimingAggregate.DispatchMicroseconds[DispatchIndex].Add(
						DispatchMicroseconds[DispatchIndex]);
				}
				for (int32 GroupIndex = 0; GroupIndex < GPUTimingInlineGroupCount; ++GroupIndex)
				{
					GPUTimingAggregate.DispatchGroupMicroseconds[GroupIndex].Add(
						DispatchGroupMicroseconds[GroupIndex]);
				}
			}
		}
		if (TotalMicroseconds >= GPUTimingAggregate.MaxTotalMicroseconds)
		{
			GPUTimingAggregate.MaxTotalMicroseconds = TotalMicroseconds;
			GPUTimingAggregate.MaxTotalSourceFrame = Slot.SourceFrame;
			GPUTimingAggregate.bMaxShaderStats = Slot.bShaderStats;
			GPUTimingAggregate.bMaxBulkRefine = Slot.bBulkRefine;
			GPUTimingAggregate.MaxMotionRefineIterations = Slot.MotionRefineIterations;
			GPUTimingAggregate.MaxTotalStageMicroseconds = StageMicroseconds;
			GPUTimingAggregate.bMaxHasInlineDispatchTiming = bSampleHasInlineDispatchTiming;
			GPUTimingAggregate.MaxTotalDispatchMicroseconds = DispatchMicroseconds;
			GPUTimingAggregate.MaxTotalDispatchGroupMicroseconds = DispatchGroupMicroseconds;
			GPUTimingAggregate.bMaxHasCorrelatedCounters = bHasCorrelatedCounters;
			if (bHasCorrelatedCounters)
			{
				FMemory::Memcpy(GPUTimingAggregate.MaxTotalCounters.GetData(),
					CorrelatedCounters.GetData(), CounterReadbackBytes);
			}
		}

		if (Now - GPUTimingAggregate.StartTimeSeconds < StatsLogIntervalSeconds)
		{
			continue;
		}

		const uint64 TotalP50 = TimingPercentile(GPUTimingAggregate.TotalMicroseconds, 0.50);
		const uint64 TotalP95 = TimingPercentile(GPUTimingAggregate.TotalMicroseconds, 0.95);
		if (Slot.Mode == 1)
		{
			UE_LOGF(LogArcTerrainCBT, Log,
				"CBTStats/T: mode 1 samples %d src %u..%u maxlag %u frames dropped %u | "
				"total-span us p50 %llu p95 %llu max %llu @src %u (stats %d bulk %d motion-refine %u)",
				GPUTimingAggregate.TotalMicroseconds.Num(), GPUTimingAggregate.FirstSourceFrame,
				GPUTimingAggregate.LastSourceFrame, GPUTimingAggregate.MaxReadbackLag,
				GPUTimingDroppedSamples, TotalP50, TotalP95,
				GPUTimingAggregate.MaxTotalMicroseconds, GPUTimingAggregate.MaxTotalSourceFrame,
				GPUTimingAggregate.bMaxShaderStats ? 1 : 0,
				GPUTimingAggregate.bMaxBulkRefine ? 1 : 0,
				GPUTimingAggregate.MaxMotionRefineIterations);
		}
		else
		{
			TStaticArray<uint64, GPUTimingStageCount> StageP95;
			for (int32 StageIndex = 0; StageIndex < GPUTimingStageCount; ++StageIndex)
			{
				StageP95[StageIndex] = TimingPercentile(
					GPUTimingAggregate.StageMicroseconds[StageIndex], 0.95);
			}
			const uint64 TopologyP95 = TimingPercentile(
				GPUTimingAggregate.TopologyMicroseconds, 0.95);
			TStaticArray<uint64, GPUTimingInlineGroupCount> DispatchGroupP95;
			for (int32 GroupIndex = 0; GroupIndex < GPUTimingInlineGroupCount; ++GroupIndex)
			{
				DispatchGroupP95[GroupIndex] = TimingPercentile(
					GPUTimingAggregate.DispatchGroupMicroseconds[GroupIndex], 0.95);
			}
			const uint64 TopologyActiveP95 = TimingPercentile(
				GPUTimingAggregate.TopologyActiveMicroseconds, 0.95);
			uint64 PeakTopologyMicroseconds = 0;
			uint64 PeakTopologyActiveMicroseconds = 0;
			for (int32 StageIndex = GPUTimingTopologyFirstStage;
				StageIndex < GPUTimingTopologyFirstStage + GPUTimingTopologyStageCount; ++StageIndex)
			{
				PeakTopologyMicroseconds += GPUTimingAggregate.MaxTotalStageMicroseconds[StageIndex];
			}
			for (uint64 DispatchTime : GPUTimingAggregate.MaxTotalDispatchMicroseconds)
			{
				PeakTopologyActiveMicroseconds += DispatchTime;
			}
			UE_LOGF(LogArcTerrainCBT, Log,
				"CBTStats/T: mode 2 samples %d src %u..%u maxlag %u frames dropped %u | "
				"total-span us p50 %llu p95 %llu max %llu @src %u (stats %d bulk %d motion-refine %u) | "
				"queue-subspan us p95 pre %llu classify %llu topology %llu (merge-select %llu split-pipeline %llu "
				"[claim-audit included only when stats=1] "
				"merge-stage %llu) mergecommit %llu base-refine %llu reduce %llu batch %llu compact %llu "
				"link-validate %llu readback-tail %llu | peak-queue-subspan @src %u us pre %llu classify %llu "
				"topology %llu (merge-select %llu split-pipeline %llu merge-stage %llu) mergecommit %llu "
				"base-refine %llu reduce %llu batch %llu compact %llu link-validate %llu readback-tail %llu",
				GPUTimingAggregate.TotalMicroseconds.Num(), GPUTimingAggregate.FirstSourceFrame,
				GPUTimingAggregate.LastSourceFrame, GPUTimingAggregate.MaxReadbackLag,
				GPUTimingDroppedSamples, TotalP50, TotalP95,
				GPUTimingAggregate.MaxTotalMicroseconds, GPUTimingAggregate.MaxTotalSourceFrame,
				GPUTimingAggregate.bMaxShaderStats ? 1 : 0,
				GPUTimingAggregate.bMaxBulkRefine ? 1 : 0,
				GPUTimingAggregate.MaxMotionRefineIterations,
				StageP95[0], StageP95[1], TopologyP95, StageP95[2], StageP95[3], StageP95[4],
				StageP95[5], StageP95[6], StageP95[7], StageP95[8], StageP95[9], StageP95[10],
				StageP95[11],
				GPUTimingAggregate.MaxTotalSourceFrame,
				GPUTimingAggregate.MaxTotalStageMicroseconds[0],
				GPUTimingAggregate.MaxTotalStageMicroseconds[1],
				PeakTopologyMicroseconds,
				GPUTimingAggregate.MaxTotalStageMicroseconds[2],
				GPUTimingAggregate.MaxTotalStageMicroseconds[3],
				GPUTimingAggregate.MaxTotalStageMicroseconds[4],
				GPUTimingAggregate.MaxTotalStageMicroseconds[5],
				GPUTimingAggregate.MaxTotalStageMicroseconds[6],
				GPUTimingAggregate.MaxTotalStageMicroseconds[7],
				GPUTimingAggregate.MaxTotalStageMicroseconds[8],
				GPUTimingAggregate.MaxTotalStageMicroseconds[9],
				GPUTimingAggregate.MaxTotalStageMicroseconds[10],
				GPUTimingAggregate.MaxTotalStageMicroseconds[11]);

			UE_LOGF(LogArcTerrainCBT, Log,
				"CBTStats/T-Dispatch: samples %d p95 active-sum us %llu (merge-select %llu "
				"split-admission[6] %llu split-prepare[6] %llu split-reserve %llu split-apply %llu "
				"merge-stage %llu) | peak @src %u available %d active-sum us %llu (merge-select %llu "
				"split-admission[6] %llu split-prepare[6] %llu split-reserve %llu split-apply %llu "
				"merge-stage %llu; stats-only claim-audit excluded from active-sum)",
				GPUTimingAggregate.TopologyActiveMicroseconds.Num(), TopologyActiveP95,
				DispatchGroupP95[static_cast<int32>(EGPUTimingInlineGroup::MergeSelect)],
				DispatchGroupP95[static_cast<int32>(EGPUTimingInlineGroup::SplitAdmission)],
				DispatchGroupP95[static_cast<int32>(EGPUTimingInlineGroup::SplitPrepare)],
				DispatchGroupP95[static_cast<int32>(EGPUTimingInlineGroup::SplitReserve)],
				DispatchGroupP95[static_cast<int32>(EGPUTimingInlineGroup::SplitApply)],
				DispatchGroupP95[static_cast<int32>(EGPUTimingInlineGroup::MergeStage)],
				GPUTimingAggregate.MaxTotalSourceFrame,
				GPUTimingAggregate.bMaxHasInlineDispatchTiming ? 1 : 0,
				PeakTopologyActiveMicroseconds,
				GPUTimingAggregate.MaxTotalDispatchGroupMicroseconds[
					static_cast<int32>(EGPUTimingInlineGroup::MergeSelect)],
				GPUTimingAggregate.MaxTotalDispatchGroupMicroseconds[
					static_cast<int32>(EGPUTimingInlineGroup::SplitAdmission)],
				GPUTimingAggregate.MaxTotalDispatchGroupMicroseconds[
					static_cast<int32>(EGPUTimingInlineGroup::SplitPrepare)],
				GPUTimingAggregate.MaxTotalDispatchGroupMicroseconds[
					static_cast<int32>(EGPUTimingInlineGroup::SplitReserve)],
				GPUTimingAggregate.MaxTotalDispatchGroupMicroseconds[
					static_cast<int32>(EGPUTimingInlineGroup::SplitApply)],
				GPUTimingAggregate.MaxTotalDispatchGroupMicroseconds[
					static_cast<int32>(EGPUTimingInlineGroup::MergeStage)]);
			UE_LOGF(LogArcTerrainCBT, Log,
				"CBTStats/T-DispatchPeakVector: src %u active dispatch us | admission p1..p6 "
				"%llu,%llu,%llu,%llu,%llu,%llu | prepare p6..p1 %llu,%llu,%llu,%llu,%llu,%llu",
				GPUTimingAggregate.MaxTotalSourceFrame,
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[1],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[2],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[3],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[4],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[5],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[6],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[7],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[8],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[9],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[10],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[11],
				GPUTimingAggregate.MaxTotalDispatchMicroseconds[12]);

			if (GPUTimingAggregate.bMaxHasCorrelatedCounters)
			{
				const auto& Counters = GPUTimingAggregate.MaxTotalCounters;
				const uint32 GuardedReclaimed =
					Counters[Counter_TopologySplitGuardedQuota]
					- FMath::Min(Counters[Counter_TopologySplitGuardedQuota],
						Counters[Counter_TopologySplitGuardedAdmitted]);
				UE_LOGF(LogArcTerrainCBT, Log,
					"CBTStats/T-Peak: src %u total-span us %llu | merge candidate/selected/deferred "
					"%u/%u/%u applied %u | split roots raw/canonical/duplicate/admitted/request-deferred "
					"%u/%u/%u/%u/%u guarded quota/use/reclaim %u/%u/%u | descriptors "
					"prepared/prepare-failed/reserved/applied/released %u/%u/%u/%u/%u | defer/refuse/malformed events "
					"budget-deferred/physical-refused/malformed %u/%u/%u | bisectors-applied %u "
					"child-slots-allocated %u held-claims-after-apply %u",
					GPUTimingAggregate.MaxTotalSourceFrame, GPUTimingAggregate.MaxTotalMicroseconds,
					Counters[Counter_MergePhaseCandidate],
					Counters[Counter_MergePhaseSelected], Counters[Counter_MergePhaseDeferred],
					Counters[Counter_MergeApplied],
					Counters[Counter_TopologySplitAppended],
					Counters[Counter_TopologySplitCanonical],
					Counters[Counter_TopologySplitDuplicate],
					Counters[Counter_TopologySplitAdmitted],
					Counters[Counter_TopologySplitDeferred],
					Counters[Counter_TopologySplitGuardedQuota],
					Counters[Counter_TopologySplitGuardedAdmitted], GuardedReclaimed,
					Counters[Counter_TopologySplitPrepared],
					Counters[Counter_TopologySplitPrepareFailed],
					Counters[Counter_TopologySplitReserved],
					Counters[Counter_TopologySplitAppliedDescriptor],
					Counters[Counter_TopologySplitReleasedDescriptor],
					Counters[Counter_TopologySplitBudgetDeferred],
					Counters[Counter_TopologySplitPhysicalRefused],
					Counters[Counter_TopologySplitMalformed],
					Counters[Counter_SplitApplied],
					Counters[Counter_TopologySplitAllocatedSlots],
					Counters[Counter_TopologySplitHeldClaims]);
				UE_LOGF(LogArcTerrainCBT, Log,
					"CBTStats/T-Peak-Buckets: src %u roots priority 1..6 raw "
					"%u,%u,%u,%u,%u,%u admitted %u,%u,%u,%u,%u,%u",
					GPUTimingAggregate.MaxTotalSourceFrame,
					Counters[Counter_TopologySplitRawBucketBase + 0],
					Counters[Counter_TopologySplitRawBucketBase + 1],
					Counters[Counter_TopologySplitRawBucketBase + 2],
					Counters[Counter_TopologySplitRawBucketBase + 3],
					Counters[Counter_TopologySplitRawBucketBase + 4],
					Counters[Counter_TopologySplitRawBucketBase + 5],
					Counters[Counter_TopologySplitAdmittedBucketBase + 0],
					Counters[Counter_TopologySplitAdmittedBucketBase + 1],
					Counters[Counter_TopologySplitAdmittedBucketBase + 2],
					Counters[Counter_TopologySplitAdmittedBucketBase + 3],
					Counters[Counter_TopologySplitAdmittedBucketBase + 4],
					Counters[Counter_TopologySplitAdmittedBucketBase + 5]);
				UE_LOGF(LogArcTerrainCBT, Log,
					"CBTStats/T-Peak-I: src %u roots raw=canonical+duplicate %ls, "
					"canonical=admitted+request-deferred %ls, admitted=prepared+prepare-failed %ls | "
					"descriptors prepared=applied+released %ls | child-slots=2*applied-bisectors %ls | "
					"held-claims-after-apply=0 %ls | integrity overflow/nonlive/claimed/missing/stale/"
					"invalid-link/chain-overflow/lost-entry/malformed %u/%u/%u/%u/%u/%u/%u/%u/%u (%ls)",
					GPUTimingAggregate.MaxTotalSourceFrame,
					Counters[Counter_TopologySplitAppended]
						== Counters[Counter_TopologySplitCanonical]
							+ Counters[Counter_TopologySplitDuplicate] ? TEXT("MATCH") : TEXT("MISMATCH"),
					Counters[Counter_TopologySplitCanonical]
						== Counters[Counter_TopologySplitAdmitted]
							+ Counters[Counter_TopologySplitDeferred] ? TEXT("MATCH") : TEXT("MISMATCH"),
					Counters[Counter_TopologySplitAdmitted]
						== Counters[Counter_TopologySplitPrepared]
							+ Counters[Counter_TopologySplitPrepareFailed] ? TEXT("MATCH") : TEXT("MISMATCH"),
					Counters[Counter_TopologySplitPrepared]
						== Counters[Counter_TopologySplitAppliedDescriptor]
							+ Counters[Counter_TopologySplitReleasedDescriptor] ? TEXT("MATCH") : TEXT("MISMATCH"),
					Counters[Counter_TopologySplitAllocatedSlots]
						== 2u * Counters[Counter_SplitApplied] ? TEXT("MATCH") : TEXT("MISMATCH"),
					Counters[Counter_TopologySplitHeldClaims] == 0u ? TEXT("MATCH") : TEXT("MISMATCH"),
					Counters[Counter_TopologySplitOverflow],
					Counters[Counter_TopologySelectedNonLive],
					Counters[Counter_TopologySelectedClaimed],
					Counters[Counter_TopologySelectedMissingBit],
					Counters[Counter_TopologyStaleCommit],
					Counters[Counter_SplitNonMutual] + Counters[Counter_MergeNonMutual]
						+ Counters[Counter_NonMutualRefs],
					Counters[Counter_SplitChainOverflow],
					Counters[Counter_TopologySplitLostEntry],
					Counters[Counter_TopologySplitMalformed],
					Counters[Counter_TopologySplitOverflow] == 0u
						&& Counters[Counter_TopologySelectedNonLive] == 0u
						&& Counters[Counter_TopologySelectedClaimed] == 0u
						&& Counters[Counter_TopologySelectedMissingBit] == 0u
						&& Counters[Counter_TopologyStaleCommit] == 0u
						&& Counters[Counter_SplitNonMutual] == 0u
						&& Counters[Counter_MergeNonMutual] == 0u
						&& Counters[Counter_NonMutualRefs] == 0u
						&& Counters[Counter_SplitChainOverflow] == 0u
						&& Counters[Counter_TopologySplitLostEntry] == 0u
						&& Counters[Counter_TopologySplitMalformed] == 0u
							? TEXT("MATCH") : TEXT("MISMATCH"));
			}
		}
		ResetGPUTimingAggregate();
	}
}

void FArcTerrainCBTRendererExtension::ClaimWorld_GameThread(const FArcTerrainCBTWorldClaim& Claim)
{
	ENQUEUE_RENDER_COMMAND(ArcTerrainCBTClaimWorld)(
		[Claim](FRHICommandListImmediate&)
		{
			FArcTerrainCBTRendererExtension& Extension = GArcTerrainCBTRendererExtension;
			UE::TScopeLock Lock(Extension.Mutex);
			Extension.ClaimWorld_RenderThread(Claim);
		});
}

void FArcTerrainCBTRendererExtension::ReleaseWorld_GameThread(uint64 WorldInstanceToken)
{
	ENQUEUE_RENDER_COMMAND(ArcTerrainCBTReleaseWorld)(
		[WorldInstanceToken](FRHICommandListImmediate&)
		{
			FArcTerrainCBTRendererExtension& Extension = GArcTerrainCBTRendererExtension;
			UE::TScopeLock Lock(Extension.Mutex);
			Extension.ReleaseWorld_RenderThread(WorldInstanceToken);
		});
}

void FArcTerrainCBTRendererExtension::PublishCandidate_GameThread(FArcTerrainCBTBaseMesh&& BaseMesh)
{
	ENQUEUE_RENDER_COMMAND(ArcTerrainCBTPublishCandidate)(
		[Published = MoveTemp(BaseMesh)](FRHICommandListImmediate&) mutable
		{
			FArcTerrainCBTRendererExtension& Extension = GArcTerrainCBTRendererExtension;
			UE::TScopeLock Lock(Extension.Mutex);
			Extension.PublishCandidate_RenderThread(MoveTemp(Published));
		});
}

void FArcTerrainCBTRendererExtension::UnpublishCandidate_GameThread(const FArcTerrainRenderIdentity& Identity)
{
	ENQUEUE_RENDER_COMMAND(ArcTerrainCBTUnpublishCandidate)(
		[Identity](FRHICommandListImmediate&)
		{
			FArcTerrainCBTRendererExtension& Extension = GArcTerrainCBTRendererExtension;
			UE::TScopeLock Lock(Extension.Mutex);
			Extension.UnpublishCandidate_RenderThread(Identity);
		});
}

void FArcTerrainCBTRendererExtension::RetireTerrain_GameThread(const FArcTerrainRenderOwnerIdentity& Owner)
{
	ENQUEUE_RENDER_COMMAND(ArcTerrainCBTRetireTerrain)(
		[Owner](FRHICommandListImmediate&)
		{
			FArcTerrainCBTRendererExtension& Extension = GArcTerrainCBTRendererExtension;
			UE::TScopeLock Lock(Extension.Mutex);
			Extension.RetireTerrain_RenderThread(Owner);
		});
}

void FArcTerrainCBTRendererExtension::ClaimWorld_RenderThread(const FArcTerrainCBTWorldClaim& Claim)
{
	if (!Claim.IsValid())
	{
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] claim rejected token=%llu worldtype=%d scenekey=%p",
			Claim.WorldInstanceToken, static_cast<int32>(Claim.WorldType), Claim.SceneKey);
		return;
	}

	const bool bRuntime = Claim.WorldType == EWorldType::PIE || Claim.WorldType == EWorldType::Game;
	if (bRuntime)
	{
		for (const TPair<uint64, FWorldClaimState>& Pair : WorldClaims)
		{
			const EWorldType::Type ExistingType = Pair.Value.Claim.WorldType;
			if (Pair.Key != Claim.WorldInstanceToken
				&& (ExistingType == EWorldType::PIE || ExistingType == EWorldType::Game))
			{
				UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] claim rejected token=%llu worldtype=%d scenekey=%p: runtime already claimed",
					Claim.WorldInstanceToken, static_cast<int32>(Claim.WorldType), Claim.SceneKey);
				return;
			}
		}
	}

	FWorldClaimState& State = WorldClaims.FindOrAdd(Claim.WorldInstanceToken);
	State.Claim = Claim;
	State.ClaimSerial = NextClaimSerial++;
	UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] claim token=%llu worldtype=%d scenekey=%p",
		Claim.WorldInstanceToken, static_cast<int32>(Claim.WorldType), Claim.SceneKey);
}

void FArcTerrainCBTRendererExtension::ReleaseWorld_RenderThread(uint64 WorldInstanceToken)
{
	const FWorldClaimState* ExistingClaim = WorldClaims.Find(WorldInstanceToken);
	UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] release token=%llu worldtype=%d scenekey=%p",
		WorldInstanceToken, ExistingClaim != nullptr ? static_cast<int32>(ExistingClaim->Claim.WorldType) : -1,
		ExistingClaim != nullptr ? ExistingClaim->Claim.SceneKey : nullptr);
	WorldClaims.Remove(WorldInstanceToken);
	if (ActiveOwnerLock.Key.WorldInstanceToken == WorldInstanceToken)
	{
		ActiveOwnerLock = FArcTerrainRenderOwnerIdentity();
	}
	if (ActiveWorldInstanceToken == WorldInstanceToken)
	{
		ReleaseActivePool();
		ActiveWorldInstanceToken = 0;
	}
	if (RuntimeCandidate.IsSet()
		&& RuntimeCandidate->BaseMesh.Identity.Owner.Key.WorldInstanceToken == WorldInstanceToken)
	{
		RuntimeCandidate.Reset();
	}
	if (ParkedEditorCandidate.IsSet()
		&& ParkedEditorCandidate->BaseMesh.Identity.Owner.Key.WorldInstanceToken == WorldInstanceToken)
	{
		ParkedEditorCandidate.Reset();
	}
#if !UE_BUILD_SHIPPING
	if (DelayedTestCandidate.IsSet()
		&& DelayedTestCandidate->BaseMesh.Identity.Owner.Key.WorldInstanceToken == WorldInstanceToken)
	{
		const FArcTerrainRenderIdentity& DelayedIdentity = DelayedTestCandidate->BaseMesh.Identity;
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] delayed publish rejected on release token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			DelayedIdentity.Owner.Key.WorldInstanceToken, DelayedIdentity.Owner.Key.TerrainInstanceId,
			DelayedIdentity.Owner.OwnerEpoch, DelayedIdentity.ResourceRevision, DelayedIdentity.RhiEpoch,
			static_cast<int32>(DelayedIdentity.WorldType), DelayedIdentity.SceneKey);
		DelayedTestCandidate.Reset();
		DelayedTestCandidateFrames = 0;
	}
#endif
}

bool FArcTerrainCBTRendererExtension::IsIdentityLive_RenderThread(
	const FArcTerrainRenderIdentity& Identity) const
{
	if (!Identity.IsValid() || Identity.RhiEpoch != RhiEpoch.load(std::memory_order_acquire))
	{
		return false;
	}

	const FWorldClaimState* ClaimState = WorldClaims.Find(Identity.Owner.Key.WorldInstanceToken);
	if (ClaimState == nullptr || ClaimState->Claim.WorldType != Identity.WorldType
		|| ClaimState->Claim.SceneKey != Identity.SceneKey)
	{
		return false;
	}

	const uint64* RetiredEpoch = RetiredOwnerEpochs.Find(Identity.Owner.Key);
	if (RetiredEpoch != nullptr && Identity.Owner.OwnerEpoch <= *RetiredEpoch)
	{
		return false;
	}

	const FUnpublishedOwnerState* Unpublished = UnpublishedOwners.Find(Identity.Owner.Key);
	return Unpublished == nullptr || Unpublished->OwnerEpoch != Identity.Owner.OwnerEpoch
		|| Identity.ResourceRevision > Unpublished->ResourceRevision;
}

void FArcTerrainCBTRendererExtension::PublishCandidate_RenderThread(FArcTerrainCBTBaseMesh&& BaseMesh,
	bool bAllowTestDelay)
{
	FCandidateState Candidate;
	Candidate.BaseMesh = MoveTemp(BaseMesh);
	PublishCandidateState_RenderThread(MoveTemp(Candidate), bAllowTestDelay);
}

void FArcTerrainCBTRendererExtension::PublishCandidateState_RenderThread(FCandidateState&& Candidate,
	bool bAllowTestDelay)
{
	const FArcTerrainRenderIdentity Identity = Candidate.BaseMesh.Identity;
	const bool bBaseMeshValid = Candidate.BaseMesh.IsValid();
	const bool bIdentityLive = bBaseMeshValid && IsIdentityLive_RenderThread(Identity);
	if (!bBaseMeshValid || !bIdentityLive)
	{
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] publish rejected token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p reason=%ls",
			Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
			Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
			static_cast<int32>(Identity.WorldType), Identity.SceneKey,
			bBaseMeshValid ? TEXT("identity-not-live") : TEXT("invalid-base-mesh"));
		return;
	}

#if !UE_BUILD_SHIPPING
	const int32 TestDelayFrames = ArcTerrainCBTRendererPrivate_9a07::CVarTestPublishDelayFrames.GetValueOnRenderThread();
	if (TestDelayFrames <= 0)
	{
		TestPublishDelayLastCvar = 0;
		bTestPublishDelayArmed = false;
		bTestPublishDelayConsumed = false;
	}
	else if (TestPublishDelayLastCvar <= 0)
	{
		TestPublishDelayLastCvar = TestDelayFrames;
		bTestPublishDelayArmed = true;
		bTestPublishDelayConsumed = false;
		UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] test publish delay armed frames=%d", TestDelayFrames);
	}
	else
	{
		TestPublishDelayLastCvar = TestDelayFrames;
	}
	if (bAllowTestDelay && bTestPublishDelayArmed && !bTestPublishDelayConsumed
		&& !DelayedTestCandidate.IsSet())
	{
		DelayedTestCandidate.Emplace(MoveTemp(Candidate));
		DelayedTestCandidateFrames = TestPublishDelayLastCvar;
		bTestPublishDelayConsumed = true;
		UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] test publish captured token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p delayframes=%d",
			Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
			Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
			static_cast<int32>(Identity.WorldType), Identity.SceneKey, DelayedTestCandidateFrames);
		return;
	}
#endif

	const bool bRuntime = Identity.WorldType == EWorldType::PIE || Identity.WorldType == EWorldType::Game;
	TOptional<FCandidateState>& Slot = bRuntime ? RuntimeCandidate : ParkedEditorCandidate;
	if (bRuntime && ActiveOwnerLock.IsValid()
		&& ActiveOwnerLock.Key.WorldInstanceToken == Identity.Owner.Key.WorldInstanceToken
		&& ActiveOwnerLock != Identity.Owner)
	{
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] publish rejected token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p reason=owner-preempted",
			Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
			Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
			static_cast<int32>(Identity.WorldType), Identity.SceneKey);
		return;
	}
	if (bRuntime && Slot.IsSet() && Slot->BaseMesh.Identity.Owner != Identity.Owner)
	{
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] publish rejected token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p reason=candidate-occupied",
			Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
			Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
			static_cast<int32>(Identity.WorldType), Identity.SceneKey);
		return;
	}
	if (Slot.IsSet() && Slot->BaseMesh.Identity == Identity)
	{
		for (const FArcTerrainCBTBindingSourceToken& SourceToken : Candidate.LiveBindingSources)
		{
			Slot->LiveBindingSources.Add(SourceToken);
		}
		return;
	}
	if (Slot.IsSet() && Slot->BaseMesh.Identity.Owner == Identity.Owner
		&& Slot->BaseMesh.Identity.ResourceRevision >= Identity.ResourceRevision)
	{
		const FArcTerrainRenderIdentity& RetainedIdentity = Slot->BaseMesh.Identity;
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] publish rejected reason=stale-revision incoming token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p; retained token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
			Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
			static_cast<int32>(Identity.WorldType), Identity.SceneKey,
			RetainedIdentity.Owner.Key.WorldInstanceToken, RetainedIdentity.Owner.Key.TerrainInstanceId,
			RetainedIdentity.Owner.OwnerEpoch, RetainedIdentity.ResourceRevision, RetainedIdentity.RhiEpoch,
			static_cast<int32>(RetainedIdentity.WorldType), RetainedIdentity.SceneKey);
		return;
	}
	Slot.Emplace(MoveTemp(Candidate));
	UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] publish accepted token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p sources=%d ready=%ls",
		Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
		Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
		static_cast<int32>(Identity.WorldType), Identity.SceneKey,
		Slot->LiveBindingSources.Num(), Slot->IsReady() ? TEXT("yes") : TEXT("no"));
}

void FArcTerrainCBTRendererExtension::UnpublishCandidate_RenderThread(
	const FArcTerrainRenderIdentity& Identity)
{
	UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] unpublish token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
		Identity.Owner.Key.WorldInstanceToken, Identity.Owner.Key.TerrainInstanceId,
		Identity.Owner.OwnerEpoch, Identity.ResourceRevision, Identity.RhiEpoch,
		static_cast<int32>(Identity.WorldType), Identity.SceneKey);
	FUnpublishedOwnerState& Unpublished = UnpublishedOwners.FindOrAdd(Identity.Owner.Key);
	if (Unpublished.OwnerEpoch < Identity.Owner.OwnerEpoch)
	{
		Unpublished.OwnerEpoch = Identity.Owner.OwnerEpoch;
		Unpublished.ResourceRevision = Identity.ResourceRevision;
	}
	else if (Unpublished.OwnerEpoch == Identity.Owner.OwnerEpoch)
	{
		Unpublished.ResourceRevision = FMath::Max(Unpublished.ResourceRevision, Identity.ResourceRevision);
	}

	TOptional<FCandidateState>* Slots[2] = { &RuntimeCandidate, &ParkedEditorCandidate };
	for (TOptional<FCandidateState>* Slot : Slots)
	{
		if (Slot->IsSet() && (*Slot)->BaseMesh.Identity.Owner == Identity.Owner
			&& (*Slot)->BaseMesh.Identity.ResourceRevision <= Identity.ResourceRevision)
		{
			Slot->Reset();
		}
	}
#if !UE_BUILD_SHIPPING
	if (DelayedTestCandidate.IsSet() && DelayedTestCandidate->BaseMesh.Identity.Owner == Identity.Owner
		&& DelayedTestCandidate->BaseMesh.Identity.ResourceRevision <= Identity.ResourceRevision)
	{
		DelayedTestCandidate.Reset();
		DelayedTestCandidateFrames = 0;
	}
#endif
	if (AcceptedIdentity.Owner == Identity.Owner
		&& AcceptedIdentity.ResourceRevision <= Identity.ResourceRevision)
	{
		ReleaseActivePool();
	}
}

void FArcTerrainCBTRendererExtension::RetireTerrain_RenderThread(
	const FArcTerrainRenderOwnerIdentity& Owner)
{
	UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] retire token=%llu terrain=%llu epoch=%llu",
		Owner.Key.WorldInstanceToken, Owner.Key.TerrainInstanceId, Owner.OwnerEpoch);
	uint64& RetiredEpoch = RetiredOwnerEpochs.FindOrAdd(Owner.Key);
	RetiredEpoch = FMath::Max(RetiredEpoch, Owner.OwnerEpoch);

	TOptional<FCandidateState>* Slots[2] = { &RuntimeCandidate, &ParkedEditorCandidate };
	for (TOptional<FCandidateState>* Slot : Slots)
	{
		if (Slot->IsSet() && (*Slot)->BaseMesh.Identity.Owner.Key == Owner.Key
			&& (*Slot)->BaseMesh.Identity.Owner.OwnerEpoch <= Owner.OwnerEpoch)
		{
			Slot->Reset();
		}
	}

	if (AcceptedIdentity.Owner.Key == Owner.Key && AcceptedIdentity.Owner.OwnerEpoch <= Owner.OwnerEpoch)
	{
		ReleaseActivePool();
	}
	if (ActiveOwnerLock.Key == Owner.Key && ActiveOwnerLock.OwnerEpoch <= Owner.OwnerEpoch)
	{
		ActiveOwnerLock = FArcTerrainRenderOwnerIdentity();
	}
#if !UE_BUILD_SHIPPING
	if (DelayedTestCandidate.IsSet() && DelayedTestCandidate->BaseMesh.Identity.Owner.Key == Owner.Key
		&& DelayedTestCandidate->BaseMesh.Identity.Owner.OwnerEpoch <= Owner.OwnerEpoch)
	{
		const FArcTerrainRenderIdentity& DelayedIdentity = DelayedTestCandidate->BaseMesh.Identity;
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] delayed publish rejected on retire token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			DelayedIdentity.Owner.Key.WorldInstanceToken, DelayedIdentity.Owner.Key.TerrainInstanceId,
			DelayedIdentity.Owner.OwnerEpoch, DelayedIdentity.ResourceRevision, DelayedIdentity.RhiEpoch,
			static_cast<int32>(DelayedIdentity.WorldType), DelayedIdentity.SceneKey);
		DelayedTestCandidate.Reset();
		DelayedTestCandidateFrames = 0;
	}
#endif
}

void FArcTerrainCBTRendererExtension::SelectActiveWorld_RenderThread()
{
	uint64 DesiredWorld = 0;
	for (const TPair<uint64, FWorldClaimState>& Pair : WorldClaims)
	{
		if (Pair.Value.Claim.WorldType == EWorldType::PIE || Pair.Value.Claim.WorldType == EWorldType::Game)
		{
			DesiredWorld = Pair.Key;
			break;
		}
	}

	if (DesiredWorld == 0)
	{
		const FWorldClaimState* Current = WorldClaims.Find(ActiveWorldInstanceToken);
		if (Current != nullptr && Current->Claim.WorldType == EWorldType::Editor)
		{
			DesiredWorld = ActiveWorldInstanceToken;
		}
	}
	if (DesiredWorld == 0 && ParkedEditorCandidate.IsSet()
		&& WorldClaims.Contains(ParkedEditorCandidate->BaseMesh.Identity.Owner.Key.WorldInstanceToken))
	{
		DesiredWorld = ParkedEditorCandidate->BaseMesh.Identity.Owner.Key.WorldInstanceToken;
	}
	if (DesiredWorld == 0)
	{
		uint64 LatestSerial = 0;
		for (const TPair<uint64, FWorldClaimState>& Pair : WorldClaims)
		{
			if (Pair.Value.Claim.WorldType == EWorldType::Editor && Pair.Value.ClaimSerial > LatestSerial)
			{
				LatestSerial = Pair.Value.ClaimSerial;
				DesiredWorld = Pair.Key;
			}
		}
	}

	if (DesiredWorld != ActiveWorldInstanceToken)
	{
		UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] world transition old=%llu new=%llu", ActiveWorldInstanceToken, DesiredWorld);
		ReleaseActivePool();
		ActiveOwnerLock = FArcTerrainRenderOwnerIdentity();
		ActiveWorldInstanceToken = DesiredWorld;
	}
}

void FArcTerrainCBTRendererExtension::AdoptActiveCandidate_RenderThread(FRDGBuilder& GraphBuilder)
{
	const FWorldClaimState* ClaimState = WorldClaims.Find(ActiveWorldInstanceToken);
	if (ClaimState == nullptr)
	{
		return;
	}
	const bool bRuntime = ClaimState->Claim.WorldType == EWorldType::PIE
		|| ClaimState->Claim.WorldType == EWorldType::Game;
	const TOptional<FCandidateState>& Slot = bRuntime ? RuntimeCandidate : ParkedEditorCandidate;
	if (!Slot.IsSet())
	{
		return;
	}
	const FArcTerrainRenderIdentity& CandidateIdentity = Slot->BaseMesh.Identity;
	if (CandidateIdentity.Owner.Key.WorldInstanceToken != ActiveWorldInstanceToken)
	{
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] candidate waiting reason=active-world-mismatch activeworld=%llu sources=%d ready=%ls token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			ActiveWorldInstanceToken, Slot->LiveBindingSources.Num(), Slot->IsReady() ? TEXT("yes") : TEXT("no"),
			CandidateIdentity.Owner.Key.WorldInstanceToken, CandidateIdentity.Owner.Key.TerrainInstanceId,
			CandidateIdentity.Owner.OwnerEpoch, CandidateIdentity.ResourceRevision, CandidateIdentity.RhiEpoch,
			static_cast<int32>(CandidateIdentity.WorldType), CandidateIdentity.SceneKey);
		return;
	}
	if (!IsIdentityLive_RenderThread(CandidateIdentity))
	{
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] candidate waiting reason=identity-not-live sources=%d ready=%ls token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			Slot->LiveBindingSources.Num(), Slot->IsReady() ? TEXT("yes") : TEXT("no"),
			CandidateIdentity.Owner.Key.WorldInstanceToken, CandidateIdentity.Owner.Key.TerrainInstanceId,
			CandidateIdentity.Owner.OwnerEpoch, CandidateIdentity.ResourceRevision, CandidateIdentity.RhiEpoch,
			static_cast<int32>(CandidateIdentity.WorldType), CandidateIdentity.SceneKey);
		return;
	}
	if (AcceptedIdentity == CandidateIdentity)
	{
		return;
	}
	if (!Slot->IsReady())
	{
		UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] candidate waiting reason=no-live-exact-source sources=0 ready=no token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			CandidateIdentity.Owner.Key.WorldInstanceToken, CandidateIdentity.Owner.Key.TerrainInstanceId,
			CandidateIdentity.Owner.OwnerEpoch, CandidateIdentity.ResourceRevision, CandidateIdentity.RhiEpoch,
			static_cast<int32>(CandidateIdentity.WorldType), CandidateIdentity.SceneKey);
		return;
	}
	if (ActiveOwnerLock.IsValid()
		&& ActiveOwnerLock.Key.WorldInstanceToken == ActiveWorldInstanceToken
		&& ActiveOwnerLock != CandidateIdentity.Owner)
	{
		UE_LOGF(LogArcTerrainCBT, Warning,"[CBTOwner] candidate waiting reason=owner-lock sources=%d ready=yes token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
			Slot->LiveBindingSources.Num(),
			CandidateIdentity.Owner.Key.WorldInstanceToken, CandidateIdentity.Owner.Key.TerrainInstanceId,
			CandidateIdentity.Owner.OwnerEpoch, CandidateIdentity.ResourceRevision, CandidateIdentity.RhiEpoch,
			static_cast<int32>(CandidateIdentity.WorldType), CandidateIdentity.SceneKey);
		return;
	}
	if (!ensureMsgf(Slot->LiveBindingSources.Num() > 0,
		TEXT("[CBTOwner] adoption without a live exact binding source: token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p"),
		CandidateIdentity.Owner.Key.WorldInstanceToken, CandidateIdentity.Owner.Key.TerrainInstanceId,
		CandidateIdentity.Owner.OwnerEpoch, CandidateIdentity.ResourceRevision, CandidateIdentity.RhiEpoch,
		static_cast<int32>(CandidateIdentity.WorldType), CandidateIdentity.SceneKey))
	{
		return;
	}
	if (AcceptedIdentity.IsValid() && AcceptedIdentity.Owner != CandidateIdentity.Owner)
	{
		UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] preempt token=%llu terrain=%llu epoch=%llu -> token=%llu terrain=%llu epoch=%llu",
			AcceptedIdentity.Owner.Key.WorldInstanceToken, AcceptedIdentity.Owner.Key.TerrainInstanceId,
			AcceptedIdentity.Owner.OwnerEpoch, CandidateIdentity.Owner.Key.WorldInstanceToken,
			CandidateIdentity.Owner.Key.TerrainInstanceId, CandidateIdentity.Owner.OwnerEpoch);
		ReleaseActivePool();
	}

	const FArcTerrainCBTBaseMesh& BaseMesh = Slot->BaseMesh;
	const int32 PublishedPoolDepth = FArcTerrainCBTBuffers::ClampPoolDepth(BaseMesh.PoolDepth);
	if (!Buffers.IsInitialized() || Buffers.GetNumFaces() != BaseMesh.NumFaces
		|| Buffers.GetPoolDepth() != PublishedPoolDepth)
	{
		check(IsInRenderingThread());
		Buffers.Initialize(GraphBuilder.RHICmdList, BaseMesh.NumFaces, PublishedPoolDepth);
		bPoolInitPending = true;
		bRefinePending = true;
	}

	if (BoundTerrainId != BaseMesh.Identity.Owner.Key.TerrainInstanceId)
	{
		InvalidateGPUTimingGeneration();
	}
	BoundTerrainId = BaseMesh.Identity.Owner.Key.TerrainInstanceId;
	ActiveBaseMesh = BaseMesh;
	ActiveOwnerLock = BaseMesh.Identity.Owner;
	AcceptedIdentity = BaseMesh.Identity;
	bRebindUpdatePending = true;
	const FArcTerrainCBTBindingSourceToken AdoptSource = *Slot->LiveBindingSources.CreateConstIterator();
	UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] adopt token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p sources=%d ready=yes sourceproxy=%llu sourceslot=%u",
		AcceptedIdentity.Owner.Key.WorldInstanceToken, AcceptedIdentity.Owner.Key.TerrainInstanceId,
		AcceptedIdentity.Owner.OwnerEpoch, AcceptedIdentity.ResourceRevision, AcceptedIdentity.RhiEpoch,
		static_cast<int32>(AcceptedIdentity.WorldType), AcceptedIdentity.SceneKey,
		Slot->LiveBindingSources.Num(), AdoptSource.ProxySerial, static_cast<uint32>(AdoptSource.SlotIndex));
}

void FArcTerrainCBTRendererExtension::BeginFrame(FRDGBuilder& GraphBuilder)
{
	UE::TScopeLock Lock(Mutex);
	bInFrame = true;
	UpdateGPUTimingMode();
	DrainGPUTimingSamples(GFrameNumberRenderThread);
#if !UE_BUILD_SHIPPING
	const int32 TestDelayFrames = ArcTerrainCBTRendererPrivate_9a07::CVarTestPublishDelayFrames.GetValueOnRenderThread();
	if (TestDelayFrames <= 0)
	{
		TestPublishDelayLastCvar = 0;
		bTestPublishDelayArmed = false;
		bTestPublishDelayConsumed = false;
	}
	else if (TestPublishDelayLastCvar <= 0)
	{
		TestPublishDelayLastCvar = TestDelayFrames;
		bTestPublishDelayArmed = true;
		bTestPublishDelayConsumed = false;
		UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] test publish delay armed frames=%d", TestDelayFrames);
	}
	else
	{
		TestPublishDelayLastCvar = TestDelayFrames;
	}
	if (DelayedTestCandidate.IsSet())
	{
		--DelayedTestCandidateFrames;
		if (DelayedTestCandidateFrames <= 0)
		{
			FCandidateState ExpiredCandidate = MoveTemp(DelayedTestCandidate.GetValue());
			DelayedTestCandidate.Reset();
			DelayedTestCandidateFrames = 0;
			UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] test publish delay expired");
			PublishCandidateState_RenderThread(MoveTemp(ExpiredCandidate), false);
		}
	}
#endif
	RetiredCounterReadbacks.RemoveAllSwap(
		[](const TUniquePtr<FRHIGPUBufferReadback>& Readback)
		{
			return !Readback.IsValid() || Readback->IsReady();
		},
		EAllowShrinking::No);
	SelectActiveWorld_RenderThread();
	AdoptActiveCandidate_RenderThread(GraphBuilder);
	DrainCounterReadback();
	PublishCounterStats();
	ArmedTopologyUpdate = FArmedTopologyUpdate();

	if (!Buffers.IsInitialized() || !ActiveBaseMesh.IsValid())
	{
		return;
	}

	const bool bEligibleFamily = EligibleFamilyFrame == GFrameNumberRenderThread;
	if (bPoolInitPending)
	{
		if (bEligibleFamily)
		{
			LastSubmittedFrame = GFrameNumberRenderThread;
			if (SubmitTopologyUpdate(GraphBuilder, ERDGPassFlags::Compute))
			{
				FinalizeUpdateForDraw(GraphBuilder, true);
				bTopologyUpdatePending = false;
				PendingTopologyIdentity = FArcTerrainRenderIdentity();
				PendingTopologySourceFrame = MAX_uint32;
				bRebindUpdatePending = false;
			}
		}
		return;
	}

	if (bRebindUpdatePending)
	{
		if (bTopologyUpdatePending)
		{
			FinalizeUpdateForDraw(GraphBuilder, true);
			bTopologyUpdatePending = false;
			PendingTopologyIdentity = FArcTerrainRenderIdentity();
			PendingTopologySourceFrame = MAX_uint32;
		}
		if (bEligibleFamily && LastSubmittedFrame != GFrameNumberRenderThread)
		{
			LastSubmittedFrame = GFrameNumberRenderThread;
			if (SubmitTopologyUpdate(GraphBuilder, ERDGPassFlags::Compute))
			{
				FinalizeUpdateForDraw(GraphBuilder, true);
				bRebindUpdatePending = false;
			}
		}
		return;
	}

	if (bTopologyUpdatePending)
	{
		FinalizeUpdateForDraw(GraphBuilder, true);
		bTopologyUpdatePending = false;
		PendingTopologyIdentity = FArcTerrainRenderIdentity();
		PendingTopologySourceFrame = MAX_uint32;
	}
	else if (bEligibleFamily && MainView.bValid
		&& MainView.CaptureFrame == GFrameNumberRenderThread)
	{
		FinalizeUpdateForDraw(GraphBuilder, false);
	}

	if (bEligibleFamily && MainView.bValid
		&& MainView.CaptureFrame == GFrameNumberRenderThread
		&& MainView.SceneKey == AcceptedIdentity.SceneKey
		&& LastSubmittedFrame != GFrameNumberRenderThread)
	{
		ArmedTopologyUpdate.Identity = AcceptedIdentity;
		ArmedTopologyUpdate.SceneKey = MainView.SceneKey;
		ArmedTopologyUpdate.ViewKey = MainView.ViewKey;
		ArmedTopologyUpdate.Frame = GFrameNumberRenderThread;
		ArmedTopologyUpdate.bArmed = true;
	}
}

void FArcTerrainCBTRendererExtension::EndFrame(FRDGBuilder&)
{
	UE::TScopeLock Lock(Mutex);
	bInFrame = false;
}

void FArcTerrainCBTRendererExtension::PostOpaque_RenderThread(FPostOpaqueRenderParameters& Parameters)
{
	UE::TScopeLock Lock(Mutex);
	if (!bInFrame || !ArmedTopologyUpdate.bArmed
		|| ArmedTopologyUpdate.Frame != GFrameNumberRenderThread
		|| Parameters.GraphBuilder == nullptr
		|| Parameters.Uid != static_cast<const void*>(ArmedTopologyUpdate.ViewKey)
		|| ArmedTopologyUpdate.SceneKey != AcceptedIdentity.SceneKey
		|| ArmedTopologyUpdate.Identity != AcceptedIdentity)
	{
		return;
	}

	const FArcTerrainRenderIdentity SubmittedIdentity = ArmedTopologyUpdate.Identity;
	ArmedTopologyUpdate = FArmedTopologyUpdate();
	LastSubmittedFrame = GFrameNumberRenderThread;
	if (SubmitTopologyUpdate(*Parameters.GraphBuilder, ERDGPassFlags::AsyncCompute))
	{
		PendingTopologyIdentity = SubmittedIdentity;
		PendingTopologySourceFrame = GFrameNumberRenderThread;
		bTopologyUpdatePending = true;
	}
}

void FArcTerrainCBTRendererExtension::LogStats(const uint32* Counters, int32 NumFaces,
	int32 MaxSubdivisionDepth) const
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;

	// A zero classify count against a non-zero live count is the one reading that makes every other
	// number on these lines meaningless: the indirect dispatch args were empty.
	const uint32 ClassifiedLeaves = Counters[Counter_ClassifyThreads];

	// Drawn plus both cull categories must equal live in the same frame: compaction tallies exactly one
	// terminal category for every published live record, so a gap is a lost append.
	const TCHAR* const DistanceCullResolution = !CounterStats.DistanceCull.bRequested
		? TEXT("not requested")
		: (CounterStats.DistanceCull.bEnabled ? TEXT("ENABLED") : TEXT("DISABLED by validation"));
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/A: live %u = drawn %u + frustum/invalid %u + distance %u "
		"(cull frustum %ls, distance %ls requested radius %.3f km, list %ls) | faces %d | "
		"classify %u | votes split %u merge %u keep %u | prism culled fine %u, guarded split %u keep %u fine %u "
		"| applied split %u merge %u "
		"| alloc %u refused %u nonmutual %u | poolfull %u",
		Counters[Counter_LiveBisectors], Counters[Counter_DrawnBisectors],
		Counters[Counter_FrustumCulled], Counters[Counter_DistanceCulled],
		MainView.bFrozen ? TEXT("FROZEN") : TEXT("live"),
		DistanceCullResolution, CounterStats.DistanceCull.RadiusCm / 100000.0,
		LastCulledBindFrame == GFrameNumberRenderThread
			? TEXT("BOUND")
			: TEXT("REFUSED, whole live set drawn"),
		NumFaces, ClassifiedLeaves,
		Counters[Counter_VoteSplit], Counters[Counter_VoteMerge], Counters[Counter_VoteKeep],
		Counters[Counter_PrismCulledFine], Counters[Counter_PrismGuardedSplit],
		Counters[Counter_PrismGuardedKeep], Counters[Counter_PrismGuardedFine],
		Counters[Counter_SplitApplied], Counters[Counter_MergeApplied],
		Counters[Counter_AllocCursor], Counters[Counter_AllocRefused], Counters[Counter_NonMutualRefs],
		Counters[Counter_SplitPoolFull]);

	const double MeanDepth = ClassifiedLeaves > 0u
		? static_cast<double>(Counters[Counter_DepthSum]) / static_cast<double>(ClassifiedLeaves)
		: 0.0;
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/B: depth min %d max %d mean %.2f | splitchain chained %u (len2 %u len3plus %u) "
		"| splitdefer mergingtwin %u mergeunsure %u (stalemerge walked %u) claimlost %u nonmutual %u "
		"maxdepth %u chainoverflow %u badface %u",
		ClassifiedLeaves > 0u ? static_cast<int32>(Counters[Counter_DepthMin]) : -1,
		ClassifiedLeaves > 0u ? static_cast<int32>(Counters[Counter_DepthMax]) : -1,
		MeanDepth,
		Counters[Counter_SplitChained],
		Counters[Counter_SplitChainLen2], Counters[Counter_SplitChainLen3Plus],
		Counters[Counter_SplitTwinMerging], Counters[Counter_SplitTwinMergeUnsure],
		Counters[Counter_SplitTwinStaleMerge],
		Counters[Counter_SplitLostElection], Counters[Counter_SplitNonMutual],
		Counters[Counter_SplitMaxDepth],
		Counters[Counter_SplitChainOverflow], Counters[Counter_SplitChainBadFace]);

	FVector ViewOriginLocal = FVector::ZeroVector;
	if (MainView.bValid)
	{
		ViewOriginLocal = ActiveBaseMesh.LocalToWorld.Inverse().TransformPosition(MainView.ViewOrigin);
	}
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/C: view %ls %.0fx%.0f pxperrad %.1f | cam local (%.0f, %.0f, %.0f) cm | targetareapx "
		"%.1f maxedgepx %.1f mergefrac %.2f errorpx %.1f | maxdepth %d",
		MainView.bValid ? TEXT("VALID") : TEXT("NONE"),
		MainView.ViewportSize.X, MainView.ViewportSize.Y, MainView.PixelsPerRadian,
		ViewOriginLocal.X, ViewOriginLocal.Y, ViewOriginLocal.Z,
		FMath::Max(ActiveBaseMesh.TargetTrianglePixels, 1.0f),
		ResolveMaxEdgePixels(ActiveBaseMesh),
		FMath::Clamp(ActiveBaseMesh.MergeThresholdFraction, 0.05f, 0.9f),
		FMath::Max(ActiveBaseMesh.TargetErrorPixels, 1.0f),
		MaxSubdivisionDepth);

	// A merge allocates nothing, so every reason it did not fire is a deferral on this line; seedalloc
	// belongs to the root seed alone and reads against [F]'s seeded count.
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/D: mergedefer root %u sibdeeper %u sibmismatch %u fardeeper %u farsibdeeper %u "
		"farmismatch %u nonmutual %u notunanimous %u election %u | seedalloc %u refused %u",
		Counters[Counter_MergeRoot], Counters[Counter_MergeSiblingDeeper],
		Counters[Counter_MergeSiblingMismatch], Counters[Counter_MergeFarDeeper],
		Counters[Counter_MergeFarSiblingDeeper], Counters[Counter_MergeFarMismatch],
		Counters[Counter_MergeNonMutual],
		Counters[Counter_MergeNotUnanimous], Counters[Counter_MergeLostElection],
		Counters[Counter_SeedCursor], Counters[Counter_SeedAllocRefused]);

	// Face ids partition the pool into one LEB tree each, so the `Node ^ 1` sibling test in the apply pass
	// only means anything when both node ids come from the same face; the same/xface split says which.
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/E: sibmismatch same %u xface %u | farmismatch same %u xface %u | splittwin same %u "
		"xface %u (xface defer %u refuse %u) | boundary leaves %u mismatchhits %u",
		Counters[Counter_SibMismatchSameFace], Counters[Counter_SibMismatchDiffFace],
		Counters[Counter_FarMismatchSameFace], Counters[Counter_FarMismatchDiffFace],
		Counters[Counter_SplitTwinSameFace], Counters[Counter_SplitTwinDiffFace],
		Counters[Counter_SplitDiffFaceDeferred], Counters[Counter_SplitDiffFaceRefused],
		Counters[Counter_BoundaryLeaves], Counters[Counter_BoundaryMismatch]);

	// `allocrefused` non-zero means part of the world has no roots this frame; `seeded` is how many faces
	// the per-frame root pass populated, so it is the face count once and zero from then on.
	UE_LOGF(LogArcTerrainCBT, Log,"CBTStats/F: seeded %u allocrefused %u | attach pairs %u | refine splits %u defer %u refused %u",
		Counters[Counter_SeedPopulated], Counters[Counter_FaceAllocRefused],
		Counters[Counter_AttachPairs], Counters[Counter_RefineSplits],
		Counters[Counter_RefineDeferred], Counters[Counter_RefineRefused]);

	// A LEB sibling sits at the same depth unless it is itself subdivided, so partnerdepth equal/shallower
	// says the deferral refused a mergeable diamond, while one/twoplus says the tree really is uneven.
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/G: partnerdepth equal %u one %u twoplus %u shallower %u | noref sib %u farsib %u "
		"| leaves deepernbr %u deepest %u nosib %u sibnotkin %u",
		Counters[Counter_MergePartnerEqual], Counters[Counter_MergePartnerOneDeeper],
		Counters[Counter_MergePartnerTwoDeeper], Counters[Counter_MergePartnerShallower],
		Counters[Counter_MergeSiblingNoRef], Counters[Counter_MergeFarSiblingNoRef],
		Counters[Counter_LeafDeeperThanNeighbor], Counters[Counter_LeafDeepestNeighbor],
		Counters[Counter_LeafSiblingNoRef], Counters[Counter_LeafSiblingNotKin]);

	// The apply pass's own read of the command buffer, against the classify pass's votes on CBTStats/A.
	// threads must equal classify, and seen split/merge/keep must equal votes split/merge/keep — any gap
	// is a command that did not survive the trip between the two dispatches.
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/H: applypass threads %u | seen split %u merge %u keep %u other %u "
		"| echo split %u merge %u LOST %u",
		Counters[Counter_SMThreads], Counters[Counter_SMSeenSplit], Counters[Counter_SMSeenMerge],
		Counters[Counter_SMSeenKeep], Counters[Counter_SMSeenOther],
		Counters[Counter_CmdEchoSplit], Counters[Counter_CmdEchoMerge], Counters[Counter_CmdEchoLost]);

	// Leaves at their geometry cap still reading TOO_COARSE, split by diagnostic height tier.
	uint32 CeilingCoarse = 0u;
	for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
	{
		CeilingCoarse += Counters[Counter_MaxDepthCoarse + Tier];
	}
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/I: cap coarse t0 %u t1 %u t2 %u t3 %u | mixedtier %u | byerror %u byarea %u "
		"height-over-target %u "
		"| max errorpx %.2f errorcm %u areapx %u | dist m %d..%u",
		Counters[Counter_MaxDepthCoarse], Counters[Counter_MaxDepthCoarse + 1],
		Counters[Counter_MaxDepthCoarse + 2], Counters[Counter_MaxDepthCoarse + 3],
		Counters[Counter_LeafTiersMixed],
		Counters[Counter_MaxDepthByError], Counters[Counter_MaxDepthByArea],
		Counters[Counter_HeightOverAtCap],
		static_cast<double>(Counters[Counter_MaxDepthErrorPixelsX100]) / 100.0,
		Counters[Counter_MaxDepthErrorCmMax],
		Counters[Counter_MaxDepthAreaPixelsMax],
		CeilingCoarse > 0u ? static_cast<int32>(Counters[Counter_MaxDepthDistMetersMin]) : -1,
		Counters[Counter_MaxDepthDistMetersMax]);

	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/I2: resident cap %d | tier1 count %u maxdepth %u | tier2 count %u maxdepth %u "
		"| resident mismatch %u | fill count %u at/above %u mismatch %u vote-violation %u",
		MaxSubdivisionDepth,
		Counters[Counter_ResidentTier1Count], Counters[Counter_ResidentTier1MaxDepth],
		Counters[Counter_ResidentTier2Count], Counters[Counter_ResidentTier2MaxDepth],
		Counters[Counter_ResidentCapMismatch], Counters[Counter_FillCount],
		Counters[Counter_FillAtOrAboveCap], Counters[Counter_FillCapMismatch],
		Counters[Counter_FillCeilingVoteViolation]);

	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/J: coarse reasons edge %u sampledheight %u area %u (subsets overlap) | height unknown %u "
		"| max edge %.3f px sampledheight %.3f px",
		Counters[Counter_CoarseByEdge], Counters[Counter_CoarseByHeight], Counters[Counter_CoarseByArea],
		Counters[Counter_HeightUnknown], FloatFromBits(Counters[Counter_MaxEdgeBits]),
		FloatFromBits(Counters[Counter_MaxSampledErrorBits]));

	// Where a coarsening vote dies: only the `fine` column meets the full hysteresis threshold and becomes
	// a MERGE command the apply pass can ever see.
	UE_LOGF(LogArcTerrainCBT, Log,"CBTStats/K: parent of a TOO_FINE leaf | coarse area %u error %u edge %u | keep %u | fine %u",
		Counters[Counter_ParentCoarseArea], Counters[Counter_ParentCoarseError],
		Counters[Counter_ParentCoarseEdge],
		Counters[Counter_ParentKeep], Counters[Counter_ParentFine]);

	const uint32 HeightSourceCulled = Counters[Counter_HeightLocalCulled]
		+ Counters[Counter_HeightFillCulled] + Counters[Counter_HeightGlobalSpanCulled]
		+ Counters[Counter_HeightGlobalUnsafeCulled];
	const uint32 HeightSourceGuarded = Counters[Counter_HeightLocalGuarded]
		+ Counters[Counter_HeightFillGuarded] + Counters[Counter_HeightGlobalSpanGuarded]
		+ Counters[Counter_HeightGlobalUnsafeGuarded];
	const uint32 PrismGuarded = Counters[Counter_PrismGuardedSplit]
		+ Counters[Counter_PrismGuardedKeep] + Counters[Counter_PrismGuardedFine];
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/L: height source culled local %u fill %u global-span %u global-unsafe %u = %u vs prism %u (%ls) "
		"| guarded local %u fill %u global-span %u global-unsafe %u = %u vs prism %u (%ls)",
		Counters[Counter_HeightLocalCulled], Counters[Counter_HeightFillCulled],
		Counters[Counter_HeightGlobalSpanCulled], Counters[Counter_HeightGlobalUnsafeCulled],
		HeightSourceCulled, Counters[Counter_PrismCulledFine],
		HeightSourceCulled == Counters[Counter_PrismCulledFine] ? TEXT("MATCH") : TEXT("MISMATCH"),
		Counters[Counter_HeightLocalGuarded], Counters[Counter_HeightFillGuarded],
		Counters[Counter_HeightGlobalSpanGuarded], Counters[Counter_HeightGlobalUnsafeGuarded],
		HeightSourceGuarded, PrismGuarded,
		HeightSourceGuarded == PrismGuarded ? TEXT("MATCH") : TEXT("MISMATCH"));

	const uint32 MergeCandidates = Counters[Counter_MergePhaseCandidate];
	const uint32 MergeSelected = Counters[Counter_MergePhaseSelected];
	const uint32 MergeDeferred = Counters[Counter_MergePhaseDeferred];
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/M: merge scheduler candidate %u = selected %u + deferred %u (%ls) "
		"| selected %u vs applied %u (%ls) | source count %u phase %u pressure override %u "
		"| fast threads resolved %u + fallback %u vs merge-seen %u (%ls) "
		"| projected selected %u + deferred %u vs resolved %u (%ls) | key mismatch %u",
		MergeCandidates, MergeSelected, MergeDeferred,
		MergeCandidates == MergeSelected + MergeDeferred ? TEXT("MATCH") : TEXT("MISMATCH"),
		MergeSelected, Counters[Counter_MergeApplied],
		MergeSelected == Counters[Counter_MergeApplied] ? TEXT("MATCH") : TEXT("MISMATCH"),
		Counters[Counter_MergePhaseCount], Counters[Counter_MergeFramePhase],
		Counters[Counter_MergePressureOverride],
		Counters[Counter_MergeFastResolved], Counters[Counter_MergeFastFallback],
		Counters[Counter_SMSeenMerge],
		Counters[Counter_MergeFastResolved] + Counters[Counter_MergeFastFallback]
			== Counters[Counter_SMSeenMerge] ? TEXT("MATCH") : TEXT("MISMATCH"),
		Counters[Counter_MergeFastProjectedSelected], Counters[Counter_MergeFastProjectedDeferred],
		Counters[Counter_MergeFastResolved],
		Counters[Counter_MergeFastProjectedSelected] + Counters[Counter_MergeFastProjectedDeferred]
			== Counters[Counter_MergeFastResolved] ? TEXT("MATCH") : TEXT("MISMATCH"),
		Counters[Counter_MergeFastKeyMismatch]);

	if (Counters[Counter_TopologyNormalBudget] != 0u
		|| Counters[Counter_TopologySplitAppended] != 0u)
	{
		const uint32 SelectedDiamonds = Counters[Counter_TopologySelectedDiamonds];
		const uint32 SelectedMembers = Counters[Counter_TopologySelectedMembers];
		const uint32 StagedDiamonds = Counters[Counter_TopologyStagedDiamonds];
		const uint32 StagedMembers = Counters[Counter_TopologyStagedMembers];
		const uint32 CommittedDiamonds = Counters[Counter_TopologyCommittedDiamonds];
		const uint32 CommittedMembers = Counters[Counter_TopologyCommittedMembers];
		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/N: bounded producers split %u overflow %u | scan %u fast-reject %u gather %u "
			"| selected diamonds %u <= scan %u (%ls), members %u <= %u (%ls), overflow %u "
			"| split-consumed %u lost-entry %u selected-deferred %u "
			"| selected-check nonlive %u claimed %u missing-bit %u "
			"| lifecycle diamonds %u/%u/%u (%ls) members %u/%u/%u (%ls) stale-commit %u "
			"| cursor %u -> %u wrap %u no-selection-wrap %u | budget normal %u pressure %u active %u "
			"pressure-active %u",
			Counters[Counter_TopologySplitAppended], Counters[Counter_TopologySplitOverflow],
			Counters[Counter_TopologyScanOrdinals], Counters[Counter_TopologyFastRejects],
			Counters[Counter_TopologyFullGathers], SelectedDiamonds,
			Counters[Counter_TopologyScanOrdinals],
			SelectedDiamonds <= Counters[Counter_TopologyScanOrdinals] ? TEXT("MATCH") : TEXT("MISMATCH"),
			SelectedMembers, 4u * SelectedDiamonds,
			SelectedMembers <= 4u * SelectedDiamonds ? TEXT("MATCH") : TEXT("MISMATCH"),
			Counters[Counter_TopologySelectedOverflow], Counters[Counter_TopologySplitConsumed],
			Counters[Counter_TopologySplitLostEntry], Counters[Counter_TopologySplitSelectedDeferred],
			Counters[Counter_TopologySelectedNonLive], Counters[Counter_TopologySelectedClaimed],
			Counters[Counter_TopologySelectedMissingBit],
			SelectedDiamonds, StagedDiamonds, CommittedDiamonds,
			SelectedDiamonds == StagedDiamonds && StagedDiamonds == CommittedDiamonds
				? TEXT("MATCH") : TEXT("MISMATCH"),
			SelectedMembers, StagedMembers, CommittedMembers,
			SelectedMembers == StagedMembers && StagedMembers == CommittedMembers
				? TEXT("MATCH") : TEXT("MISMATCH"),
			Counters[Counter_TopologyStaleCommit], Counters[Counter_TopologyCursorStart],
			Counters[Counter_TopologyCursorEnd], Counters[Counter_TopologyCursorWrapped],
			Counters[Counter_TopologyFullWrapNoSelection], Counters[Counter_TopologyNormalBudget],
			Counters[Counter_TopologyPressureBudget], Counters[Counter_TopologyActiveBudget],
			Counters[Counter_TopologyPressureActive]);

		uint32 RawBuckets = 0u;
		uint32 CanonicalBuckets = 0u;
		uint32 AdmittedBuckets = 0u;
		for (int32 Bucket = 0; Bucket < static_cast<int32>(SplitPriorityBucketCount); ++Bucket)
		{
			RawBuckets += Counters[Counter_TopologySplitRawBucketBase + Bucket];
			CanonicalBuckets += Counters[Counter_TopologySplitCanonicalBucketBase + Bucket];
			AdmittedBuckets += Counters[Counter_TopologySplitAdmittedBucketBase + Bucket];
		}
		const uint32 RawSplits = Counters[Counter_TopologySplitAppended];
		const uint32 CanonicalSplits = Counters[Counter_TopologySplitCanonical];
		const uint32 AdmittedSplits = Counters[Counter_TopologySplitAdmitted];
		const uint32 GuardedReclaimed = Counters[Counter_TopologySplitGuardedQuota]
			- FMath::Min(Counters[Counter_TopologySplitGuardedQuota],
				Counters[Counter_TopologySplitGuardedAdmitted]);
		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/O: split roots admission raw %u = canonical %u + duplicate %u (%ls) "
			"| canonical %u = admitted %u + deferred %u (%ls) | buckets raw/canonical/admitted "
			"%u/%u/%u (%ls) | guarded reserve quota/use/reclaim %u/%u/%u | priority 1..6 raw "
			"%u,%u,%u,%u,%u,%u admitted %u,%u,%u,%u,%u,%u",
			RawSplits, CanonicalSplits, Counters[Counter_TopologySplitDuplicate],
			RawSplits == CanonicalSplits + Counters[Counter_TopologySplitDuplicate]
				? TEXT("MATCH") : TEXT("MISMATCH"),
			CanonicalSplits, AdmittedSplits, Counters[Counter_TopologySplitDeferred],
			CanonicalSplits == AdmittedSplits + Counters[Counter_TopologySplitDeferred]
				? TEXT("MATCH") : TEXT("MISMATCH"),
			RawBuckets, CanonicalBuckets, AdmittedBuckets,
			RawBuckets == RawSplits && CanonicalBuckets == CanonicalSplits
				&& AdmittedBuckets == AdmittedSplits ? TEXT("MATCH") : TEXT("MISMATCH"),
			Counters[Counter_TopologySplitGuardedQuota],
			Counters[Counter_TopologySplitGuardedAdmitted], GuardedReclaimed,
			Counters[Counter_TopologySplitRawBucketBase + 0],
			Counters[Counter_TopologySplitRawBucketBase + 1],
			Counters[Counter_TopologySplitRawBucketBase + 2],
			Counters[Counter_TopologySplitRawBucketBase + 3],
			Counters[Counter_TopologySplitRawBucketBase + 4],
			Counters[Counter_TopologySplitRawBucketBase + 5],
			Counters[Counter_TopologySplitAdmittedBucketBase + 0],
			Counters[Counter_TopologySplitAdmittedBucketBase + 1],
			Counters[Counter_TopologySplitAdmittedBucketBase + 2],
			Counters[Counter_TopologySplitAdmittedBucketBase + 3],
			Counters[Counter_TopologySplitAdmittedBucketBase + 4],
			Counters[Counter_TopologySplitAdmittedBucketBase + 5]);

		const uint32 PreparedDescriptors = Counters[Counter_TopologySplitPrepared];
		const uint32 AppliedDescriptors = Counters[Counter_TopologySplitAppliedDescriptor];
		const uint32 ReleasedDescriptors = Counters[Counter_TopologySplitReleasedDescriptor];
		const uint32 AppliedBisectors = Counters[Counter_SplitApplied];
		const uint32 AllocatedChildSlots = Counters[Counter_TopologySplitAllocatedSlots];
		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/O2: admitted roots %u = prepared descriptors %u + prepare-failed roots %u (%ls) "
			"| prepared descriptors %u = applied %u + released %u (%ls) | reserved descriptors %u "
			"| releases budget-deferred %u physical-refused %u malformed %u | allocated child slots %u "
			"= 2 * applied conformity bisectors %u (%ls) | held claims after apply %u (%ls)",
			AdmittedSplits, PreparedDescriptors, Counters[Counter_TopologySplitPrepareFailed],
			AdmittedSplits == PreparedDescriptors + Counters[Counter_TopologySplitPrepareFailed]
				? TEXT("MATCH") : TEXT("MISMATCH"),
			PreparedDescriptors, AppliedDescriptors, ReleasedDescriptors,
			PreparedDescriptors == AppliedDescriptors + ReleasedDescriptors
				? TEXT("MATCH") : TEXT("MISMATCH"),
			Counters[Counter_TopologySplitReserved],
			Counters[Counter_TopologySplitBudgetDeferred],
			Counters[Counter_TopologySplitPhysicalRefused],
			Counters[Counter_TopologySplitMalformed], AllocatedChildSlots, AppliedBisectors,
			AllocatedChildSlots == 2u * AppliedBisectors ? TEXT("MATCH") : TEXT("MISMATCH"),
			Counters[Counter_TopologySplitHeldClaims],
			Counters[Counter_TopologySplitHeldClaims] == 0u ? TEXT("MATCH") : TEXT("MISMATCH"));

		const uint32 InvalidLinks = Counters[Counter_SplitNonMutual]
			+ Counters[Counter_MergeNonMutual] + Counters[Counter_NonMutualRefs];
		const bool bIntegrityClean = Counters[Counter_TopologySplitOverflow] == 0u
			&& Counters[Counter_TopologySelectedNonLive] == 0u
			&& Counters[Counter_TopologySelectedClaimed] == 0u
			&& Counters[Counter_TopologySelectedMissingBit] == 0u
			&& Counters[Counter_TopologyStaleCommit] == 0u && InvalidLinks == 0u
			&& Counters[Counter_SplitChainOverflow] == 0u
			&& Counters[Counter_TopologySplitLostEntry] == 0u
			&& Counters[Counter_TopologySplitMalformed] == 0u;
		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/O3: integrity overflow %u selected-nonlive %u selected-claimed %u "
			"selected-missing-bit %u stale-commit %u invalid-links %u chain-overflow %u lost-entry %u "
			"malformed %u (%ls)",
			Counters[Counter_TopologySplitOverflow], Counters[Counter_TopologySelectedNonLive],
			Counters[Counter_TopologySelectedClaimed], Counters[Counter_TopologySelectedMissingBit],
			Counters[Counter_TopologyStaleCommit], InvalidLinks,
			Counters[Counter_SplitChainOverflow], Counters[Counter_TopologySplitLostEntry],
			Counters[Counter_TopologySplitMalformed],
			bIntegrityClean ? TEXT("MATCH") : TEXT("MISMATCH"));
	}
}

void FArcTerrainCBTRendererExtension::LogProbe(const uint32* Counters) const
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;

	FVector ViewOriginLocal = FVector::ZeroVector;
	if (MainView.bValid)
	{
		ViewOriginLocal = ActiveBaseMesh.LocalToWorld.Inverse().TransformPosition(MainView.ViewOrigin);
	}
	// The corners the metric transforms are EYE-RELATIVE, so this matrix's translation row is what a
	// corner of (0,0,0) lands on: it should read as the eye itself, not as the terrain origin.
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTStats/P0: view %.0fx%.0f pxperrad %.3f | targets area %.2f px edge %.2f px error %.2f px mergefrac "
		"%.2f flatrelief %.1fx to %.4g cm/m2 | eye local (%.1f, %.1f, %.1f) cm | clipw fallbacks %u "
		"| ceiling branch fallback %u projected %u",
		MainView.ViewportSize.X, MainView.ViewportSize.Y, MainView.PixelsPerRadian,
		FMath::Max(ActiveBaseMesh.TargetTrianglePixels, 1.0f),
		ResolveMaxEdgePixels(ActiveBaseMesh),
		FMath::Max(ActiveBaseMesh.TargetErrorPixels, 1.0f),
		FMath::Clamp(ActiveBaseMesh.MergeThresholdFraction, 0.05f, 0.9f),
		FMath::Clamp(ActiveBaseMesh.FlatAreaRelief, 1.0f, 4096.0f),
		FMath::Clamp(ActiveBaseMesh.FlatReliefSagCmPerMeterSq, 0.0001f, 1000.0f),
		ViewOriginLocal.X, ViewOriginLocal.Y, ViewOriginLocal.Z,
		Counters[Counter_ClipWFallback], Counters[Counter_CeilFallback], Counters[Counter_CeilProjected]);

	for (int32 Row = 0; Row < 4; ++Row)
	{
		UE_LOGF(LogArcTerrainCBT, Log,"CBTStats/P0: EyeLocalToClip row %d = (%.9g, %.9g, %.9g, %.9g)",
			Row, LastEyeLocalToClip.M[Row][0], LastEyeLocalToClip.M[Row][1],
			LastEyeLocalToClip.M[Row][2], LastEyeLocalToClip.M[Row][3]);
	}

	const TCHAR* const RecordNames[4] = {
		TEXT("A max-areapx, any leaf"),
		TEXT("B ceiling-coarse, NEAREST"),
		TEXT("C ceiling-coarse, FARTHEST"),
		TEXT("D fine leaf vs COARSE parent, NEAREST"),
	};
	const int32 RecordBases[4] = { ProbeBaseA, ProbeBaseB, ProbeBaseC, ProbeBaseD };
	const int32 RecordKeySlots[4] = {
		Counter_ProbeKeyA, Counter_ProbeKeyB, Counter_ProbeKeyC, Counter_ProbeKeyD };

	for (int32 Record = 0; Record < 4; ++Record)
	{
		const int32 Base = RecordBases[Record];
		const uint32 ElectionKey = Counters[RecordKeySlots[Record]];
		const uint32 KeyFirst = Counters[Base + Probe_KeyFirst];
		const uint32 KeyLast = Counters[Base + Probe_KeyLast];
		// B and D elect on a MIN and reset to 0xFFFFFFFF; A and C elect on a MAX and reset to 0. Either
		// identity still standing means no leaf met the record's condition this frame.
		if (ElectionKey == 0u || ElectionKey == MAX_uint32)
		{
			UE_LOGF(LogArcTerrainCBT, Log,"CBTStats/P%d [%ls]: no leaf elected this frame.",
				Record + 1, RecordNames[Record]);
			continue;
		}
		// The record is written by whichever thread last raised the key, non-atomically. A better key
		// arriving mid-write leaves the guards disagreeing, and the numbers below are then a mix of two
		// leaves — reported, never quietly averaged away.
		const bool bWhole = KeyFirst == KeyLast && KeyFirst == ElectionKey;

		const uint32 Flags = Counters[Base + Probe_Flags];
		const int32 Verdict = static_cast<int32>(Counters[Base + Probe_Verdict]);

		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/P%d [%ls]%ls: face %u depth %u node (%u, %u) verdict %d visibility %ls | "
			"%ls branch | atceiling %d leaf tier %u cap %u fillonly %d errmeasured %d "
			"resident c0 %d c1 %d c2 %d mid %d",
			Record + 1, RecordNames[Record], bWhole ? TEXT("") : TEXT(" TORN"),
			Counters[Base + Probe_Face], Counters[Base + Probe_Depth],
			Counters[Base + Probe_NodeLo], Counters[Base + Probe_NodeHi], Verdict,
			ProbeVisibilityName(Counters[Base + Probe_Visibility]),
			(Flags & Probe_FlagFallback) != 0u ? TEXT("FALLBACK solid-angle") : TEXT("PROJECTED"),
			(Flags & Probe_FlagAtCeiling) != 0u ? 1 : 0,
			Counters[Base + Probe_LeafTier], Counters[Base + Probe_LeafCap],
			(Flags & Probe_FlagFillOnly) != 0u ? 1 : 0,
			(Flags & Probe_FlagErrorMeasured) != 0u ? 1 : 0,
			(Flags & Probe_FlagResident0) != 0u ? 1 : 0,
			(Flags & Probe_FlagResident1) != 0u ? 1 : 0,
			(Flags & Probe_FlagResident2) != 0u ? 1 : 0,
			(Flags & Probe_FlagResidentMid) != 0u ? 1 : 0);

		UE_LOGF(LogArcTerrainCBT, Log,"CBTStats/P%d   corners c0 (%.9g, %.9g) c1 (%.9g, %.9g) c2 (%.9g, %.9g)",
			Record + 1,
			ProbeFloat(Counters, Base, Probe_Corners + 0), ProbeFloat(Counters, Base, Probe_Corners + 1),
			ProbeFloat(Counters, Base, Probe_Corners + 2), ProbeFloat(Counters, Base, Probe_Corners + 3),
			ProbeFloat(Counters, Base, Probe_Corners + 4), ProbeFloat(Counters, Base, Probe_Corners + 5));

		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/P%d   eye-relative cm p0 (%.3f, %.3f, %.3f) p1 (%.3f, %.3f, %.3f) "
			"p2 (%.3f, %.3f, %.3f)",
			Record + 1,
			ProbeFloat(Counters, Base, Probe_Points + 0), ProbeFloat(Counters, Base, Probe_Points + 1),
			ProbeFloat(Counters, Base, Probe_Points + 2), ProbeFloat(Counters, Base, Probe_Points + 3),
			ProbeFloat(Counters, Base, Probe_Points + 4), ProbeFloat(Counters, Base, Probe_Points + 5),
			ProbeFloat(Counters, Base, Probe_Points + 6), ProbeFloat(Counters, Base, Probe_Points + 7),
			ProbeFloat(Counters, Base, Probe_Points + 8));

		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/P%d   clip w (%.6g, %.6g, %.6g) vs minclipw %.3f | dist %.2f cm (%.1f m) "
			"| area3d %.6g cm2 -> %.6g px vs effective target %.6g px | edge %.6g px | "
			"sampled errorcm %.6g -> %.6g px (legacy reducible %.6g cm) | discovery %.6g / %.6g px",
			Record + 1,
			ProbeFloat(Counters, Base, Probe_ClipW + 0), ProbeFloat(Counters, Base, Probe_ClipW + 1),
			ProbeFloat(Counters, Base, Probe_ClipW + 2), ProbeFloat(Counters, Base, Probe_MinClipW),
			ProbeFloat(Counters, Base, Probe_DistCm), ProbeFloat(Counters, Base, Probe_DistCm) * 0.01f,
			ProbeFloat(Counters, Base, Probe_Area3DCm2), ProbeFloat(Counters, Base, Probe_AreaPixels),
			ProbeFloat(Counters, Base, Probe_EffTarget),
			ProbeFloat(Counters, Base, Probe_EdgePixels), ProbeFloat(Counters, Base, Probe_ErrorCm),
			ProbeFloat(Counters, Base, Probe_ErrorPixels), ProbeFloat(Counters, Base, Probe_LegacyErrorCm),
			ProbeFloat(Counters, Base, Probe_DiscoveryEdgePixels),
			ProbeFloat(Counters, Base, Probe_DiscoveryTargetPixels));

		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/P%d   heights cm c0 %.3f c1 %.3f c2 %.3f mid %.3f | sag whole %.6g low %.6g "
			"high %.6g",
			Record + 1,
			ProbeFloat(Counters, Base, Probe_Heights + 0), ProbeFloat(Counters, Base, Probe_Heights + 1),
			ProbeFloat(Counters, Base, Probe_Heights + 2), ProbeFloat(Counters, Base, Probe_Heights + 3),
			ProbeFloat(Counters, Base, Probe_Deviation + 0),
			ProbeFloat(Counters, Base, Probe_Deviation + 1),
			ProbeFloat(Counters, Base, Probe_Deviation + 2));

		UE_LOGF(LogArcTerrainCBT, Log,"CBTStats/P%d   prism height source %ls | tier %u sector (%d,%d) slice %u range [%.3f, %.3f] cm",
			Record + 1, ProbeHeightSourceName(Counters[Base + Probe_HeightSource]),
			Counters[Base + Probe_HeightTier], ProbeInt(Counters, Base, Probe_HeightSectorX),
			ProbeInt(Counters, Base, Probe_HeightSectorY), Counters[Base + Probe_HeightSlice],
			ProbeFloat(Counters, Base, Probe_HeightMinCm),
			ProbeFloat(Counters, Base, Probe_HeightMaxCm));

		if (Base != ProbeBaseD)
		{
			continue;
		}

		const uint32 ParentFlags = Counters[Base + Probe_D_ParentFlags];
		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/P%d   PARENT depth %u verdict %d visibility %ls | %ls branch | atceiling %d "
			"leaf tier %u cap %u fillonly %d resident c0 %d c1 %d c2 %d mid %d",
			Record + 1,
			Counters[Base + Probe_D_ParentDepth],
			static_cast<int32>(Counters[Base + Probe_D_ParentVerdict]),
			ProbeVisibilityName(Counters[Base + Probe_D_ParentVisibility]),
			(ParentFlags & Probe_FlagFallback) != 0u ? TEXT("FALLBACK solid-angle") : TEXT("PROJECTED"),
			(ParentFlags & Probe_FlagAtCeiling) != 0u ? 1 : 0,
			Counters[Base + Probe_D_ParentLeafTier], Counters[Base + Probe_D_ParentLeafCap],
			(ParentFlags & Probe_FlagFillOnly) != 0u ? 1 : 0,
			(ParentFlags & Probe_FlagResident0) != 0u ? 1 : 0,
			(ParentFlags & Probe_FlagResident1) != 0u ? 1 : 0,
			(ParentFlags & Probe_FlagResident2) != 0u ? 1 : 0,
			(ParentFlags & Probe_FlagResidentMid) != 0u ? 1 : 0);

		// The one line that separates real sampled error from unknown residency. Unknown keeps the fill-slice
		// flat relief because extra topology cannot reconstruct height data that is not loaded.
		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/P%d   PARENT dist %.2f cm | area %.6g px vs efftarget %.6g px | errorcm %.6g "
			"-> %.6g px vs target %.6g px | edge %.6g px vs target %.6g px | discovery %.6g / %.6g px "
			"| sag %.6g cm/m2 "
			"measured %d (leaf sag %.6g measured %d)",
			Record + 1,
			ProbeFloat(Counters, Base, Probe_D_ParentDistCm),
			ProbeFloat(Counters, Base, Probe_D_ParentAreaPixels),
			ProbeFloat(Counters, Base, Probe_D_ParentEffTarget),
			ProbeFloat(Counters, Base, Probe_D_ParentErrorCm),
			ProbeFloat(Counters, Base, Probe_D_ParentErrorPixels),
			ProbeFloat(Counters, Base, Probe_D_TargetErrorPixels),
			ProbeFloat(Counters, Base, Probe_D_ParentEdgePixels),
			ProbeFloat(Counters, Base, Probe_D_TargetEdgePixels),
			ProbeFloat(Counters, Base, Probe_D_ParentDiscoveryEdgePixels),
			ProbeFloat(Counters, Base, Probe_D_ParentDiscoveryTargetPixels),
			ProbeFloat(Counters, Base, Probe_D_ParentSagCmPerM2),
			(ParentFlags & Probe_FlagErrorMeasured) != 0u ? 1 : 0,
			ProbeFloat(Counters, Base, Probe_D_SagCmPerM2),
			(Flags & Probe_FlagErrorMeasured) != 0u ? 1 : 0);

		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTStats/P%d   PARENT prism height source %ls | tier %u sector (%d,%d) slice %u "
			"range [%.3f, %.3f] cm",
			Record + 1,
			ProbeHeightSourceName(Counters[Base + Probe_D_ParentHeightSource]),
			Counters[Base + Probe_D_ParentHeightTier],
			ProbeInt(Counters, Base, Probe_D_ParentHeightSectorX),
			ProbeInt(Counters, Base, Probe_D_ParentHeightSectorY),
			Counters[Base + Probe_D_ParentHeightSlice],
			ProbeFloat(Counters, Base, Probe_D_ParentHeightMinCm),
			ProbeFloat(Counters, Base, Probe_D_ParentHeightMaxCm));
	}
}

void FArcTerrainCBTRendererExtension::DrainLatencyReadbacks(uint32 DrainFrame, bool bLoggingEnabled)
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;
	bool bHasActivePendingReadback = false;
	for (const FLatencyReadbackSlot& Slot : LatencyReadbacks)
	{
		bHasActivePendingReadback = bHasActivePendingReadback || Slot.bPending;
	}
	if (!bLoggingEnabled && !bHasActivePendingReadback && RetiredLatencyReadbacks.IsEmpty()
		&& PendingLatencyGapRanges.IsEmpty() && !bLatencyRetirementBlocked)
	{
		return;
	}

	auto DrainReadySlot = [this, DrainFrame](FLatencyReadbackSlot& Slot, bool bProcess,
		bool bGapCoveredBeforeSample = false)
	{
		const FLatencySampleMetadata Metadata = Slot.Metadata;
		const uint32* const Counters =
			static_cast<const uint32*>(Slot.Readback->Lock(LatencyReadbackBytes));
		if (Counters != nullptr && bProcess)
		{
			const int32 SlotIndex = static_cast<int32>(&Slot - LatencyReadbacks.GetData());
			ProcessLatencySample(Counters, SlotIndex, DrainFrame, bGapCoveredBeforeSample);
		}
		else if (Counters == nullptr && bProcess)
		{
			EnqueueLatencyGap(Metadata.SourceFrame, Metadata.EpisodeId, Metadata.LoggerGeneration,
				TEXT("readback-lock"));
		}
		if (Counters != nullptr)
		{
			Slot.Readback->Unlock();
		}
		Slot.Metadata = FLatencySampleMetadata();
		Slot.bPending = false;
	};

	// Retired objects retain ownership until their GPU copies complete, but never consume an active-ring
	// slot or participate in a later logger generation's ordering/logging.
	for (int32 RetiredIndex = RetiredLatencyReadbacks.Num() - 1; RetiredIndex >= 0; --RetiredIndex)
	{
		FLatencyReadbackSlot& Retired = RetiredLatencyReadbacks[RetiredIndex];
		if (Retired.Readback.IsValid() && Retired.Readback->IsReady())
		{
			const uint32* const Counters =
				static_cast<const uint32*>(Retired.Readback->Lock(LatencyReadbackBytes));
			(void)Counters;
			if (Counters != nullptr)
			{
				Retired.Readback->Unlock();
			}
			RetiredLatencyReadbacks.RemoveAtSwap(RetiredIndex, 1, EAllowShrinking::No);
		}
	}

	if (!bLoggingEnabled)
	{
		PendingLatencyGapRanges.Reset();
	}
	else
	{
		PendingLatencyGapRanges.RemoveAll([this](const FLatencyPendingGapRange& Gap)
		{
			return Gap.LoggerGeneration != LatencyLoggerGeneration;
		});
	}

	bLatencyRetirementBlocked = false;
	// Invalid-generation copies are either drained immediately or moved to the bounded retired quarantine.
	// If quarantine is saturated, the old slot remains owned and unavailable until a retired copy completes.
	for (int32 SlotIndex = 0; SlotIndex < LatencyReadbackRingSize; ++SlotIndex)
	{
		FLatencyReadbackSlot& Slot = LatencyReadbacks[SlotIndex];
		if (!Slot.bPending || (bLoggingEnabled && Slot.Metadata.LoggerGeneration == LatencyLoggerGeneration))
		{
			continue;
		}
		if (Slot.Readback.IsValid() && Slot.Readback->IsReady())
		{
			DrainReadySlot(Slot, false);
		}
		else if (RetiredLatencyReadbacks.Num() < RetiredLatencyReadbackLimit)
		{
			FLatencyReadbackSlot& Retired = RetiredLatencyReadbacks.AddDefaulted_GetRef();
			Retired.Readback = MoveTemp(Slot.Readback);
			Retired.Metadata = MoveTemp(Slot.Metadata);
			Retired.bPending = true;
			Slot = FLatencyReadbackSlot();
		}
		else
		{
			bLatencyRetirementBlocked = true;
		}
	}

	if (!bLoggingEnabled)
	{
		return;
	}

	// A diagnostic copy that itself exceeds the episode window must not hold the ordered stream forever.
	// Quarantine the in-flight object, replace its sample with an ordered gap, and keep progressing.
	const uint32 ReadbackMaxFrames = static_cast<uint32>(FMath::Clamp(
		CVarLatencyMaxFrames.GetValueOnRenderThread(), 1, 600));
	for (int32 SlotIndex = 0; SlotIndex < LatencyReadbackRingSize; ++SlotIndex)
	{
		FLatencyReadbackSlot& Slot = LatencyReadbacks[SlotIndex];
		if (!Slot.bPending || Slot.Metadata.LoggerGeneration != LatencyLoggerGeneration
			|| !Slot.Readback.IsValid() || Slot.Readback->IsReady()
			|| DrainFrame <= Slot.Metadata.SourceFrame
			|| DrainFrame - Slot.Metadata.SourceFrame <= ReadbackMaxFrames)
		{
			continue;
		}
		if (RetiredLatencyReadbacks.Num() >= RetiredLatencyReadbackLimit)
		{
			bLatencyRetirementBlocked = true;
			continue;
		}
		const FLatencySampleMetadata Metadata = Slot.Metadata;
		FLatencyReadbackSlot& Retired = RetiredLatencyReadbacks.AddDefaulted_GetRef();
		Retired.Readback = MoveTemp(Slot.Readback);
		Retired.Metadata = MoveTemp(Slot.Metadata);
		Retired.bPending = true;
		Slot = FLatencyReadbackSlot();
		EnqueueLatencyGap(Metadata.SourceFrame, Metadata.EpisodeId, Metadata.LoggerGeneration,
			TEXT("readback-timeout"));
	}

	// Samples and explicit gaps share one source-frame stream. A later ready event waits behind the
	// oldest in-flight copy, so the episode watermark can only advance and a current gap can never leap
	// ahead of an older GPU sample.
	for (;;)
	{
		int32 EarliestSlot = INDEX_NONE;
		uint32 EarliestSlotFrame = MAX_uint32;
		for (int32 SlotIndex = 0; SlotIndex < LatencyReadbackRingSize; ++SlotIndex)
		{
			const FLatencyReadbackSlot& Slot = LatencyReadbacks[SlotIndex];
			if (Slot.bPending && Slot.Metadata.LoggerGeneration == LatencyLoggerGeneration
				&& Slot.Metadata.SourceFrame < EarliestSlotFrame)
			{
				EarliestSlot = SlotIndex;
				EarliestSlotFrame = Slot.Metadata.SourceFrame;
			}
		}

		int32 EarliestGap = INDEX_NONE;
		uint32 EarliestGapFrame = MAX_uint32;
		for (int32 GapIndex = 0; GapIndex < PendingLatencyGapRanges.Num(); ++GapIndex)
		{
			const FLatencyPendingGapRange& Gap = PendingLatencyGapRanges[GapIndex];
			if (Gap.LoggerGeneration == LatencyLoggerGeneration && Gap.FirstFrame < EarliestGapFrame)
			{
				EarliestGap = GapIndex;
				EarliestGapFrame = Gap.FirstFrame;
			}
		}

		if (EarliestGap != INDEX_NONE
			&& (EarliestSlot == INDEX_NONE || EarliestGapFrame < EarliestSlotFrame))
		{
			const FLatencyPendingGapRange& Candidate = PendingLatencyGapRanges[EarliestGap];
			if (Candidate.bOverflowMarker && EarliestSlot != INDEX_NONE
				&& EarliestSlotFrame <= Candidate.LastFrame)
			{
				if (!LatencyReadbacks[EarliestSlot].Readback.IsValid()
					|| !LatencyReadbacks[EarliestSlot].Readback->IsReady())
				{
					break;
				}
				DrainReadySlot(LatencyReadbacks[EarliestSlot], true, true);
				continue;
			}
			const FLatencyPendingGapRange Gap = MoveTemp(PendingLatencyGapRanges[EarliestGap]);
			PendingLatencyGapRanges.RemoveAtSwap(EarliestGap, 1, EAllowShrinking::No);
			ProcessLatencyGap(Gap.FirstFrame, Gap.LastFrame, Gap.ExplicitGapCount,
				Gap.EpisodeId, Gap.LoggerGeneration, *Gap.Reason);
			continue;
		}

		if (EarliestSlot == INDEX_NONE)
		{
			break;
		}
		const FLatencyReadbackSlot& Slot = LatencyReadbacks[EarliestSlot];
		if (!Slot.Readback.IsValid() || !Slot.Readback->IsReady())
		{
			break;
		}
		DrainReadySlot(LatencyReadbacks[EarliestSlot], true);
	}
}

void FArcTerrainCBTRendererExtension::ProcessLatencySample(const uint32* Counters, int32 SlotIndex,
	uint32 DrainFrame, bool bGapCoveredBeforeSample)
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;

	const FLatencySampleMetadata& Metadata = LatencyReadbacks[SlotIndex].Metadata;
	if (!bLatencyLoggerWasEnabled || Metadata.LoggerGeneration != LatencyLoggerGeneration)
	{
		return;
	}
	if (!LatencyEpisode.bActive || Metadata.EpisodeId != LatencyEpisode.Id)
	{
		return;
	}
	const uint32 VisibleDebt = Counters[LatencyCounter_VisibleSplittable];
	const float WorstAreaRatio = FloatFromBits(Counters[LatencyCounter_WorstAreaRatioBits]);
	const float WorstErrorRatio = FloatFromBits(Counters[LatencyCounter_WorstErrorRatioBits]);
	const float WorstEdgeRatio = FloatFromBits(Counters[LatencyCounter_WorstEdgeRatioBits]);
	const uint32 Lag = DrainFrame >= Metadata.SourceFrame ? DrainFrame - Metadata.SourceFrame : 0u;

	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTLatency src=%u drain=%u lag=%u ep=%u cause=%ls turn=%.2fdeg turnSpeed=%.1fdeg/s "
		"move=%.1fcm moveSpeed=%.1fcm/s forwardSpeed=%.1fcm/s lateralSpeed=%.1fcm/s "
		"motionRefineRequested=%u motionRefineScheduled=%u live=%u "
		"visibleDebt=%u capped=[%u,%u,%u,%u] splitApplied=%u splitDeferred=%u splitRefused=%u "
		"motionRefine=[%u,%u,%u] "
		"poolFull=%u areaMax=%.3fx edgeMax=%.3fx errorMax=%.3fx areaLevelsEstimate=%u listBound=%d bulkRefine=%d",
		Metadata.SourceFrame, DrainFrame, Lag, Metadata.EpisodeId, LatencyCauseName(Metadata.MotionCause),
		Metadata.TurnDegrees, Metadata.TurnDegreesPerSecond, Metadata.MoveCm,
		Metadata.MoveCmPerSecond, Metadata.ForwardSpeedCmPerSecond,
		Metadata.LateralSpeedCmPerSecond, static_cast<uint32>(Metadata.MotionRefineRequestedIterations),
		static_cast<uint32>(Metadata.MotionRefineScheduledIterations),
		Counters[LatencyCounter_LiveBisectors], VisibleDebt,
		Counters[LatencyCounter_CappedTier0], Counters[LatencyCounter_CappedTier1],
		Counters[LatencyCounter_CappedTier2], Counters[LatencyCounter_CappedTier3],
		Counters[LatencyCounter_SplitApplied], Counters[LatencyCounter_SplitDeferred],
		Counters[LatencyCounter_SplitRefused], Counters[LatencyCounter_MotionRefineApplied],
		Counters[LatencyCounter_MotionRefineDeferred], Counters[LatencyCounter_MotionRefineRefused],
		Counters[LatencyCounter_PoolFull], WorstAreaRatio, WorstEdgeRatio,
		WorstErrorRatio, Counters[LatencyCounter_AreaLevelEstimate], Metadata.bListBoundObserved ? 1 : 0,
		Metadata.bBulkRefineSubmitted ? 1 : 0);

	if (CVarLatencyLog.GetValueOnRenderThread() >= 2)
	{
		UE_LOGF(LogArcTerrainCBT, Log,
			"CBTLatencyConfig src=%u viewOrigin=(%.1f,%.1f,%.1f) viewDir=(%.5f,%.5f,%.5f) "
			"viewport=%.0fx%.0f pxPerRad=%.2f poolDepth=%d capacity=%u faces=%d maxSubdiv=%d "
			"residentCap=%d targetArea=%.2f maxEdge=%.2f targetError=%.2f drawMargin=%.3f tessMargin=%.3f "
			"frozen=%d moveDir=(%.4f,%.4f,%.4f)",
			Metadata.SourceFrame, Metadata.ViewOrigin.X, Metadata.ViewOrigin.Y, Metadata.ViewOrigin.Z,
			Metadata.ViewDirection.X, Metadata.ViewDirection.Y, Metadata.ViewDirection.Z,
			Metadata.ViewportSize.X, Metadata.ViewportSize.Y, Metadata.PixelsPerRadian,
			Metadata.PoolDepth, Metadata.PoolCapacity, Metadata.NumFaces, Metadata.MaxSubdivisionDepth,
			Metadata.ResidentCapDepth,
			Metadata.TargetTrianglePixels, Metadata.MaxTriangleEdgePixels, Metadata.TargetErrorPixels, Metadata.FrustumCullMargin,
			Metadata.TessFrustumCullMargin, Metadata.bFrozen ? 1 : 0,
			Metadata.MoveDirection.X, Metadata.MoveDirection.Y, Metadata.MoveDirection.Z);
	}

	if (LatencyEpisode.bHasSample
		&& Metadata.SourceFrame <= LatencyEpisode.LastProcessedSourceFrame)
	{
		return;
	}

	if (LatencyEpisode.bHasSample
		&& Metadata.SourceFrame != LatencyEpisode.LastProcessedSourceFrame + 1u)
	{
		LatencyEpisode.bExact = false;
		if (!bGapCoveredBeforeSample)
		{
			LatencyEpisode.SampleGaps += Metadata.SourceFrame - LatencyEpisode.LastProcessedSourceFrame - 1u;
		}
		LatencyEpisode.ConsecutiveZeroDebtFrames = 0;
	}
	LatencyEpisode.bHasSample = true;
	LatencyEpisode.LastProcessedSourceFrame = Metadata.SourceFrame;
	LatencyEpisode.DistanceCm += Metadata.MoveCm;
	LatencyEpisode.PeakTurnDegrees = FMath::Max(LatencyEpisode.PeakTurnDegrees, Metadata.TurnDegrees);
	LatencyEpisode.PeakTurnDegreesPerSecond =
		FMath::Max(LatencyEpisode.PeakTurnDegreesPerSecond, Metadata.TurnDegreesPerSecond);
	LatencyEpisode.PeakMoveCmPerSecond =
		FMath::Max(LatencyEpisode.PeakMoveCmPerSecond, Metadata.MoveCmPerSecond);
	LatencyEpisode.PeakVisibleDebt = FMath::Max(LatencyEpisode.PeakVisibleDebt, VisibleDebt);
	const uint32 CappedDebt = Counters[LatencyCounter_CappedTier0] + Counters[LatencyCounter_CappedTier1]
		+ Counters[LatencyCounter_CappedTier2] + Counters[LatencyCounter_CappedTier3];
	LatencyEpisode.PeakCappedDebt = FMath::Max(LatencyEpisode.PeakCappedDebt, CappedDebt);
	LatencyEpisode.TotalSplitApplied += Counters[LatencyCounter_SplitApplied];
	LatencyEpisode.TotalSplitDeferred += Counters[LatencyCounter_SplitDeferred];
	LatencyEpisode.TotalSplitRefused += Counters[LatencyCounter_SplitRefused];
	LatencyEpisode.TotalMotionRefineScheduledIterations += Metadata.MotionRefineScheduledIterations;
	LatencyEpisode.TotalMotionRefineApplied += Counters[LatencyCounter_MotionRefineApplied];
	LatencyEpisode.TotalMotionRefineDeferred += Counters[LatencyCounter_MotionRefineDeferred];
	LatencyEpisode.TotalMotionRefineRefused += Counters[LatencyCounter_MotionRefineRefused];
	LatencyEpisode.MinMotionRefineRequested = FMath::Min(
		LatencyEpisode.MinMotionRefineRequested, Metadata.MotionRefineRequestedIterations);
	LatencyEpisode.MaxMotionRefineRequested = FMath::Max(
		LatencyEpisode.MaxMotionRefineRequested, Metadata.MotionRefineRequestedIterations);
	LatencyEpisode.bPoolFull = LatencyEpisode.bPoolFull || Counters[LatencyCounter_PoolFull] != 0u;
	LatencyEpisode.PeakAreaRatio = FMath::Max(LatencyEpisode.PeakAreaRatio, WorstAreaRatio);
	LatencyEpisode.PeakEdgeRatio = FMath::Max(LatencyEpisode.PeakEdgeRatio, WorstEdgeRatio);
	LatencyEpisode.PeakErrorRatio = FMath::Max(LatencyEpisode.PeakErrorRatio, WorstErrorRatio);
	LatencyEpisode.PeakAreaLevelEstimate = FMath::Max(LatencyEpisode.PeakAreaLevelEstimate,
		Counters[LatencyCounter_AreaLevelEstimate]);

	const bool bAtOrAfterLatestTrigger = Metadata.SourceFrame >= LatencyEpisode.LastTriggerFrame;
	if (bAtOrAfterLatestTrigger)
	{
		LatencyEpisode.ConsecutiveZeroDebtFrames = VisibleDebt == 0u
			? LatencyEpisode.ConsecutiveZeroDebtFrames + 1u : 0u;
	}
	const uint32 QuietFrames = static_cast<uint32>(FMath::Clamp(
		CVarLatencyQuietFrames.GetValueOnRenderThread(), 1, 120));
	if (bAtOrAfterLatestTrigger && LatencyEpisode.ConsecutiveZeroDebtFrames >= QuietFrames)
	{
		FinishLatencyEpisode(TEXT("converged"), Metadata.SourceFrame, SlotIndex);
		return;
	}

	const uint32 MaxFrames = static_cast<uint32>(FMath::Clamp(
		CVarLatencyMaxFrames.GetValueOnRenderThread(), 1, 600));
	if (Metadata.SourceFrame - LatencyEpisode.StartFrame >= MaxFrames)
	{
		FinishLatencyEpisode(TEXT("timeout"), Metadata.SourceFrame, SlotIndex);
	}
}

void FArcTerrainCBTRendererExtension::MarkLatencySampleGap(uint32 SourceFrame, const TCHAR* Reason)
{
	if (!bLatencyLoggerWasEnabled || !LatencyEpisode.bActive)
	{
		return;
	}
	EnqueueLatencyGap(SourceFrame, LatencyEpisode.Id, LatencyLoggerGeneration, Reason);
}

void FArcTerrainCBTRendererExtension::EnqueueLatencyGap(uint32 SourceFrame, uint32 EpisodeId,
	uint32 LoggerGeneration, const TCHAR* Reason)
{
	if (!bLatencyLoggerWasEnabled || LoggerGeneration != LatencyLoggerGeneration
		|| !LatencyEpisode.bActive || EpisodeId != LatencyEpisode.Id)
	{
		return;
	}
	auto FinishIfPendingGapReachedTimeout = [this, SourceFrame, EpisodeId, LoggerGeneration]()
	{
		const uint32 MaxFrames = static_cast<uint32>(FMath::Clamp(
			ArcTerrainCBTRendererPrivate_9a07::CVarLatencyMaxFrames.GetValueOnRenderThread(), 1, 600));
		if (SourceFrame < LatencyEpisode.StartFrame
			|| SourceFrame - LatencyEpisode.StartFrame < MaxFrames)
		{
			return;
		}
		uint32 PendingGapCount = 0u;
		for (const FLatencyPendingGapRange& Range : PendingLatencyGapRanges)
		{
			if (Range.EpisodeId == EpisodeId && Range.LoggerGeneration == LoggerGeneration)
			{
				PendingGapCount += Range.ExplicitGapCount;
			}
		}
		LatencyEpisode.bExact = false;
		LatencyEpisode.ConsecutiveZeroDebtFrames = 0;
		LatencyEpisode.SampleGaps += PendingGapCount;
		FinishLatencyEpisode(TEXT("timeout"), SourceFrame);
	};

	for (FLatencyPendingGapRange& Range : PendingLatencyGapRanges)
	{
		if (Range.EpisodeId != EpisodeId || Range.LoggerGeneration != LoggerGeneration)
		{
			continue;
		}
		if (SourceFrame >= Range.FirstFrame && SourceFrame <= Range.LastFrame)
		{
			return;
		}
		const bool bSameReason = Range.Reason == Reason;
		if (bSameReason && SourceFrame + 1u == Range.FirstFrame)
		{
			Range.FirstFrame = SourceFrame;
			++Range.ExplicitGapCount;
			FinishIfPendingGapReachedTimeout();
			return;
		}
		if (bSameReason && Range.LastFrame != MAX_uint32 && SourceFrame == Range.LastFrame + 1u)
		{
			Range.LastFrame = SourceFrame;
			++Range.ExplicitGapCount;
			FinishIfPendingGapReachedTimeout();
			return;
		}
	}

	if (PendingLatencyGapRanges.Num() < PendingLatencyGapRangeLimit)
	{
		FLatencyPendingGapRange& Range = PendingLatencyGapRanges.AddDefaulted_GetRef();
		Range.FirstFrame = SourceFrame;
		Range.LastFrame = SourceFrame;
		Range.ExplicitGapCount = 1u;
		Range.EpisodeId = EpisodeId;
		Range.LoggerGeneration = LoggerGeneration;
		Range.Reason = Reason;
		FinishIfPendingGapReachedTimeout();
		return;
	}

	// Hard-bound fallback. Adjacent frames cannot hide a sample, so merge them even when their reasons
	// alternate. A non-adjacent overflow becomes one marker with an exact explicit-gap count; ordered
	// draining waits for every active sample through LastFrame before consuming that marker.
	for (FLatencyPendingGapRange& Range : PendingLatencyGapRanges)
	{
		if (Range.EpisodeId == EpisodeId && Range.LoggerGeneration == LoggerGeneration
			&& (SourceFrame + 1u == Range.FirstFrame
				|| (Range.LastFrame != MAX_uint32 && SourceFrame == Range.LastFrame + 1u)))
		{
			Range.FirstFrame = FMath::Min(Range.FirstFrame, SourceFrame);
			Range.LastFrame = FMath::Max(Range.LastFrame, SourceFrame);
			++Range.ExplicitGapCount;
			Range.Reason = TEXT("coalesced-gap-overflow");
			Range.bOverflowMarker = true;
			FinishIfPendingGapReachedTimeout();
			return;
		}
	}

	FLatencyPendingGapRange Marker = MoveTemp(PendingLatencyGapRanges.Last());
	PendingLatencyGapRanges.RemoveAt(PendingLatencyGapRanges.Num() - 1, 1, EAllowShrinking::No);
	Marker.FirstFrame = FMath::Min(Marker.FirstFrame, SourceFrame);
	Marker.LastFrame = FMath::Max(Marker.LastFrame, SourceFrame);
	++Marker.ExplicitGapCount;
	Marker.EpisodeId = EpisodeId;
	Marker.LoggerGeneration = LoggerGeneration;
	Marker.Reason = TEXT("coalesced-gap-overflow");
	Marker.bOverflowMarker = true;
	for (int32 RangeIndex = PendingLatencyGapRanges.Num() - 1; RangeIndex >= 0; --RangeIndex)
	{
		const FLatencyPendingGapRange& Range = PendingLatencyGapRanges[RangeIndex];
		if (Range.EpisodeId == EpisodeId && Range.LoggerGeneration == LoggerGeneration
			&& Range.FirstFrame <= Marker.LastFrame && Range.LastFrame >= Marker.FirstFrame)
		{
			Marker.FirstFrame = FMath::Min(Marker.FirstFrame, Range.FirstFrame);
			Marker.LastFrame = FMath::Max(Marker.LastFrame, Range.LastFrame);
			Marker.ExplicitGapCount += Range.ExplicitGapCount;
			PendingLatencyGapRanges.RemoveAtSwap(RangeIndex, 1, EAllowShrinking::No);
		}
	}
	PendingLatencyGapRanges.Add(MoveTemp(Marker));
	LatencyEpisode.bExact = false;
	LatencyEpisode.ConsecutiveZeroDebtFrames = 0;
	FinishIfPendingGapReachedTimeout();
}

void FArcTerrainCBTRendererExtension::ProcessLatencyGap(uint32 FirstFrame, uint32 LastFrame,
	uint32 ExplicitGapCount, uint32 EpisodeId, uint32 LoggerGeneration, const TCHAR* Reason)
{
	if (!bLatencyLoggerWasEnabled || LoggerGeneration != LatencyLoggerGeneration
		|| !LatencyEpisode.bActive || EpisodeId != LatencyEpisode.Id)
	{
		return;
	}
	if (LatencyEpisode.bHasSample && LastFrame <= LatencyEpisode.LastProcessedSourceFrame)
	{
		return;
	}
	const bool bHadSample = LatencyEpisode.bHasSample;
	LatencyEpisode.bExact = false;
	LatencyEpisode.bHasSample = true;
	LatencyEpisode.LastProcessedSourceFrame = bHadSample
		? FMath::Max(LatencyEpisode.LastProcessedSourceFrame, LastFrame) : LastFrame;
	LatencyEpisode.ConsecutiveZeroDebtFrames = 0;
	LatencyEpisode.SampleGaps += ExplicitGapCount;
	UE_LOGF(LogArcTerrainCBT, Warning,"CBTLatencyGap src=[%u,%u] count=%u ep=%u reason=%ls sampleGap=%u",
		FirstFrame, LastFrame, ExplicitGapCount, EpisodeId, Reason, LatencyEpisode.SampleGaps);
	const uint32 MaxFrames = static_cast<uint32>(FMath::Clamp(
		ArcTerrainCBTRendererPrivate_9a07::CVarLatencyMaxFrames.GetValueOnRenderThread(), 1, 600));
	if (LastFrame - LatencyEpisode.StartFrame >= MaxFrames)
	{
		FinishLatencyEpisode(TEXT("timeout"), LastFrame);
	}
}

void FArcTerrainCBTRendererExtension::FinishLatencyEpisode(const TCHAR* FinishReason,
	uint32 ReadySourceFrame, int32 DrainingSlotIndex)
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;
	if (!LatencyEpisode.bActive)
	{
		return;
	}
	const uint32 ConvergedFrames = ReadySourceFrame >= LatencyEpisode.StartFrame
		? ReadySourceFrame - LatencyEpisode.StartFrame : 0u;
	const TCHAR* Constraint = TEXT("no-visible-debt");
	if (LatencyEpisode.bPoolFull || LatencyEpisode.TotalSplitRefused > 0u)
	{
		Constraint = TEXT("pool-full");
	}
	else if (LatencyEpisode.PeakVisibleDebt > 0u && LatencyEpisode.TotalSplitApplied > 0u)
	{
		Constraint = TEXT("level-conveyor");
	}
	else if (LatencyEpisode.PeakVisibleDebt > 0u && LatencyEpisode.TotalSplitDeferred > 0u)
	{
		Constraint = TEXT("split-deferred");
	}
	else if (LatencyEpisode.PeakCappedDebt > 0u)
	{
		Constraint = TEXT("geometry-cap");
	}
	const uint8 MinMotionRefineRequested = LatencyEpisode.MinMotionRefineRequested == MAX_uint8
		? 0u : LatencyEpisode.MinMotionRefineRequested;
	UE_LOGF(LogArcTerrainCBT, Log,
		"CBTLatencySummary ep=%u cause=%ls constraint=%ls finish=%ls exact=%d start=%u ready=%u convergedIn=%u "
		"distance=%.1fcm peakTurn=%.2fdeg peakTurnSpeed=%.1fdeg/s peakMoveSpeed=%.1fcm/s "
		"peakDebt=%u peakCapped=%u splits=[%llu,%llu,%llu] peakArea=%.3fx peakEdge=%.3fx peakError=%.3fx "
		"peakAreaLevelsEstimate=%u sampleGaps=%u motionRefineRequested=[%u,%u] "
		"motionRefineScheduledTotal=%llu motionRefine=[%llu,%llu,%llu]",
		LatencyEpisode.Id, LatencyCauseName(LatencyEpisode.Cause), Constraint, FinishReason,
		LatencyEpisode.bExact ? 1 : 0, LatencyEpisode.StartFrame, ReadySourceFrame, ConvergedFrames,
		LatencyEpisode.DistanceCm, LatencyEpisode.PeakTurnDegrees,
		LatencyEpisode.PeakTurnDegreesPerSecond, LatencyEpisode.PeakMoveCmPerSecond,
		LatencyEpisode.PeakVisibleDebt, LatencyEpisode.PeakCappedDebt,
		LatencyEpisode.TotalSplitApplied, LatencyEpisode.TotalSplitDeferred,
		LatencyEpisode.TotalSplitRefused, LatencyEpisode.PeakAreaRatio, LatencyEpisode.PeakEdgeRatio,
		LatencyEpisode.PeakErrorRatio,
		LatencyEpisode.PeakAreaLevelEstimate, LatencyEpisode.SampleGaps,
		static_cast<uint32>(MinMotionRefineRequested),
		static_cast<uint32>(LatencyEpisode.MaxMotionRefineRequested),
		LatencyEpisode.TotalMotionRefineScheduledIterations,
		LatencyEpisode.TotalMotionRefineApplied, LatencyEpisode.TotalMotionRefineDeferred,
		LatencyEpisode.TotalMotionRefineRefused);

	// A summary is terminal for this episode. Its trailing GPU copies must neither log nor hold the next
	// episode behind their source frames. Do not move DrainingSlotIndex: its readback is currently locked
	// by DrainLatencyReadbacks and remains owned there until the caller unlocks it.
	for (int32 SlotIndex = 0; SlotIndex < LatencyReadbackRingSize; ++SlotIndex)
	{
		if (SlotIndex == DrainingSlotIndex)
		{
			continue;
		}
		FLatencyReadbackSlot& Slot = LatencyReadbacks[SlotIndex];
		if (!Slot.bPending || Slot.Metadata.EpisodeId != LatencyEpisode.Id
			|| Slot.Metadata.LoggerGeneration != LatencyLoggerGeneration)
		{
			continue;
		}
		if (Slot.Readback.IsValid() && Slot.Readback->IsReady())
		{
			const uint32* const Counters =
				static_cast<const uint32*>(Slot.Readback->Lock(LatencyReadbackBytes));
			if (Counters != nullptr)
			{
				Slot.Readback->Unlock();
			}
			Slot = FLatencyReadbackSlot();
		}
		else if (RetiredLatencyReadbacks.Num() < RetiredLatencyReadbackLimit)
		{
			FLatencyReadbackSlot& Retired = RetiredLatencyReadbacks.AddDefaulted_GetRef();
			Retired.Readback = MoveTemp(Slot.Readback);
			Retired.Metadata = MoveTemp(Slot.Metadata);
			Retired.bPending = true;
			Slot = FLatencyReadbackSlot();
		}
		else
		{
			// Ownership remains bounded in the active slot, but generation 0 excludes it immediately from
			// ordering and logging. The next drain retries retirement once quarantine capacity returns.
			Slot.Metadata.EpisodeId = 0u;
			Slot.Metadata.LoggerGeneration = 0u;
			bLatencyRetirementBlocked = true;
		}
	}
	PendingLatencyGapRanges.RemoveAll([this](const FLatencyPendingGapRange& Range)
	{
		return Range.EpisodeId == LatencyEpisode.Id
			&& Range.LoggerGeneration == LatencyLoggerGeneration;
	});
	LatencyEpisode = FLatencyEpisode();
}

void FArcTerrainCBTRendererExtension::PublishCounterStats() const
{
	SET_DWORD_STAT(STAT_ArcTerrainCBTLive, CounterStats.Live);
	SET_DWORD_STAT(STAT_ArcTerrainCBTPoolAllocated, CounterStats.PoolAllocated);
	SET_DWORD_STAT(STAT_ArcTerrainCBTDrawn, CounterStats.Drawn);
	SET_DWORD_STAT(STAT_ArcTerrainCBTCulled, CounterStats.Culled);
	SET_DWORD_STAT(STAT_ArcTerrainCBTSplitApplied, CounterStats.SplitApplied);
	SET_DWORD_STAT(STAT_ArcTerrainCBTMergeApplied, CounterStats.MergeApplied);
}

FArcTerrainCBTRendererExtension::FSubmittedViewMotion
FArcTerrainCBTRendererExtension::SampleSubmittedViewMotion(uint32 SourceFrame)
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;

	FSubmittedViewMotion Motion;
	// FreezeRendering deliberately measures the live camera while drawing a frozen frustum. That is useful
	// for culling diagnostics but cannot produce an honest submitted-camera motion sample.
	Motion.bFresh = MainView.bValid && !MainView.bFrozen && MainView.CaptureFrame == SourceFrame;
	if (!Motion.bFresh)
	{
		return Motion;
	}

	const double Now = FPlatformTime::Seconds();
	if (bHasLastSubmittedMotionView)
	{
		Motion.bHasDelta = true;
		const double DeltaSeconds = FMath::Max(Now - LastSubmittedMotionTime, 1.0 / 10000.0);
		const FVector CurrentDirection = MainView.ViewDirection.GetSafeNormal();
		const FVector PreviousDirection = LastSubmittedMotionView.ViewDirection.GetSafeNormal();
		const double DirectionDot = FMath::Clamp(CurrentDirection.Dot(PreviousDirection), -1.0, 1.0);
		Motion.TurnDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(DirectionDot)));
		Motion.TurnDegreesPerSecond = Motion.TurnDegrees / static_cast<float>(DeltaSeconds);

		const FVector MoveDelta = MainView.ViewOrigin - LastSubmittedMotionView.ViewOrigin;
		Motion.MoveCm = static_cast<float>(MoveDelta.Size());
		Motion.MoveDirection = Motion.MoveCm > UE_SMALL_NUMBER
			? MoveDelta / static_cast<double>(Motion.MoveCm) : FVector::ZeroVector;
		Motion.MoveCmPerSecond = Motion.MoveCm / static_cast<float>(DeltaSeconds);
		Motion.ForwardSpeedCmPerSecond =
			static_cast<float>(MoveDelta.Dot(CurrentDirection) / DeltaSeconds);
		const FVector LateralDelta = MoveDelta - CurrentDirection * MoveDelta.Dot(CurrentDirection);
		Motion.LateralSpeedCmPerSecond = static_cast<float>(LateralDelta.Size() / DeltaSeconds);

		const float TurnDegreesThreshold = FMath::Max(
			CVarLatencyTurnDegrees.GetValueOnRenderThread(), 0.0f);
		const float TurnSpeedThreshold = FMath::Max(
			CVarLatencyTurnDegreesPerSecond.GetValueOnRenderThread(), 0.0f);
		const float MoveCmThreshold = FMath::Max(CVarLatencyMoveCm.GetValueOnRenderThread(), 0.0f);
		const float MoveSpeedThreshold = FMath::Max(
			CVarLatencyMoveCmPerSecond.GetValueOnRenderThread(), 0.0f);
		const bool bTurnTrigger = Motion.TurnDegrees >= TurnDegreesThreshold
			|| Motion.TurnDegreesPerSecond >= TurnSpeedThreshold;
		const bool bTeleportTrigger = Motion.MoveCm >= MoveCmThreshold;
		const bool bMoveTrigger = bTeleportTrigger || Motion.MoveCmPerSecond >= MoveSpeedThreshold;
		Motion.Cause = (bTurnTrigger ? LatencyCause_Turn : 0u)
			| (bMoveTrigger ? LatencyCause_Move : 0u)
			| (bTeleportTrigger ? LatencyCause_Teleport : 0u);
	}

	LastSubmittedMotionView = MainView;
	LastSubmittedMotionTime = Now;
	bHasLastSubmittedMotionView = true;
	return Motion;
}

void FArcTerrainCBTRendererExtension::DrainCounterReadback()
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;
	if (!bCounterReadbackPending || !CounterReadback.IsValid() || !CounterReadback->IsReady())
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_ArcTerrainCBTReadbackDrain);
	const uint32* const Counters = static_cast<const uint32*>(CounterReadback->Lock(CounterReadbackBytes));
	if (Counters != nullptr)
	{
		CounterStats.Live = Counters[Counter_LiveBisectors];
		CounterStats.PoolAllocated = Counters[Counter_AllocCursor];
		CounterStats.Drawn = Counters[Counter_DrawnBisectors];
		CounterStats.Culled = Counters[Counter_FrustumCulled] + Counters[Counter_DistanceCulled];
		CounterStats.DistanceCull = PendingCounterDistanceCull;
		CounterStats.SplitApplied = Counters[Counter_SplitApplied];
		CounterStats.MergeApplied = Counters[Counter_MergeApplied];

		const bool bLogStats = CVarStats.GetValueOnRenderThread() > 0;
		const bool bLogProbe = CVarProbePredicate.GetValueOnRenderThread() > 0;
		if (bLogStats || bLogProbe)
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - LastStatsLogTime >= StatsLogIntervalSeconds)
			{
				LastStatsLogTime = Now;
				if (bLogStats)
				{
					LogStats(Counters, Buffers.GetNumFaces(),
						ArcTerrain::ResolveMaxSubdivisionDepth(ActiveBaseMesh.FaceQuads));
				}
				if (bLogProbe)
				{
					LogProbe(Counters);
				}
			}
		}

		if ((Counters[Counter_AllocRefused] != 0u || Counters[Counter_SplitPoolFull] != 0u)
			&& !bWarnedPoolFull)
		{
			bWarnedPoolFull = true;
			UE_LOGF(LogArcTerrainCBT, Warning,
				"Bisector pool saturated (%u split refusals; classify gated: %u): refinement stops "
				"and the terrain stays COARSER there — no geometry is dropped, and the collapse "
				"path keeps running so the pool recovers on its own. The metric is asking for more "
				"leaves than the pool holds: raise the terrain's TargetTrianglePixels or lower its "
				"FaceQuadsShift before reaching for CBTPoolDepth.",
				Counters[Counter_AllocRefused], Counters[Counter_SplitPoolFull]);
		}
		if (Counters[Counter_NonMutualRefs] != 0u && !bWarnedTwinValidation)
		{
			bWarnedTwinValidation = true;
			UE_LOGF(LogArcTerrainCBT, Error,
				"arc.Terrain.CBT.Validate: %u halfedge references are not mutual — the twin "
				"structure is corrupt and seams will crack.", Counters[Counter_NonMutualRefs]);
		}
	}
	CounterReadback->Unlock();
	bCounterReadbackPending = false;
}

bool FArcTerrainCBTRendererExtension::SubmitTopologyUpdate(
	FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags)
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;
	SCOPE_CYCLE_COUNTER(STAT_ArcTerrainCBTSubmit);
	RDG_EVENT_SCOPE_STAT(GraphBuilder, ArcTerrainCBT, "ArcTerrainCBT");
	const uint32 SourceFrame = GFrameNumberRenderThread;
	if (CVarLatencyLog.GetValueOnRenderThread() > 0 && !bWarnedAsyncLatencyLog)
	{
		bWarnedAsyncLatencyLog = true;
		UE_LOGF(LogArcTerrainCBT, Warning,"arc.Terrain.CBT.LatencyLog is unavailable with the cross-frame async CBT pipeline; no GPU join is inserted.");
	}
	const bool bLatencyLoggingEnabled = false;
	if (bLatencyLoggerWasEnabled && !bLatencyLoggingEnabled)
	{
		++LatencyLoggerGeneration;
		PendingLatencyGapRanges.Reset();
		LatencyEpisode = FLatencyEpisode();
		bLatencyMotionArmed = false;
	}
	bLatencyLoggerWasEnabled = bLatencyLoggingEnabled;
	DrainLatencyReadbacks(SourceFrame, bLatencyLoggingEnabled);
	if (!bLatencyLoggingEnabled)
	{
		LatencyEpisode = FLatencyEpisode();
		bLatencyMotionArmed = false;
	}

	FRHITexture* const HeightAtlasRHI = ActiveBaseMesh.AtlasResources->HeightAtlas.IsValid()
		? ActiveBaseMesh.AtlasResources->HeightAtlas->GetRHI() : nullptr;
	if (HeightAtlasRHI == nullptr)
	{
		return false; // the atlas has not been filled yet; nothing to measure against
	}
	if (!bLoggedFirstSubmit && AcceptedIdentity.IsValid())
	{
		uint64 ProxyAccepted = 0;
		uint64 ProxyRejected = 0;
		GetProxyDiagnosticCounts_RenderThread(ProxyAccepted, ProxyRejected);
		const FArcTerrainRenderIdentity& PacketIdentity = ActiveBaseMesh.Identity;
		UE_LOGF(LogArcTerrainCBT, Log,"[CBTOwner] first renderer submit packet token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p; accepted token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p; proxyaccepted=%llu proxyrejected=%llu",
			PacketIdentity.Owner.Key.WorldInstanceToken, PacketIdentity.Owner.Key.TerrainInstanceId,
			PacketIdentity.Owner.OwnerEpoch, PacketIdentity.ResourceRevision, PacketIdentity.RhiEpoch,
			static_cast<int32>(PacketIdentity.WorldType), PacketIdentity.SceneKey,
			AcceptedIdentity.Owner.Key.WorldInstanceToken, AcceptedIdentity.Owner.Key.TerrainInstanceId,
			AcceptedIdentity.Owner.OwnerEpoch, AcceptedIdentity.ResourceRevision, AcceptedIdentity.RhiEpoch,
			static_cast<int32>(AcceptedIdentity.WorldType), AcceptedIdentity.SceneKey,
			ProxyAccepted, ProxyRejected);
		bLoggedFirstSubmit = true;
	}
	const uint32 MergePhaseCount = ResolveMergePhaseCount();
	const uint32 MergeFramePhase = SourceFrame & (MergePhaseCount - 1u);

	const int32 PoolDepth = Buffers.GetPoolDepth();
	const uint32 PoolCapacity = Buffers.GetPoolCapacity();
	const int32 NumFaces = Buffers.GetNumFaces();
	const int32 MaxSubdivisionDepth = ArcTerrain::ResolveMaxSubdivisionDepth(ActiveBaseMesh.FaceQuads);
	FGlobalShaderMap* const ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	const bool bBoundedTopologyWork = CVarBoundedTopologyWork.GetValueOnRenderThread() > 0
		&& IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::SM5);
	const FSubmittedViewMotion SubmittedViewMotion = SampleSubmittedViewMotion(SourceFrame);
	const bool bBulkRefineSubmittedThisFrame = bRefinePending && MainView.bValid;
	const int32 MotionRefineRequestedIterations = FMath::Clamp(
		CVarMotionRefineIterations.GetValueOnRenderThread(), 0, 4);
	const int32 MotionRefineScheduledIterations =
		SubmittedViewMotion.Cause != 0u && !bBulkRefineSubmittedThisFrame
			? MotionRefineRequestedIterations : 0;
	int32 LatencyReadbackSlotIndex = INDEX_NONE;

	if (bLatencyLoggingEnabled && SubmittedViewMotion.bFresh)
	{
		const bool bCanTriggerLatencyEpisode = bLatencyMotionArmed && SubmittedViewMotion.bHasDelta;
		bLatencyMotionArmed = true;
		if (bCanTriggerLatencyEpisode && SubmittedViewMotion.Cause != 0u)
		{
			if (!LatencyEpisode.bActive)
			{
				LatencyEpisode = FLatencyEpisode();
				LatencyEpisode.bActive = true;
				LatencyEpisode.Id = NextLatencyEpisodeId++;
				LatencyEpisode.StartFrame = SourceFrame;
			}
			LatencyEpisode.Cause |= SubmittedViewMotion.Cause;
			LatencyEpisode.LastTriggerFrame = SourceFrame;
			// A newer camera motion invalidates any quiet run accumulated for the older view. Delayed
			// readbacks from before this trigger may still contribute diagnostics, but cannot converge it.
			LatencyEpisode.ConsecutiveZeroDebtFrames = 0u;
		}
	}
	else if (bLatencyLoggingEnabled && LatencyEpisode.bActive)
	{
		// The sticky view may keep tessellation alive for an orthographic/capture-only family, but it is not
		// the same submitted camera and cannot be inserted into an exact motion/convergence sequence.
		MarkLatencySampleGap(SourceFrame, TEXT("no-fresh-main-view"));
	}

	if (bLatencyLoggingEnabled && LatencyEpisode.bActive && SubmittedViewMotion.bFresh)
	{
		for (int32 Offset = 0; Offset < LatencyReadbackRingSize; ++Offset)
		{
			const int32 Candidate = (NextLatencyReadbackSlot + Offset) % LatencyReadbackRingSize;
			if (!LatencyReadbacks[Candidate].bPending)
			{
				LatencyReadbackSlotIndex = Candidate;
				NextLatencyReadbackSlot = (Candidate + 1) % LatencyReadbackRingSize;
				break;
			}
		}
		if (LatencyReadbackSlotIndex == INDEX_NONE)
		{
			MarkLatencySampleGap(SourceFrame, TEXT("readback-ring-full"));
		}
	}
	const bool bLatencyAuditThisFrame = LatencyReadbackSlotIndex != INDEX_NONE;

	if (GPUTimingMode != 0 && !bWarnedAsyncGPUTiming)
	{
		bWarnedAsyncGPUTiming = true;
		UE_LOGF(LogArcTerrainCBT, Warning,"arc.Terrain.CBT.GPUTiming is unavailable with the cross-frame async CBT pipeline; no GPU join is inserted.");
	}
	const uint8 TimingMode = 0;
	const uint8 TimingBoundaryCount = TimingMode == 2
		? GPUTimingMode2BoundaryCount : GPUTimingMode1BoundaryCount;
	const bool bShaderStats = CVarStats.GetValueOnRenderThread() > 0;
	const int32 GPUTimingSlotIndex = AcquireGPUTimingSlot(
		SourceFrame, TimingMode, TimingBoundaryCount,
		bShaderStats,
		bBulkRefineSubmittedThisFrame,
		static_cast<uint8>(MotionRefineScheduledIterations),
		TimingMode == 2 && bBoundedTopologyWork && bShaderStats);
	const bool bStageGPUTiming = GPUTimingSlotIndex != INDEX_NONE && TimingMode == 2;
	const bool bInlineDispatchGPUTiming = bStageGPUTiming && bBoundedTopologyWork && bShaderStats;
	AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
		static_cast<int32>(EGPUTimingBoundary::Begin), TEXT("Begin"));

	FRDGBufferRef HeapRDG = Buffers.RegisterHeap(GraphBuilder);
	FRDGBufferRef BisectorRDG = Buffers.RegisterBisectors(GraphBuilder);
	FRDGBufferRef NeighborRDG = Buffers.RegisterNeighbors(GraphBuilder);
	FRDGBufferRef NodeCounterRDG = Buffers.RegisterNodeCounter(GraphBuilder);
	FRDGBufferRef PoolCountersRDG = Buffers.RegisterPoolCounters(GraphBuilder);
	FRDGBufferRef FaceLiveCountRDG = Buffers.RegisterFaceLiveCount(GraphBuilder);
	FRDGBufferRef FaceRootsRDG = Buffers.RegisterFaceRoots(GraphBuilder);
	FRDGBufferRef DrawArgsRDG = Buffers.RegisterDrawArgs(GraphBuilder);
	FRDGBufferRef LinkArgsRDG = Buffers.RegisterLinkArgs(GraphBuilder);
	FRDGBufferRef PromoteRDG = Buffers.RegisterPromote(GraphBuilder);
	GraphBuilder.UseInternalAccessMode(HeapRDG);
	GraphBuilder.UseInternalAccessMode(BisectorRDG);
	GraphBuilder.UseInternalAccessMode(DrawArgsRDG);
	FRDGBufferRef LatencyCountersRDG = nullptr;
	FRDGBufferUAVRef LatencyCountersUAV = nullptr;
	if (bLatencyAuditThisFrame)
	{
		LatencyCountersRDG = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), LatencyCounter_Num),
			TEXT("ArcTerrainCBT.LatencyCounters"));
		LatencyCountersUAV =
			GraphBuilder.CreateUAV(FRDGBufferUAVDesc(LatencyCountersRDG, PF_R32_UINT));
		AddClearUAVPass(GraphBuilder, LatencyCountersUAV, 0u, ComputePassFlags);
	}

	if (bPoolInitPending)
	{
		check(ComputePassFlags == ERDGPassFlags::Compute);
		bPoolInitPending = false;
		Buffers.InitializePool(GraphBuilder);
	}

	// The one per-face input left, and it is static: the grid's own adjacency.
	FRDGBufferRef FaceNeighborsRDG = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumFaces * 4), TEXT("ArcTerrainCBT.FaceNeighbors"));
	GraphBuilder.QueueBufferUpload(FaceNeighborsRDG, MakeArrayView(ActiveBaseMesh.Neighbors));
	FRDGBufferSRVRef FaceNeighborsSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(FaceNeighborsRDG, PF_R32_UINT));

	const FArcTerrainTessPredicateResources TessResources =
		CreateTessPredicateResources(GraphBuilder, ActiveBaseMesh);

	// 1. Dispatch sizing from the published live count + this frame's allocation cursor reset.
	FRDGBufferRef DispatchArgsRDG = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc(3u), TEXT("ArcTerrainCBT.UpdateArgs"));
	{
		FArcTerrainCBTDispatchArgsCS::FParameters* Parameters =
			GraphBuilder.AllocParameters<FArcTerrainCBTDispatchArgsCS::FParameters>();
		Parameters->NodeCounter = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(NodeCounterRDG, PF_R32_UINT));
		Parameters->RWDispatchArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(DispatchArgsRDG, PF_R32_UINT));
		Parameters->PoolCounters = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
		Parameters->ThreadsPerGroup = static_cast<uint32>(ClassifyThreads);

		TShaderMapRef<FArcTerrainCBTDispatchArgsCS> Shader(ShaderMap);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.DispatchArgs"),
			ComputePassFlags, Shader, Parameters, FIntVector(1, 1, 1));
	}

	// 2. The single frozen snapshot of state N. Classify, split/merge and the base mesh passes all decide
	//    against it, and it is what makes the allocator safe: a slot released this frame is still marked
	//    used here.
	FRDGBufferRef SnapshotRDG = GraphBuilder.CreateBuffer(HeapRDG->Desc, TEXT("ArcTerrainCBT.HeapSnapshot"));
	AddCopyBufferPass(GraphBuilder, SnapshotRDG, HeapRDG);
	FRDGBufferSRVRef SnapshotSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(SnapshotRDG));

	FRDGBufferRef CommandsRDG = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), static_cast<int32>(PoolCapacity)),
		TEXT("ArcTerrainCBT.Commands"));
	FRDGBufferUAVRef CommandsUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(CommandsRDG, PF_R32_UINT));
	AddClearUAVPass(GraphBuilder, CommandsUAV, 0u, ComputePassFlags);

	// Which edge a slot a merge kept for its parent now presents where the even child's hypotenuse was.
	// Per-frame: a stale entry would move a legitimate hypotenuse reference off the parent's own twin.
	FRDGBufferUAVRef PromoteUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PromoteRDG, PF_R32_UINT));
	AddClearUAVPass(GraphBuilder, PromoteUAV, 0u, ComputePassFlags);
	FTopologyWorkBudgets TopologyBudgets;
	FTopologyWorkResources TopologyResources;
	if (bBoundedTopologyWork)
	{
		TopologyBudgets = ResolveTopologyWorkBudgets(PoolCapacity);
		TopologyResources = CreateTopologyWorkResources(
			GraphBuilder, PoolCapacity, TopologyBudgets, ComputePassFlags);
	}
	if (bStageGPUTiming)
	{
		AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
			static_cast<int32>(EGPUTimingBoundary::PreworkEnd), TEXT("PreworkEnd"));
	}

	// 3. Classify: one vote per live bisector. Without a main view there is nothing to measure against,
	//    so the vote is skipped and every command stays KEEP — the tree holds its triangulation while
	//    the maintenance passes below still run.
	if (MainView.bValid)
	{
		SCOPE_CYCLE_COUNTER(STAT_ArcTerrainCBTClassifySetup);
		FArcTerrainCBTClassifyCS::FParameters* Parameters =
			GraphBuilder.AllocParameters<FArcTerrainCBTClassifyCS::FParameters>();
		Parameters->CBTHeapSnapshot = SnapshotSRV;
		Parameters->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
		Parameters->BisectorNeighbors = GraphBuilder.CreateUAV(NeighborRDG);
		Parameters->BisectorCommands = CommandsUAV;
		Parameters->PoolCounters = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
		if (bBoundedTopologyWork)
		{
			Parameters->RawSplitRequests = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.RawSplitRequests, PF_R32_UINT));
			Parameters->SplitPriorityBySlot = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.SplitPriorityBySlot, PF_R32_UINT));
			Parameters->WorkCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WorkCounters, PF_R32_UINT));
		}
		Parameters->PoolDepth = static_cast<uint32>(PoolDepth);
		Parameters->PoolCapacity = PoolCapacity;
		Parameters->MaxSubdivisionDepth = static_cast<uint32>(MaxSubdivisionDepth);
		Parameters->StatsEnabled = CVarStats.GetValueOnRenderThread() > 0 ? 1u : 0u;
		FillTessPredicateParameters(ActiveBaseMesh, MainView, TessResources, EClipSource::Live,
			Parameters->TessPredicate);
		Parameters->IndirectArgs = DispatchArgsRDG;
		LastEyeLocalToClip = Parameters->TessPredicate.EyeLocalToClip;

		FArcTerrainCBTClassifyCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FArcTerrainCBTClassifyCS::FProbePredicateDim>(
			CVarProbePredicate.GetValueOnRenderThread() > 0);
		PermutationVector.Set<FArcTerrainCBTClassifyCS::FBoundedTopologyWorkDim>(bBoundedTopologyWork);
		TShaderMapRef<FArcTerrainCBTClassifyCS> Shader(ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.Classify"),
			ComputePassFlags, Shader, Parameters, DispatchArgsRDG, 0u);
	}
	else
	{
		// Frozen-but-drawing is the failure mode that looks like success: the tree holds whatever
		// triangulation it had and every other pass keeps running. Throttled, because the condition
		// persists for as long as no main view arrives.
		const double Now = FPlatformTime::Seconds();
		if (Now - LastNoMainViewWarnTime >= NoMainViewWarnIntervalSeconds)
		{
			LastNoMainViewWarnTime = Now;
			UE_LOGF(LogArcTerrainCBT, Warning,
				"No main view captured yet — the bisector tree is HOLDING its current triangulation, "
				"which before the first refine is the two depth-1 triangles per face the base mesh "
				"passes root every frame (terrain draws, flat and coarse, but tessellation does not "
				"follow the camera). Expected before the first rendered frame of a non-capture view "
				"family, and permanent in a family whose only view is orthographic.");
		}
	}
	if (bStageGPUTiming)
	{
		AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
			static_cast<int32>(EGPUTimingBoundary::ClassifyEnd), TEXT("ClassifyEnd"));
	}

	// 4. Apply. The branch is fixed before bounded resources are created, so exactly one topology writer
	//    owns the frame. Both paths keep stage and commit separate because a surviving even child is still
	//    a decision input until every selected diamond has staged.
	{
		const uint32 StatsEnabled = CVarStats.GetValueOnRenderThread() > 0 ? 1u : 0u;
		const uint32 SelectedArgsOffset = 9u * sizeof(uint32);
		if (!bBoundedTopologyWork)
		{
			TFunction<FArcTerrainCBTSplitMergeParameters*()> MakeParameters =
				[&]() -> FArcTerrainCBTSplitMergeParameters*
				{
					FArcTerrainCBTSplitMergeParameters* Out =
						GraphBuilder.AllocParameters<FArcTerrainCBTSplitMergeParameters>();
					Out->CBTHeapSnapshot = SnapshotSRV;
					Out->CBTHeap = GraphBuilder.CreateUAV(HeapRDG);
					Out->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
					Out->BisectorNeighbors = GraphBuilder.CreateUAV(NeighborRDG);
					Out->BisectorCommands = CommandsUAV;
					Out->BisectorPromote = PromoteUAV;
					Out->PoolCounters = GraphBuilder.CreateUAV(
						FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
					Out->LatencyCounters = LatencyCountersUAV;
					Out->FaceLiveCount = GraphBuilder.CreateUAV(
						FRDGBufferUAVDesc(FaceLiveCountRDG, PF_R32_UINT));
					Out->FaceRoots = GraphBuilder.CreateUAV(
						FRDGBufferUAVDesc(FaceRootsRDG, PF_R32_UINT));
					Out->PoolDepth = static_cast<uint32>(PoolDepth);
					Out->PoolCapacity = PoolCapacity;
					Out->NumFaces = static_cast<uint32>(NumFaces);
					Out->MaxSubdivisionDepth = static_cast<uint32>(MaxSubdivisionDepth);
					Out->MergePhaseCount = MergePhaseCount;
					Out->MergeFramePhase = MergeFramePhase;
					Out->StatsEnabled = StatsEnabled;
					Out->IndirectArgs = DispatchArgsRDG;
					return Out;
				};

			FArcTerrainCBTSplitMergeCS::FPermutationDomain SplitPermutation;
			SplitPermutation.Set<FArcTerrainCBTSplitMergeCS::FLatencyAuditDim>(bLatencyAuditThisFrame);
			TShaderMapRef<FArcTerrainCBTSplitMergeCS> Shader(ShaderMap, SplitPermutation);
			if (bStageGPUTiming)
			{
				AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
					static_cast<int32>(EGPUTimingBoundary::MergeSelectEnd), TEXT("MergeSelectEnd"));
			}
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.SplitMerge"),
				ComputePassFlags, Shader, MakeParameters(), DispatchArgsRDG, 0u);
			if (bStageGPUTiming)
			{
				AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
					static_cast<int32>(EGPUTimingBoundary::SplitApplyEnd), TEXT("SplitApplyEnd"));
				AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
					static_cast<int32>(EGPUTimingBoundary::MergeStageEnd), TEXT("MergeStageEnd"));
			}

			FArcTerrainCBTMergeCommitCS::FPermutationDomain CommitPermutation;
			CommitPermutation.Set<FArcTerrainCBTMergeCommitCS::FLatencyAuditDim>(false);
			TShaderMapRef<FArcTerrainCBTMergeCommitCS> CommitShader(ShaderMap, CommitPermutation);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.MergeCommit"),
				ComputePassFlags, CommitShader, MakeParameters(), DispatchArgsRDG, 0u);
		}
		else
		{
			const uint32 RawSplitArgsOffset = 3u * sizeof(uint32);
			const uint32 SplitArgsOffset = 6u * sizeof(uint32);
			const uint32 ClaimAuditArgsOffset = 12u * sizeof(uint32);

			FArcTerrainCBTSplitRawArgsParameters* RawArgsParameters =
				GraphBuilder.AllocParameters<FArcTerrainCBTSplitRawArgsParameters>();
			RawArgsParameters->WorkCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WorkCounters, PF_R32_UINT));
			RawArgsParameters->WorkIndirectArgs = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.IndirectArgs, PF_R32_UINT));
			RawArgsParameters->PoolCapacity = PoolCapacity;
			TShaderMapRef<FArcTerrainCBTSplitRawArgsCS> RawArgsShader(ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.SplitRawArgs"),
				ComputePassFlags, RawArgsShader, RawArgsParameters, FIntVector(1, 1, 1));

			FArcTerrainCBTMergeWindowArgsParameters* WindowParameters =
				GraphBuilder.AllocParameters<FArcTerrainCBTMergeWindowArgsParameters>();
			WindowParameters->CBTHeapSnapshot = SnapshotSRV;
			WindowParameters->PoolCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
			WindowParameters->WindowMetadata = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WindowMetadata, PF_R32_UINT));
			WindowParameters->WorkIndirectArgs = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.IndirectArgs, PF_R32_UINT));
			WindowParameters->MergeScanCursor = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.MergeScanCursor, PF_R32_UINT));
			WindowParameters->NormalScanBudget = TopologyBudgets.NormalScanBudget;
			WindowParameters->PressureScanBudget = TopologyBudgets.PressureScanBudget;
			WindowParameters->PoolDepth = static_cast<uint32>(PoolDepth);
			WindowParameters->PoolCapacity = PoolCapacity;
			WindowParameters->StatsEnabled = StatsEnabled;
			TShaderMapRef<FArcTerrainCBTMergeWindowArgsCS> WindowShader(ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.MergeWindowArgs"),
				ComputePassFlags, WindowShader, WindowParameters, FIntVector(1, 1, 1));

			FArcTerrainCBTMergeSelectParameters* SelectParameters =
				GraphBuilder.AllocParameters<FArcTerrainCBTMergeSelectParameters>();
			SelectParameters->CBTHeapSnapshot = SnapshotSRV;
			SelectParameters->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
			SelectParameters->BisectorNeighbors = GraphBuilder.CreateUAV(NeighborRDG);
			SelectParameters->BisectorCommands = CommandsUAV;
			SelectParameters->PoolCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
			SelectParameters->SelectedDiamonds = GraphBuilder.CreateUAV(TopologyResources.SelectedDiamonds);
			SelectParameters->WorkCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WorkCounters, PF_R32_UINT));
			SelectParameters->WindowMetadata = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WindowMetadata, PF_R32_UINT));
			SelectParameters->SelectedCapacity = TopologyBudgets.SelectedCapacity;
			SelectParameters->PoolDepth = static_cast<uint32>(PoolDepth);
			SelectParameters->PoolCapacity = PoolCapacity;
			SelectParameters->StatsEnabled = StatsEnabled;
			SelectParameters->IndirectArgs = TopologyResources.IndirectArgs;
			TShaderMapRef<FArcTerrainCBTMergeSelectCS> SelectShader(ShaderMap);
			if (bInlineDispatchGPUTiming)
			{
				FGPUTimingSlot& TimingSlot = GPUTimingSlots[GPUTimingSlotIndex];
				const int32 QueryIndex =
					static_cast<int32>(EGPUTimingInlineDispatch::MergeSelect) * 2;
				AddInlineTimedIndirectComputePass(GraphBuilder,
					RDG_EVENT_NAME("ArcTerrainCBT.MergeSelect"), SelectShader, SelectParameters,
					TopologyResources.IndirectArgs, 0u,
					TimingSlot.InlineQueries[QueryIndex].GetQuery(),
					TimingSlot.InlineQueries[QueryIndex + 1].GetQuery());
			}
			else
			{
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.MergeSelect"),
					ComputePassFlags, SelectShader, SelectParameters, TopologyResources.IndirectArgs, 0u);
			}
			if (bStageGPUTiming)
			{
				AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
					static_cast<int32>(EGPUTimingBoundary::MergeSelectEnd), TEXT("MergeSelectEnd"));
			}

			for (uint32 Priority = 1u; Priority <= SplitPriorityBucketCount; ++Priority)
			{
				FArcTerrainCBTSplitAdmissionParameters* AdmissionParameters =
					GraphBuilder.AllocParameters<FArcTerrainCBTSplitAdmissionParameters>();
				AdmissionParameters->CBTHeapSnapshot = SnapshotSRV;
				AdmissionParameters->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
				AdmissionParameters->BisectorNeighbors = GraphBuilder.CreateUAV(NeighborRDG);
				AdmissionParameters->BisectorCommands = CommandsUAV;
				AdmissionParameters->PoolCounters = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
				AdmissionParameters->RawSplitRequests = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(TopologyResources.RawSplitRequests, PF_R32_UINT));
				AdmissionParameters->SplitPriorityBySlot = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(TopologyResources.SplitPriorityBySlot, PF_R32_UINT));
				AdmissionParameters->SplitBucketCandidates = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(TopologyResources.SplitBucketCandidates, PF_R32_UINT));
				AdmissionParameters->SplitBucketCounts = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(TopologyResources.SplitBucketCounts, PF_R32_UINT));
				AdmissionParameters->WorkCounters = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(TopologyResources.WorkCounters, PF_R32_UINT));
				AdmissionParameters->SplitApplyBudget = TopologyBudgets.SplitApplyBudget;
				AdmissionParameters->PriorityBucket = Priority;
				AdmissionParameters->PoolDepth = static_cast<uint32>(PoolDepth);
				AdmissionParameters->PoolCapacity = PoolCapacity;
				AdmissionParameters->StatsEnabled = StatsEnabled;
				AdmissionParameters->IndirectArgs = TopologyResources.IndirectArgs;
				TShaderMapRef<FArcTerrainCBTSplitAdmissionCS> AdmissionShader(ShaderMap);
				if (bInlineDispatchGPUTiming)
				{
					FGPUTimingSlot& TimingSlot = GPUTimingSlots[GPUTimingSlotIndex];
					const int32 DispatchIndex =
						static_cast<int32>(EGPUTimingInlineDispatch::SplitAdmissionPriority1)
						+ static_cast<int32>(Priority - 1u);
					const int32 QueryIndex = DispatchIndex * 2;
					AddInlineTimedIndirectComputePass(GraphBuilder,
						RDG_EVENT_NAME("ArcTerrainCBT.SplitAdmission(Priority=%u)", Priority),
						AdmissionShader, AdmissionParameters, TopologyResources.IndirectArgs,
						RawSplitArgsOffset, TimingSlot.InlineQueries[QueryIndex].GetQuery(),
						TimingSlot.InlineQueries[QueryIndex + 1].GetQuery());
				}
				else
				{
					FComputeShaderUtils::AddPass(GraphBuilder,
						RDG_EVENT_NAME("ArcTerrainCBT.SplitAdmission(Priority=%u)", Priority),
						ComputePassFlags, AdmissionShader, AdmissionParameters, TopologyResources.IndirectArgs,
						RawSplitArgsOffset);
				}
			}

			FArcTerrainCBTSplitFinalizeArgsParameters* FinalizeParameters =
				GraphBuilder.AllocParameters<FArcTerrainCBTSplitFinalizeArgsParameters>();
			FinalizeParameters->CBTHeapSnapshot = SnapshotSRV;
			FinalizeParameters->PoolCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
			FinalizeParameters->SplitBucketCandidates = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.SplitBucketCandidates, PF_R32_UINT));
			FinalizeParameters->SplitBucketCounts = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.SplitBucketCounts, PF_R32_UINT));
			FinalizeParameters->AdmittedSplitRequests = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.AdmittedSplitRequests, PF_R32_UINT));
			FinalizeParameters->WorkCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WorkCounters, PF_R32_UINT));
			FinalizeParameters->WindowMetadata = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WindowMetadata, PF_R32_UINT));
			FinalizeParameters->WorkIndirectArgs = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.IndirectArgs, PF_R32_UINT));
			FinalizeParameters->MergeScanCursor = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.MergeScanCursor, PF_R32_UINT));
			FinalizeParameters->SelectedCapacity = TopologyBudgets.SelectedCapacity;
			FinalizeParameters->SplitApplyBudget = TopologyBudgets.SplitApplyBudget;
			FinalizeParameters->PoolDepth = static_cast<uint32>(PoolDepth);
			FinalizeParameters->PoolCapacity = PoolCapacity;
			FinalizeParameters->StatsEnabled = StatsEnabled;
			TShaderMapRef<FArcTerrainCBTSplitFinalizeArgsCS> FinalizeShader(ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.SplitFinalizeArgs"),
				ComputePassFlags, FinalizeShader, FinalizeParameters, FIntVector(1, 1, 1));

			for (uint32 Priority = SplitPriorityBucketCount; Priority >= 1u; --Priority)
			{
				FArcTerrainCBTSplitPrepareParameters* PrepareParameters =
					GraphBuilder.AllocParameters<FArcTerrainCBTSplitPrepareParameters>();
				PrepareParameters->CBTHeapSnapshot = SnapshotSRV;
				PrepareParameters->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
				PrepareParameters->BisectorNeighbors = GraphBuilder.CreateUAV(NeighborRDG);
				PrepareParameters->BisectorCommands = CommandsUAV;
				PrepareParameters->PoolCounters = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
				PrepareParameters->LatencyCounters = LatencyCountersUAV;
				PrepareParameters->AdmittedSplitRequests = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(TopologyResources.AdmittedSplitRequests, PF_R32_UINT));
				PrepareParameters->SplitPriorityBySlot = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(TopologyResources.SplitPriorityBySlot, PF_R32_UINT));
				PrepareParameters->PreparedSplitDescriptors =
					GraphBuilder.CreateUAV(TopologyResources.PreparedSplitDescriptors);
				PrepareParameters->WorkCounters = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(TopologyResources.WorkCounters, PF_R32_UINT));
				PrepareParameters->SplitApplyBudget = TopologyBudgets.SplitApplyBudget;
				PrepareParameters->PriorityBucket = Priority;
				PrepareParameters->PoolDepth = static_cast<uint32>(PoolDepth);
				PrepareParameters->PoolCapacity = PoolCapacity;
				PrepareParameters->NumFaces = static_cast<uint32>(NumFaces);
				PrepareParameters->MaxSubdivisionDepth = static_cast<uint32>(MaxSubdivisionDepth);
				PrepareParameters->MergePhaseCount = 1u;
				PrepareParameters->MergeFramePhase = 0u;
				PrepareParameters->StatsEnabled = StatsEnabled;
				PrepareParameters->IndirectArgs = TopologyResources.IndirectArgs;
				FArcTerrainCBTSplitPrepareCS::FPermutationDomain PreparePermutation;
				PreparePermutation.Set<FArcTerrainCBTSplitPrepareCS::FLatencyAuditDim>(
					bLatencyAuditThisFrame);
				TShaderMapRef<FArcTerrainCBTSplitPrepareCS> PrepareShader(ShaderMap, PreparePermutation);
				if (bInlineDispatchGPUTiming)
				{
					FGPUTimingSlot& TimingSlot = GPUTimingSlots[GPUTimingSlotIndex];
					const int32 DispatchIndex =
						static_cast<int32>(EGPUTimingInlineDispatch::SplitPreparePriority6)
						+ static_cast<int32>(SplitPriorityBucketCount - Priority);
					const int32 QueryIndex = DispatchIndex * 2;
					AddInlineTimedIndirectComputePass(GraphBuilder,
						RDG_EVENT_NAME("ArcTerrainCBT.SplitPrepare(Priority=%u)", Priority),
						PrepareShader, PrepareParameters, TopologyResources.IndirectArgs,
						SplitArgsOffset, TimingSlot.InlineQueries[QueryIndex].GetQuery(),
						TimingSlot.InlineQueries[QueryIndex + 1].GetQuery());
				}
				else
				{
					FComputeShaderUtils::AddPass(GraphBuilder,
						RDG_EVENT_NAME("ArcTerrainCBT.SplitPrepare(Priority=%u)", Priority),
						ComputePassFlags, PrepareShader, PrepareParameters, TopologyResources.IndirectArgs,
						SplitArgsOffset);
				}
			}

			FArcTerrainCBTSplitReserveParameters* ReserveParameters =
				GraphBuilder.AllocParameters<FArcTerrainCBTSplitReserveParameters>();
			ReserveParameters->CBTHeapSnapshot = SnapshotSRV;
			ReserveParameters->PoolCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
			ReserveParameters->PreparedSplitDescriptors =
				GraphBuilder.CreateUAV(TopologyResources.PreparedSplitDescriptors);
			ReserveParameters->WorkCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WorkCounters, PF_R32_UINT));
			ReserveParameters->SplitApplyBudget = TopologyBudgets.SplitApplyBudget;
			ReserveParameters->SplitAllocationBudget = TopologyBudgets.SplitAllocationBudget;
			ReserveParameters->PoolDepth = static_cast<uint32>(PoolDepth);
			ReserveParameters->PoolCapacity = PoolCapacity;
			ReserveParameters->MaxSubdivisionDepth = static_cast<uint32>(MaxSubdivisionDepth);
			ReserveParameters->StatsEnabled = StatsEnabled;
			TShaderMapRef<FArcTerrainCBTSplitReserveCS> ReserveShader(ShaderMap);
			if (bInlineDispatchGPUTiming)
			{
				FGPUTimingSlot& TimingSlot = GPUTimingSlots[GPUTimingSlotIndex];
				const int32 QueryIndex =
					static_cast<int32>(EGPUTimingInlineDispatch::SplitReserve) * 2;
				AddInlineTimedComputePass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.SplitReserve"),
					ReserveShader, ReserveParameters, FIntVector(1, 1, 1),
					TimingSlot.InlineQueries[QueryIndex].GetQuery(),
					TimingSlot.InlineQueries[QueryIndex + 1].GetQuery());
			}
			else
			{
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.SplitReserve"),
					ComputePassFlags, ReserveShader, ReserveParameters, FIntVector(1, 1, 1));
			}

			FArcTerrainCBTSplitApplyParameters* SplitParameters =
				GraphBuilder.AllocParameters<FArcTerrainCBTSplitApplyParameters>();
			SplitParameters->CBTHeapSnapshot = SnapshotSRV;
			SplitParameters->CBTHeap = GraphBuilder.CreateUAV(HeapRDG);
			SplitParameters->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
			SplitParameters->BisectorNeighbors = GraphBuilder.CreateUAV(NeighborRDG);
			SplitParameters->BisectorCommands = CommandsUAV;
			SplitParameters->PoolCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
			SplitParameters->LatencyCounters = LatencyCountersUAV;
			SplitParameters->FaceLiveCount = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(FaceLiveCountRDG, PF_R32_UINT));
			SplitParameters->PreparedSplitDescriptors =
				GraphBuilder.CreateUAV(TopologyResources.PreparedSplitDescriptors);
			SplitParameters->WorkCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WorkCounters, PF_R32_UINT));
			SplitParameters->SplitApplyBudget = TopologyBudgets.SplitApplyBudget;
			SplitParameters->PoolDepth = static_cast<uint32>(PoolDepth);
			SplitParameters->PoolCapacity = PoolCapacity;
			SplitParameters->NumFaces = static_cast<uint32>(NumFaces);
			SplitParameters->MaxSubdivisionDepth = static_cast<uint32>(MaxSubdivisionDepth);
			SplitParameters->MergePhaseCount = 1u;
			SplitParameters->MergeFramePhase = 0u;
			SplitParameters->StatsEnabled = StatsEnabled;
			SplitParameters->IndirectArgs = TopologyResources.IndirectArgs;
			FArcTerrainCBTSplitApplyCS::FPermutationDomain SplitPermutation;
			SplitPermutation.Set<FArcTerrainCBTSplitApplyCS::FLatencyAuditDim>(bLatencyAuditThisFrame);
			TShaderMapRef<FArcTerrainCBTSplitApplyCS> SplitShader(ShaderMap, SplitPermutation);
			if (bInlineDispatchGPUTiming)
			{
				FGPUTimingSlot& TimingSlot = GPUTimingSlots[GPUTimingSlotIndex];
				const int32 QueryIndex =
					static_cast<int32>(EGPUTimingInlineDispatch::SplitApply) * 2;
				AddInlineTimedIndirectComputePass(GraphBuilder,
					RDG_EVENT_NAME("ArcTerrainCBT.SplitApply"), SplitShader, SplitParameters,
					TopologyResources.IndirectArgs, SplitArgsOffset,
					TimingSlot.InlineQueries[QueryIndex].GetQuery(),
					TimingSlot.InlineQueries[QueryIndex + 1].GetQuery());
			}
			else
			{
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.SplitApply"),
					ComputePassFlags, SplitShader, SplitParameters, TopologyResources.IndirectArgs, SplitArgsOffset);
			}
			if (StatsEnabled != 0u)
			{
				FArcTerrainCBTSplitClaimAuditParameters* AuditParameters =
					GraphBuilder.AllocParameters<FArcTerrainCBTSplitClaimAuditParameters>();
				AuditParameters->CBTHeapSnapshot = SnapshotSRV;
				AuditParameters->CBTHeap = GraphBuilder.CreateUAV(HeapRDG);
				AuditParameters->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
				AuditParameters->PoolCounters = GraphBuilder.CreateUAV(
					FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
				AuditParameters->PoolDepth = static_cast<uint32>(PoolDepth);
				AuditParameters->PoolCapacity = PoolCapacity;
				TShaderMapRef<FArcTerrainCBTSplitClaimAuditCS> AuditShader(ShaderMap);
				FComputeShaderUtils::AddPass(GraphBuilder,
					RDG_EVENT_NAME("ArcTerrainCBT.SplitClaimAudit(StatsOnly)"), ComputePassFlags,
					AuditShader, AuditParameters, TopologyResources.IndirectArgs, ClaimAuditArgsOffset);
			}
			if (bStageGPUTiming)
			{
				AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
					static_cast<int32>(EGPUTimingBoundary::SplitApplyEnd), TEXT("SplitApplyEnd"));
			}

			FArcTerrainCBTMergeStageParameters* StageParameters =
				GraphBuilder.AllocParameters<FArcTerrainCBTMergeStageParameters>();
			StageParameters->CBTHeap = GraphBuilder.CreateUAV(HeapRDG);
			StageParameters->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
			StageParameters->BisectorCommands = CommandsUAV;
			StageParameters->BisectorPromote = PromoteUAV;
			StageParameters->PoolCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
			StageParameters->FaceLiveCount = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(FaceLiveCountRDG, PF_R32_UINT));
			StageParameters->SelectedDiamonds = GraphBuilder.CreateUAV(TopologyResources.SelectedDiamonds);
			StageParameters->WorkCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WorkCounters, PF_R32_UINT));
			StageParameters->SelectedCapacity = TopologyBudgets.SelectedCapacity;
			StageParameters->PoolDepth = static_cast<uint32>(PoolDepth);
			StageParameters->PoolCapacity = PoolCapacity;
			StageParameters->NumFaces = static_cast<uint32>(NumFaces);
			StageParameters->StatsEnabled = StatsEnabled;
			StageParameters->IndirectArgs = TopologyResources.IndirectArgs;
			TShaderMapRef<FArcTerrainCBTMergeStageCS> StageShader(ShaderMap);
			if (bInlineDispatchGPUTiming)
			{
				FGPUTimingSlot& TimingSlot = GPUTimingSlots[GPUTimingSlotIndex];
				const int32 QueryIndex =
					static_cast<int32>(EGPUTimingInlineDispatch::MergeStage) * 2;
				AddInlineTimedIndirectComputePass(GraphBuilder,
					RDG_EVENT_NAME("ArcTerrainCBT.MergeStage"), StageShader, StageParameters,
					TopologyResources.IndirectArgs, SelectedArgsOffset,
					TimingSlot.InlineQueries[QueryIndex].GetQuery(),
					TimingSlot.InlineQueries[QueryIndex + 1].GetQuery());
			}
			else
			{
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.MergeStage"),
					ComputePassFlags, StageShader, StageParameters, TopologyResources.IndirectArgs, SelectedArgsOffset);
			}
			if (bStageGPUTiming)
			{
				AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
					static_cast<int32>(EGPUTimingBoundary::MergeStageEnd), TEXT("MergeStageEnd"));
			}
		}

		if (bBoundedTopologyWork)
		{
			FArcTerrainCBTMergeCommitSparseParameters* CommitParameters =
				GraphBuilder.AllocParameters<FArcTerrainCBTMergeCommitSparseParameters>();
			CommitParameters->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
			CommitParameters->BisectorNeighbors = GraphBuilder.CreateUAV(NeighborRDG);
			CommitParameters->BisectorPromote = PromoteUAV;
			CommitParameters->PoolCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
			CommitParameters->FaceRoots = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(FaceRootsRDG, PF_R32_UINT));
			CommitParameters->SelectedDiamonds = GraphBuilder.CreateUAV(TopologyResources.SelectedDiamonds);
			CommitParameters->WorkCounters = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TopologyResources.WorkCounters, PF_R32_UINT));
			CommitParameters->SelectedCapacity = TopologyBudgets.SelectedCapacity;
			CommitParameters->PoolCapacity = PoolCapacity;
			CommitParameters->StatsEnabled = StatsEnabled;
			CommitParameters->IndirectArgs = TopologyResources.IndirectArgs;
			TShaderMapRef<FArcTerrainCBTMergeCommitSparseCS> CommitShader(ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.MergeCommitSparse"),
				ComputePassFlags, CommitShader, CommitParameters, TopologyResources.IndirectArgs, SelectedArgsOffset);
		}
		if (bStageGPUTiming)
		{
			AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
				static_cast<int32>(EGPUTimingBoundary::MergeCommitEnd), TEXT("MergeCommitEnd"));
		}
	}

	// 5. The base mesh, after the topology writes so it never mutates occupancy bits while the passes
	//    above are still walking the sum reduction those bits belong to.
	//
	//    Rooting and attaching run EVERY frame and take NO view gate: both are idempotent (a populated
	//    face re-roots nothing; a side is paired only while both its faces are still at their roots) and
	//    cost NumFaces / 64 groups. That makes "every face has geometry" a structural property rather
	//    than a property of one frame having had a perspective camera, and makes a pool refusal at seed
	//    time self-healing. Only the refine loop measures against the metric, so only it waits for a view.
	//
	//    Every pass gets its OWN parameter struct rather than sharing one: FComputeShaderUtils::AddPass
	//    runs ClearUnusedGraphResources over the struct it is given, nulling in place every resource the
	//    bound shader does not declare, and a pass dereferences its struct at EXECUTION — long after a
	//    second AddPass on the same memory would have nulled views it does read.
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ArcTerrainCBT.BaseMesh");

		const uint32 StatsEnabled = CVarStats.GetValueOnRenderThread() > 0 ? 1u : 0u;

		TFunction<FArcTerrainCBTBaseMeshParameters*(FRDGBufferUAVRef)> MakeParameters =
			[&](FRDGBufferUAVRef PassCommandsUAV) -> FArcTerrainCBTBaseMeshParameters*
			{
				FArcTerrainCBTBaseMeshParameters* Out =
					GraphBuilder.AllocParameters<FArcTerrainCBTBaseMeshParameters>();
				Out->CBTHeapSnapshot = SnapshotSRV;
				Out->CBTHeap = GraphBuilder.CreateUAV(HeapRDG);
				Out->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
				Out->BisectorNeighbors = GraphBuilder.CreateUAV(NeighborRDG);
				Out->BisectorCommands = PassCommandsUAV;
				Out->BisectorPromote = PromoteUAV;
				Out->PoolCounters = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
				Out->LatencyCounters = LatencyCountersUAV;
				Out->FaceLiveCount = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(FaceLiveCountRDG, PF_R32_UINT));
				Out->FaceRoots = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(FaceRootsRDG, PF_R32_UINT));
				Out->FaceNeighbors = FaceNeighborsSRV;
				Out->PoolDepth = static_cast<uint32>(PoolDepth);
				Out->PoolCapacity = PoolCapacity;
				Out->NumFaces = static_cast<uint32>(NumFaces);
				Out->MaxSubdivisionDepth = static_cast<uint32>(MaxSubdivisionDepth);
				Out->MergePhaseCount = 1u;
				Out->MergeFramePhase = 0u;
				Out->StatsEnabled = StatsEnabled;
				FillTessPredicateParameters(ActiveBaseMesh, MainView, TessResources, EClipSource::Live,
					Out->TessPredicate);
				return Out;
			};

		const FIntVector FaceGroups = FComputeShaderUtils::GetGroupCount(NumFaces, FaceThreads);

		FArcTerrainCBTSeedRootsCS::FPermutationDomain SeedPermutation;
		SeedPermutation.Set<FArcTerrainCBTSeedRootsCS::FLatencyAuditDim>(false);
		TShaderMapRef<FArcTerrainCBTSeedRootsCS> SeedShader(ShaderMap, SeedPermutation);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("SeedRoots"),
			ComputePassFlags, SeedShader, MakeParameters(CommandsUAV), FaceGroups);

		// Attach BEFORE refining, not after: at depth 1 the pairing is a table over the grid, and from
		// there the ordinary split rules carry the twin structure down every level the loop adds.
		FArcTerrainCBTAttachCS::FPermutationDomain AttachPermutation;
		AttachPermutation.Set<FArcTerrainCBTAttachCS::FLatencyAuditDim>(false);
		TShaderMapRef<FArcTerrainCBTAttachCS> AttachShader(ShaderMap, AttachPermutation);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Attach"),
			ComputePassFlags, AttachShader, MakeParameters(CommandsUAV), FaceGroups);

		if (bRefinePending && MainView.bValid)
		{
			bRefinePending = false;
			RDG_EVENT_SCOPE(GraphBuilder, "Refine");

			// Its own command buffer: this frame's already carries the classify pass's votes, which the
			// split/merge pass has applied and the refine loop must not re-apply.
			FRDGBufferRef RefineCommandsRDG = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), static_cast<int32>(PoolCapacity)),
				TEXT("ArcTerrainCBT.RefineCommands"));
			FRDGBufferUAVRef RefineCommandsUAV =
				GraphBuilder.CreateUAV(FRDGBufferUAVDesc(RefineCommandsRDG, PF_R32_UINT));
			AddClearUAVPass(GraphBuilder, RefineCommandsUAV, 0u, ComputePassFlags);

			const FIntVector PoolGroups =
				FComputeShaderUtils::GetGroupCount(static_cast<int32>(PoolCapacity), PoolThreads);

			// A fixed iteration count, driven from here: the GPU never loops on data. Each iteration
			// deepens a bisector by at most one level; the surplus over MaxSubdivisionDepth is there for
			// the iterations a chain lost its claim in and has to retry.
			FArcTerrainCBTRefineVoteCS::FPermutationDomain VotePermutation;
			VotePermutation.Set<FArcTerrainCBTRefineVoteCS::FLatencyAuditDim>(false);
			TShaderMapRef<FArcTerrainCBTRefineVoteCS> VoteShader(ShaderMap, VotePermutation);
			FArcTerrainCBTRefineApplyCS::FPermutationDomain ApplyPermutation;
			ApplyPermutation.Set<FArcTerrainCBTRefineApplyCS::FLatencyAuditDim>(false);
			TShaderMapRef<FArcTerrainCBTRefineApplyCS> ApplyShader(ShaderMap, ApplyPermutation);
			for (int32 Pass = 0; Pass < 2 * MaxSubdivisionDepth; ++Pass)
			{
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RefineVote"),
					ComputePassFlags, VoteShader, MakeParameters(RefineCommandsUAV), PoolGroups);
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RefineApply"),
					ComputePassFlags, ApplyShader, MakeParameters(RefineCommandsUAV), PoolGroups);
			}
		}
		else if (MotionRefineScheduledIterations > 0)
		{
			RDG_EVENT_SCOPE_STAT(GraphBuilder, ArcTerrainCBTMotionRefine, "ArcTerrainCBT.MotionRefine");

			// Motion catch-up must not inherit classify votes from the normal topology update. Keep its
			// command storage, clear, and all scheduled rounds inside the feature's GPU timing scope.
			FRDGBufferRef RefineCommandsRDG = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), static_cast<int32>(PoolCapacity)),
				TEXT("ArcTerrainCBT.MotionRefineCommands"));
			FRDGBufferUAVRef RefineCommandsUAV =
				GraphBuilder.CreateUAV(FRDGBufferUAVDesc(RefineCommandsRDG, PF_R32_UINT));
			AddClearUAVPass(GraphBuilder, RefineCommandsUAV, 0u, ComputePassFlags);

			const FIntVector PoolGroups =
				FComputeShaderUtils::GetGroupCount(static_cast<int32>(PoolCapacity), PoolThreads);

			FArcTerrainCBTRefineVoteCS::FPermutationDomain VotePermutation;
			VotePermutation.Set<FArcTerrainCBTRefineVoteCS::FLatencyAuditDim>(false);
			TShaderMapRef<FArcTerrainCBTRefineVoteCS> VoteShader(ShaderMap, VotePermutation);
			FArcTerrainCBTRefineApplyCS::FPermutationDomain ApplyPermutation;
			ApplyPermutation.Set<FArcTerrainCBTRefineApplyCS::FLatencyAuditDim>(bLatencyAuditThisFrame);
			TShaderMapRef<FArcTerrainCBTRefineApplyCS> ApplyShader(ShaderMap, ApplyPermutation);
			for (int32 Pass = 0; Pass < MotionRefineScheduledIterations; ++Pass)
			{
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("MotionRefineVote"),
					ComputePassFlags, VoteShader, MakeParameters(RefineCommandsUAV), PoolGroups);
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("MotionRefineApply"),
					ComputePassFlags, ApplyShader, MakeParameters(RefineCommandsUAV), PoolGroups);
			}
		}
	}
	if (bStageGPUTiming)
	{
		AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
			static_cast<int32>(EGPUTimingBoundary::BaseRefineEnd), TEXT("BaseRefineEnd"));
	}

	// 6. Sum-reduction, deepest internal level up to the root.
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ArcTerrainCBT.Reduce");
		for (int32 Level = PoolDepth - 1; Level >= 0; --Level)
		{
			const int64 NodesPerLevel = 1ll << static_cast<int64>(Level);
			const int32 GroupCount = FMath::Min<int32>(
				static_cast<int32>(FMath::DivideAndRoundUp<int64>(NodesPerLevel, ReduceThreads)), MaxReduceGroups);

			FArcTerrainCBTReduceCS::FParameters* Parameters =
				GraphBuilder.AllocParameters<FArcTerrainCBTReduceCS::FParameters>();
			Parameters->CBTHeap = GraphBuilder.CreateUAV(HeapRDG);
			Parameters->PoolDepth = static_cast<uint32>(PoolDepth);
			Parameters->Level = static_cast<uint32>(Level);
			Parameters->ThreadStride = static_cast<uint32>(GroupCount) * static_cast<uint32>(ReduceThreads);

			TShaderMapRef<FArcTerrainCBTReduceCS> Shader(ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.Reduce (level %d)", Level),
				ComputePassFlags, Shader, Parameters, FIntVector(GroupCount, 1, 1));
		}
	}
	if (bStageGPUTiming)
	{
		AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
			static_cast<int32>(EGPUTimingBoundary::ReduceEnd), TEXT("ReduceEnd"));
	}

	// 7. Publish the full-live draw and link dispatch arguments.
	{
		FArcTerrainCBTBatchCS::FParameters* Parameters =
			GraphBuilder.AllocParameters<FArcTerrainCBTBatchCS::FParameters>();
		Parameters->CBTHeap = GraphBuilder.CreateUAV(HeapRDG);
		Parameters->NodeCounter = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(NodeCounterRDG, PF_R32_UINT));
		Parameters->RWDrawArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(DrawArgsRDG, PF_R32_UINT));
		Parameters->RWLinkDispatchArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(LinkArgsRDG, PF_R32_UINT));
		Parameters->PoolCounters = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
		Parameters->PoolDepth = static_cast<uint32>(PoolDepth);
		Parameters->IndexCount = 3u; // {0,1,2} ref-patch IB
		Parameters->ThreadsPerGroup = static_cast<uint32>(LinkThreads);

		TShaderMapRef<FArcTerrainCBTBatchCS> Shader(ShaderMap);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.Batch"),
			ComputePassFlags, Shader, Parameters, FIntVector(1, 1, 1));
		if (bStageGPUTiming)
		{
			AddGPUTimingBoundary(GraphBuilder, GPUTimingSlotIndex,
				static_cast<int32>(EGPUTimingBoundary::BatchEnd), TEXT("BatchEnd"));
		}
	}

	if (EnumHasAnyFlags(ComputePassFlags, ERDGPassFlags::AsyncCompute))
	{
		GraphBuilder.SetBufferAccessFinal(HeapRDG, ERHIAccess::UAVCompute);
		GraphBuilder.SetBufferAccessFinal(BisectorRDG, ERHIAccess::UAVCompute);
		GraphBuilder.SetBufferAccessFinal(NeighborRDG, ERHIAccess::UAVCompute);
		GraphBuilder.SetBufferAccessFinal(NodeCounterRDG, ERHIAccess::UAVCompute);
		GraphBuilder.SetBufferAccessFinal(PoolCountersRDG, ERHIAccess::UAVCompute);
		GraphBuilder.SetBufferAccessFinal(FaceLiveCountRDG, ERHIAccess::UAVCompute);
		GraphBuilder.SetBufferAccessFinal(FaceRootsRDG, ERHIAccess::UAVCompute);
		GraphBuilder.SetBufferAccessFinal(DrawArgsRDG, ERHIAccess::UAVCompute);
		GraphBuilder.SetBufferAccessFinal(LinkArgsRDG, ERHIAccess::UAVCompute);
		GraphBuilder.SetBufferAccessFinal(PromoteRDG, ERHIAccess::UAVCompute);
		if (bBoundedTopologyWork)
		{
			GraphBuilder.SetBufferAccessFinal(TopologyResources.MergeScanCursor, ERHIAccess::UAVCompute);
		}
	}
	return true;
}

void FArcTerrainCBTRendererExtension::FinalizeUpdateForDraw(
	FRDGBuilder& GraphBuilder, bool bFinalizeTopology)
{
	using namespace ArcTerrainCBTRendererPrivate_9a07;
	check(Buffers.IsInitialized());
	check(ActiveBaseMesh.IsValid());

	RDG_EVENT_SCOPE_STAT(GraphBuilder, ArcTerrainCBT, "ArcTerrainCBT.Finalize");
	const int32 PoolDepth = Buffers.GetPoolDepth();
	const int32 NumFaces = Buffers.GetNumFaces();
	FGlobalShaderMap* const ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	FRDGBufferRef HeapRDG = Buffers.RegisterHeap(GraphBuilder);
	FRDGBufferRef BisectorRDG = Buffers.RegisterBisectors(GraphBuilder);
	FRDGBufferRef NeighborRDG = Buffers.RegisterNeighbors(GraphBuilder);
	FRDGBufferRef PoolCountersRDG = Buffers.RegisterPoolCounters(GraphBuilder);
	FRDGBufferRef VisibleBisectorsRDG = Buffers.RegisterVisibleBisectors(GraphBuilder);
	FRDGBufferRef DrawArgsRDG = Buffers.RegisterDrawArgs(GraphBuilder);
	FRDGBufferRef LinkArgsRDG = Buffers.RegisterLinkArgs(GraphBuilder);
	FRDGBufferRef PromoteRDG = Buffers.RegisterPromote(GraphBuilder);
	GraphBuilder.UseInternalAccessMode(HeapRDG);
	GraphBuilder.UseInternalAccessMode(BisectorRDG);
	GraphBuilder.UseInternalAccessMode(VisibleBisectorsRDG);
	GraphBuilder.UseInternalAccessMode(DrawArgsRDG);

	const FArcTerrainDistanceCullParameters DistanceCull =
		ResolveDistanceCullParameters(ActiveBaseMesh, MainView);
	if (MainView.bValid && MainView.CaptureFrame == GFrameNumberRenderThread)
	{
		SCOPE_CYCLE_COUNTER(STAT_ArcTerrainCBTCompactSetup);
		const FArcTerrainTessPredicateResources TessResources =
			CreateTessPredicateResources(GraphBuilder, ActiveBaseMesh);
		FArcTerrainCBTCompactCS::FParameters* Parameters =
			GraphBuilder.AllocParameters<FArcTerrainCBTCompactCS::FParameters>();
		Parameters->CBTHeapRead = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(HeapRDG));
		Parameters->BisectorPoolRead = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(BisectorRDG));
		Parameters->RWVisibleBisectors = GraphBuilder.CreateUAV(VisibleBisectorsRDG);
		Parameters->RWDrawArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(DrawArgsRDG, PF_R32_UINT));
		Parameters->PoolCounters =
			GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
		Parameters->LatencyCounters = nullptr;
		Parameters->PoolDepth = static_cast<uint32>(PoolDepth);
		Parameters->NumFaces = static_cast<uint32>(NumFaces);
		Parameters->StatsEnabled = CVarStats.GetValueOnRenderThread() > 0 ? 1u : 0u;
		Parameters->DistanceCullEnabled = DistanceCull.bEnabled;
		Parameters->DistanceCullRadiusSq = DistanceCull.RadiusSq;
		Parameters->DistanceCullMetric = DistanceCull.Metric;
		Parameters->DistanceCullCameraTexel = DistanceCull.CameraTexel;
		Parameters->DistanceCullCameraTexelFrac = DistanceCull.CameraTexelFrac;
		FillTessPredicateParameters(ActiveBaseMesh, MainView, TessResources, EClipSource::Cull,
			Parameters->TessPredicate);
		Parameters->IndirectArgs = LinkArgsRDG;

		FArcTerrainCBTCompactCS::FPermutationDomain Permutation;
		Permutation.Set<FArcTerrainCBTCompactCS::FLatencyAuditDim>(false);
		TShaderMapRef<FArcTerrainCBTCompactCS> Shader(ShaderMap, Permutation);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.Compact"),
			ERDGPassFlags::Compute, Shader, Parameters, LinkArgsRDG, 0u);
	}

	if (bFinalizeTopology)
	{
		FArcTerrainCBTLinkParameters* Parameters = GraphBuilder.AllocParameters<FArcTerrainCBTLinkParameters>();
		Parameters->CBTHeapRead = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(HeapRDG));
		Parameters->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
		Parameters->BisectorNeighbors = GraphBuilder.CreateUAV(NeighborRDG);
		Parameters->BisectorPromote = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PromoteRDG, PF_R32_UINT));
		Parameters->PoolCounters = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
		Parameters->PoolDepth = static_cast<uint32>(PoolDepth);
		Parameters->IndirectArgs = LinkArgsRDG;

		TShaderMapRef<FArcTerrainCBTLinkCS> Shader(ShaderMap);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.Link"),
			ERDGPassFlags::Compute, Shader, Parameters, LinkArgsRDG, 0u);

		if (CVarValidate.GetValueOnRenderThread() > 0)
		{
			FArcTerrainCBTLinkParameters* ValidateParameters =
				GraphBuilder.AllocParameters<FArcTerrainCBTLinkParameters>();
			ValidateParameters->CBTHeapRead = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(HeapRDG));
			ValidateParameters->BisectorPool = GraphBuilder.CreateUAV(BisectorRDG);
			ValidateParameters->BisectorNeighbors = GraphBuilder.CreateUAV(NeighborRDG);
			ValidateParameters->PoolCounters =
				GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PoolCountersRDG, PF_R32_UINT));
			ValidateParameters->PoolDepth = static_cast<uint32>(PoolDepth);
			ValidateParameters->IndirectArgs = LinkArgsRDG;

			TShaderMapRef<FArcTerrainCBTValidateCS> ValidateShader(ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.Validate"),
				ERDGPassFlags::Compute, ValidateShader, ValidateParameters, LinkArgsRDG, 0u);
		}
	}

	if (!bCounterReadbackPending)
	{
		if (!CounterReadback.IsValid())
		{
			CounterReadback = MakeUnique<FRHIGPUBufferReadback>(TEXT("ArcTerrainCBT.CounterReadback"));
		}
		AddEnqueueCopyPass(GraphBuilder, CounterReadback.Get(), PoolCountersRDG, CounterReadbackBytes);
		PendingCounterDistanceCull.bRequested = DistanceCull.bRequested;
		PendingCounterDistanceCull.bEnabled = DistanceCull.bEnabled != 0u;
		PendingCounterDistanceCull.RadiusCm = DistanceCull.RequestedRadiusCm;
		bCounterReadbackPending = true;
	}

	GraphBuilder.UseExternalAccessMode(HeapRDG, ERHIAccess::SRVGraphics, ERHIPipeline::Graphics);
	GraphBuilder.UseExternalAccessMode(BisectorRDG, ERHIAccess::SRVGraphics, ERHIPipeline::Graphics);
	GraphBuilder.UseExternalAccessMode(VisibleBisectorsRDG, ERHIAccess::SRVGraphics, ERHIPipeline::Graphics);
	GraphBuilder.UseExternalAccessMode(DrawArgsRDG, ERHIAccess::IndirectArgs, ERHIPipeline::Graphics);
}
