// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "Render/ArcTerrainCBTBuffers.h"
#include "Render/ArcTerrainTileAtlas.h"
#include "Async/Mutex.h"
#include "RHIGPUReadback.h"
#include "RHIResources.h"
#include "SceneViewExtension.h"

#include <atomic>

class FRDGBuilder;
class FPostOpaqueRenderParameters;
class FSceneInterface;

struct FArcTerrainRenderOwnerKey
{
	uint64 WorldInstanceToken = 0;
	uint64 TerrainInstanceId = 0;

	bool IsValid() const { return WorldInstanceToken != 0 && TerrainInstanceId != 0; }

	bool operator==(const FArcTerrainRenderOwnerKey& Other) const
	{
		return WorldInstanceToken == Other.WorldInstanceToken && TerrainInstanceId == Other.TerrainInstanceId;
	}
};

inline uint32 GetTypeHash(const FArcTerrainRenderOwnerKey& Key)
{
	return HashCombineFast(GetTypeHash(Key.WorldInstanceToken), GetTypeHash(Key.TerrainInstanceId));
}

struct FArcTerrainRenderOwnerIdentity
{
	FArcTerrainRenderOwnerKey Key;
	uint64 OwnerEpoch = 0;

	bool IsValid() const { return Key.IsValid() && OwnerEpoch != 0; }

	bool operator==(const FArcTerrainRenderOwnerIdentity& Other) const
	{
		return Key == Other.Key && OwnerEpoch == Other.OwnerEpoch;
	}

	bool operator!=(const FArcTerrainRenderOwnerIdentity& Other) const { return !(*this == Other); }
};

struct FArcTerrainRenderIdentity
{
	FArcTerrainRenderOwnerIdentity Owner;
	uint64 ResourceRevision = 0;
	uint64 RhiEpoch = 0;
	EWorldType::Type WorldType = EWorldType::None;
	FSceneInterface* SceneKey = nullptr;

	bool IsValid() const
	{
		return Owner.IsValid() && ResourceRevision != 0 && RhiEpoch != 0 && SceneKey != nullptr;
	}

	bool operator==(const FArcTerrainRenderIdentity& Other) const
	{
		return Owner == Other.Owner && ResourceRevision == Other.ResourceRevision
			&& RhiEpoch == Other.RhiEpoch && WorldType == Other.WorldType && SceneKey == Other.SceneKey;
	}

	bool operator!=(const FArcTerrainRenderIdentity& Other) const { return !(*this == Other); }
};

struct FArcTerrainCBTBindingSourceToken
{
	uint64 ProxySerial = 0;
	uint8 SlotIndex = MAX_uint8;

	bool IsValid() const { return ProxySerial != 0 && SlotIndex < 2; }

	bool operator==(const FArcTerrainCBTBindingSourceToken& Other) const
	{
		return ProxySerial == Other.ProxySerial && SlotIndex == Other.SlotIndex;
	}
};

inline uint32 GetTypeHash(const FArcTerrainCBTBindingSourceToken& Token)
{
	return HashCombineFast(GetTypeHash(Token.ProxySerial), GetTypeHash(Token.SlotIndex));
}

struct FArcTerrainCBTWorldClaim
{
	uint64 WorldInstanceToken = 0;
	EWorldType::Type WorldType = EWorldType::None;
	FSceneInterface* SceneKey = nullptr;

	bool IsValid() const { return WorldInstanceToken != 0 && SceneKey != nullptr; }
};

/**
 * The base mesh handed to the CBT update: the STATIC face grid covering the world. A face id is a
 * position in that grid and nothing else — it holds no atlas slice, it is never reissued, and it exists
 * from level start to level end, so a residency change moves no face and destroys no subdivision.
 */
struct FArcTerrainCBTBaseMesh
{
	FArcTerrainRenderIdentity Identity;

	/** The face id space — NumFacesPerEdge^2 of the terrain's face grid. */
	int32 NumFaces = 0;

	/** Per face, four entries (-X, +X, -Y, +Y): the neighbouring face id, or 0xFFFFFFFF at the outer
	 *  border of the grid, which is the only halfedge with no twin. */
	TArray<uint32> Neighbors;

	/** Per face: GLOBAL TEXEL INDEX of the face's (0,0) corner. Integer, because it is the only
	 *  quantity the predicate can subtract the camera from without losing metre-scale edges to a
	 *  terrain-scale magnitude — a float32 centimetre at 2e9 cm has a 32 cm ULP, which is a third of the
	 *  finest edge the metric measures. */
	TArray<FIntPoint> FaceTexelOrigins;

	/** The terrain actor's transform: the terrain-local frame every face origin is expressed in. */
	FMatrix LocalToWorld = FMatrix::Identity;

	/** Height atlas the split/merge metric samples. The slice comes from HeightRing, never from a face id. */
	TSharedPtr<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe> AtlasResources;

	/** Quads per sector side and per face side, and the log2 of the former (SectorKey = GlobalTexel >>). */
	int32 SectorQuads = 0;
	int32 FaceQuads = 0;
	int32 SectorQuadsShift = 0;

	/** Terrain-local cm per texel — the one place a length leaves the integer domain. */
	float TexelToLocalCm = 0.0f;

	/** Sector-keyed slice lookup PER TIER, published from the same snapshot the vertex factory gets. */
	TArray<FArcTerrainResidencyRing, TInlineAllocator<ArcTerrain::MaxTiers>> HeightRings;

	/** The authored render/tessellation knobs (AArcTerrain, Terrain|Render + Terrain|Tessellation). They ride
	 *  the base mesh because every consumer of them is render-thread; the values are clamped where they
	 *  are consumed, not here. A change of PoolDepth rebuilds the pool in BeginFrame. */
	int32 PoolDepth = 20;
	bool bEnableDistanceCulling = false;
	double DistanceCullRadius = 10000000.0;
	float ConservativeMinHeightCm = -1000000.0f;
	float ConservativeMaxHeightCm = 1000000.0f;
	float TargetTrianglePixels = 144.0f;
	float MaxTriangleEdgePixels = 24.0f;
	float MergeThresholdFraction = 0.5f;
	float TargetErrorPixels = 1.0e6f;
	float FlatAreaRelief = 64.0f;
	float FlatReliefSagCmPerMeterSq = 0.1f;
	float FrustumCullMargin = 0.35f;
	float TessFrustumCullMargin = 1.5f;

	bool IsValid() const
	{
		return Identity.IsValid() && NumFaces > 0 && SectorQuads > 0 && FaceQuads > 0 && TexelToLocalCm > 0.0f
			&& Neighbors.Num() == NumFaces * 4 && FaceTexelOrigins.Num() == NumFaces
			&& AtlasResources.IsValid();
	}
};

/**
 * The MAIN VIEW of a frame, captured on the render thread and reduced to what the tessellation
 * predicate needs.
 *
 * There is one CBT tree and many views — editor viewport, PIE, scene captures, shadow passes. The tree
 * is driven by the main view (the first non-capture view of the family) and every other view draws the
 * same triangulation. That is a contract, not an implementation detail: the alternative is one tree per
 * view, which is the whole cost the pool exists to avoid.
 */
struct FArcTerrainCBTMainView
{
	FSceneInterface* SceneKey = nullptr;
	/** The captured view, as an identity key ONLY — compared, never dereferenced, and dangling from the
	 *  frame after it was captured. The scene renderer rewrites FSceneViewFamily::Views to its own
	 *  FViewInfo objects before any extension runs, so this is the same pointer GetDynamicMeshElements is
	 *  later handed for the same view. Matrices cannot serve as the key: temporal jitter rewrites the
	 *  projection between this capture and the gather, and every equality test would fail. */
	const FSceneView* ViewKey = nullptr;

	/** GFrameNumberRenderThread of the capture. The view block is STICKY so the tree keeps following the
	 *  last real camera through capture-only frames, but a drawn list may only be bound by a view captured
	 *  in the same frame — a stale key is a list culled to a frustum nobody is looking through. */
	uint32 CaptureFrame = MAX_uint32;

	FVector ViewOrigin = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	FVector2f ViewportSize = FVector2f::ZeroVector;

	/** Viewport pixels per radian at the view centre: ViewportHeight / (2 * tan(HalfFOVY)). */
	float PixelsPerRadian = 0.0f;

	/** The clip transform, kept in its TRANSLATED-world form plus the translation that reaches it. The
	 *  predicate is composed against the terrain transform into an eye-relative matrix, so no planet-scale
	 *  world position is ever built — the whole reason the corner scheme is integer texels. */
	FMatrix TranslatedWorldToClip = FMatrix::Identity;
	FVector PreViewTranslation = FVector::ZeroVector;

	/** The same three quantities as FreezeRendering snapshotted them, valid only while bFrozen. The
	 *  compaction cuts the drawn list with these while the metric keeps measuring against the live
	 *  camera, which is what makes flying away from a frozen frustum show what it removed. */
	FMatrix FrozenTranslatedWorldToClip = FMatrix::Identity;
	FVector FrozenPreViewTranslation = FVector::ZeroVector;
	FVector FrozenViewOrigin = FVector::ZeroVector;
	bool bFrozen = false;

	bool bValid = false;
};

/**
 * Captures the main view into the renderer extension. PreRenderViewFamily_RenderThread runs in the
 * scene-update phase, ahead of the PreRender delegate the pool update hangs off, so the capture and the
 * consumption are the same frame.
 */
class FArcTerrainCBTViewExtension : public FSceneViewExtensionBase
{
public:
	explicit FArcTerrainCBTViewExtension(const FAutoRegister& AutoRegister)
		: FSceneViewExtensionBase(AutoRegister)
	{
	}

	//~ Begin ISceneViewExtension Interface
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder,
		FSceneViewFamily& InViewFamily) override;
	//~ End ISceneViewExtension Interface
};

/**
 * FArcTerrainCBTRendererExtension — the cross-frame bisector-pool update.
 *
 * A single TGlobalResource owns one persistent pool. Post-opaque advances topology through Batch on
 * async compute; the next BeginFrame acquires it and runs Compact, Link, and optional Validate on graphics.
 * The pipeline runs a CONSTANT number of dispatches over ONE tree regardless of resident sector count:
 *
 *   args -> snapshot -> classify -> split/merge -> root + attach -> reduce -> batch | compact -> link
 *   (-> validate), where | is the frame boundary in steady state.
 *
 * Rooting and attaching run every frame with no view gate, so every face carries geometry from the first
 * update whatever the projection is; the FIRST frame that has a main view additionally inserts the
 * one-shot 2 * MaxSubdivisionDepth refine vote/apply loop, which is what brings the mesh to the metric's
 * triangulation in one frame rather than over that many frames at planet-sized triangles.
 *
 * Everything decides against the single frozen snapshot of state N; the maintenance passes run after
 * the topology writes precisely so that they never invalidate the decode that pass is walking.
 *
 * Render-thread only.
 */
class FArcTerrainCBTRendererExtension : public FRenderResource
{
public:
	FArcTerrainCBTRendererExtension() = default;

	/** Idempotent one-time registration of the renderer hooks. */
	void RegisterExtension();
	void UnregisterExtension();

	bool IsInFrame() const { return bInFrame; }

	const FArcTerrainCBTBuffers& GetBuffers() const { return Buffers; }

	void ClaimWorld_GameThread(const FArcTerrainCBTWorldClaim& Claim);
	void ReleaseWorld_GameThread(uint64 WorldInstanceToken);
	void PublishCandidate_GameThread(FArcTerrainCBTBaseMesh&& BaseMesh);
	void UnpublishCandidate_GameThread(const FArcTerrainRenderIdentity& Identity);
	void RetireTerrain_GameThread(const FArcTerrainRenderOwnerIdentity& Owner);
	uint64 GetRhiEpoch() const { return RhiEpoch.load(std::memory_order_acquire); }

	/** Render-thread: adopt this frame's main view. Called by FArcTerrainCBTViewExtension. */
	void SetMainView_RenderThread(FArcTerrainCBTMainView&& View);

	/**
	 * Whether this view is the one the frame's compaction culled the drawn list for, and may therefore
	 * bind the compacted args. Everything else — shadow depth, scene capture, the second eye of a stereo
	 * pair, a view family that never captured a main view — answers false and draws the whole live set,
	 * because a list culled to one frustum drops the casters standing outside it.
	 */
	bool IsCulledView_RenderThread(const FSceneView& View);
	bool IsDrawPacketAccepted_RenderThread(const FArcTerrainRenderIdentity& Identity,
		const FSceneInterface* Scene) const;
	const FArcTerrainRenderIdentity& GetAcceptedIdentity_RenderThread() const { return AcceptedIdentity; }
	bool RegisterBindingSource_RenderThread(const FArcTerrainRenderIdentity& Identity,
		const FArcTerrainCBTBindingSourceToken& SourceToken);
	void UnregisterBindingSource_RenderThread(const FArcTerrainRenderIdentity& Identity,
		const FArcTerrainCBTBindingSourceToken& SourceToken);
	void RecordProxyDiagnostic_RenderThread(bool bAccepted) const;
	void GetProxyDiagnosticCounts_RenderThread(uint64& OutAccepted, uint64& OutRejected) const;

	//~ Begin FRenderResource Interface
	virtual void ReleaseRHI() override;
	//~ End FRenderResource Interface

private:
	struct FCandidateState
	{
		FArcTerrainCBTBaseMesh BaseMesh;
		TSet<FArcTerrainCBTBindingSourceToken> LiveBindingSources;

		bool IsReady() const { return LiveBindingSources.Num() > 0; }
	};

	void BeginFrame(FRDGBuilder& GraphBuilder);
	void EndFrame(FRDGBuilder& GraphBuilder);
	void PostOpaque_RenderThread(FPostOpaqueRenderParameters& Parameters);

	bool SubmitTopologyUpdate(FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags);
	void FinalizeUpdateForDraw(FRDGBuilder& GraphBuilder, bool bFinalizeTopology);
	void DrainCounterReadback();

	/** arc.Terrain.CBT.Stats: the drained GPU counters plus the resolved predicate inputs and view
	 *  state, which no GPU counter can report — a mis-plumbed view block shows up only here. */
	void LogStats(const uint32* Counters, int32 NumFaces, int32 MaxSubdivisionDepth) const;

	/** arc.Terrain.CBT.ProbePredicate: the three elected leaves' whole predicate input set, against the
	 *  view state that produced it — a matrix built about a different origin than the corners it
	 *  transforms is visible only with the two side by side. */
	void LogProbe(const uint32* Counters) const;

	/** Re-sets the six STATGROUP_ArcTerrain counters from the last drained readback. Called every frame,
	 *  not only on a drain frame: the readback lands every few frames and a DWORD counter left unset in a
	 *  frame reads 0. */
	void PublishCounterStats() const;

	/** Apply the sanitized GPU-timing mode and invalidate samples from the previous mode. */
	void UpdateGPUTimingMode();

	/** Poll completed absolute-time query sets without flushing or waiting for the GPU. */
	void DrainGPUTimingSamples(uint32 DrainFrame);

	/** Invalidate the current timing generation while retaining in-flight query ownership. */
	void InvalidateGPUTimingGeneration();

	/** Reset only the current generation's one-second timing aggregate. */
	void ResetGPUTimingAggregate();

	/** Reserve and fully allocate one fixed timing-ring slot, or return INDEX_NONE without waiting. */
	int32 AcquireGPUTimingSlot(uint32 SourceFrame, uint8 Mode, uint8 BoundaryCount,
		bool bShaderStats, bool bBulkRefine, uint8 MotionRefineIterations,
		bool bInlineDispatchTiming);

	/** Add one absolute timestamp pass. The RDG lambda captures only the slot-owned stable raw query. */
	void AddGPUTimingBoundary(FRDGBuilder& GraphBuilder, int32 SlotIndex, int32 BoundaryIndex,
		const TCHAR* BoundaryName);

	struct FSubmittedViewMotion
	{
		bool bFresh = false;
		bool bHasDelta = false;
		uint8 Cause = 0;
		float TurnDegrees = 0.0f;
		float TurnDegreesPerSecond = 0.0f;
		float MoveCm = 0.0f;
		float MoveCmPerSecond = 0.0f;
		float ForwardSpeedCmPerSecond = 0.0f;
		float LateralSpeedCmPerSecond = 0.0f;
		FVector MoveDirection = FVector::ZeroVector;
	};

	/** Sample motion only from a fresh, non-frozen main view actually consumed by this submission. */
	FSubmittedViewMotion SampleSubmittedViewMotion(uint32 SourceFrame);

	struct FTopologyWorkBudgets
	{
		uint32 NormalScanBudget = 0;
		uint32 PressureScanBudget = 0;
		uint32 SelectedCapacity = 0;
		uint32 SplitApplyBudget = 0;
		uint32 SplitAllocationBudget = 0;
	};

	struct FTopologyWorkResources
	{
		FRDGBufferRef RawSplitRequests = nullptr;
		FRDGBufferRef SplitPriorityBySlot = nullptr;
		FRDGBufferRef SplitBucketCandidates = nullptr;
		FRDGBufferRef SplitBucketCounts = nullptr;
		FRDGBufferRef AdmittedSplitRequests = nullptr;
		FRDGBufferRef PreparedSplitDescriptors = nullptr;
		FRDGBufferRef SelectedDiamonds = nullptr;
		FRDGBufferRef WorkCounters = nullptr;
		FRDGBufferRef WindowMetadata = nullptr;
		FRDGBufferRef IndirectArgs = nullptr;
		FRDGBufferRef MergeScanCursor = nullptr;
	};

	/** Used only after the completed bounded path is selected before transient resource creation. */
	static FTopologyWorkBudgets ResolveTopologyWorkBudgets(uint32 PoolCapacity);
	FTopologyWorkResources CreateTopologyWorkResources(FRDGBuilder& GraphBuilder, uint32 PoolCapacity,
		const FTopologyWorkBudgets& Budgets, ERDGPassFlags ComputePassFlags) const;

	/** Drain source-frame-tagged camera-motion audit samples without ever waiting for the GPU. */
	void DrainLatencyReadbacks(uint32 DrainFrame, bool bLoggingEnabled);

	/** Consume one post-update visible-debt sample and advance/close its episode. */
	void ProcessLatencySample(const uint32* Counters, int32 SlotIndex, uint32 DrainFrame,
		bool bGapCoveredBeforeSample);

	/** Mark a source frame that could not be sampled; convergence is no longer exact across the gap. */
	void MarkLatencySampleGap(uint32 SourceFrame, const TCHAR* Reason);

	/** Add one missing source frame to the bounded/coalesced ordered gap stream. */
	void EnqueueLatencyGap(uint32 SourceFrame, uint32 EpisodeId, uint32 LoggerGeneration,
		const TCHAR* Reason);

	/** Consume one queued gap range after every older source-frame readback has drained. */
	void ProcessLatencyGap(uint32 FirstFrame, uint32 LastFrame, uint32 ExplicitGapCount,
		uint32 EpisodeId, uint32 LoggerGeneration, const TCHAR* Reason);

	/** Emit and reset the currently active camera-motion episode. */
	void FinishLatencyEpisode(const TCHAR* FinishReason, uint32 ReadySourceFrame,
		int32 DrainingSlotIndex = INDEX_NONE);

	void ReleaseActivePool();
	void ResetCandidateRegistry();
	void ResetOwnerDependentState();
	void SelectActiveWorld_RenderThread();
	void AdoptActiveCandidate_RenderThread(FRDGBuilder& GraphBuilder);
	void ClaimWorld_RenderThread(const FArcTerrainCBTWorldClaim& Claim);
	void ReleaseWorld_RenderThread(uint64 WorldInstanceToken);
	void PublishCandidate_RenderThread(FArcTerrainCBTBaseMesh&& BaseMesh, bool bAllowTestDelay = true);
	void PublishCandidateState_RenderThread(FCandidateState&& Candidate, bool bAllowTestDelay);
	void UnpublishCandidate_RenderThread(const FArcTerrainRenderIdentity& Identity);
	void RetireTerrain_RenderThread(const FArcTerrainRenderOwnerIdentity& Owner);
	bool IsIdentityLive_RenderThread(const FArcTerrainRenderIdentity& Identity) const;

	bool bInFrame = false;
	bool bRegistered = false;
	FDelegateHandle PreRenderDelegateHandle;
	FDelegateHandle PostRenderDelegateHandle;
	FDelegateHandle PostOpaqueRenderDelegateHandle;

	struct FArmedTopologyUpdate
	{
		FArcTerrainRenderIdentity Identity;
		FSceneInterface* SceneKey = nullptr;
		const FSceneView* ViewKey = nullptr;
		uint32 Frame = MAX_uint32;
		bool bArmed = false;
	};
	FArmedTopologyUpdate ArmedTopologyUpdate;
	FArcTerrainRenderIdentity PendingTopologyIdentity;
	uint32 PendingTopologySourceFrame = MAX_uint32;
	bool bTopologyUpdatePending = false;

	FArcTerrainCBTBuffers Buffers;

	/** Guards the pending base mesh against the game-thread publisher. */
	UE::FMutex Mutex;

	struct FWorldClaimState
	{
		FArcTerrainCBTWorldClaim Claim;
		uint64 ClaimSerial = 0;
	};
	TMap<uint64, FWorldClaimState> WorldClaims;
	TOptional<FCandidateState> RuntimeCandidate;
	TOptional<FCandidateState> ParkedEditorCandidate;
#if !UE_BUILD_SHIPPING
	TOptional<FCandidateState> DelayedTestCandidate;
	int32 DelayedTestCandidateFrames = 0;
	int32 TestPublishDelayLastCvar = 0;
	bool bTestPublishDelayArmed = false;
	bool bTestPublishDelayConsumed = false;
#endif
	TMap<FArcTerrainRenderOwnerKey, uint64> RetiredOwnerEpochs;
	struct FUnpublishedOwnerState
	{
		uint64 OwnerEpoch = 0;
		uint64 ResourceRevision = 0;
	};
	TMap<FArcTerrainRenderOwnerKey, FUnpublishedOwnerState> UnpublishedOwners;
	uint64 NextClaimSerial = 1;
	uint64 ActiveWorldInstanceToken = 0;
	FArcTerrainRenderOwnerIdentity ActiveOwnerLock;
	FArcTerrainRenderIdentity AcceptedIdentity;
	mutable std::atomic<uint64> ProxyAcceptedCount { 0 };
	mutable std::atomic<uint64> ProxyRejectedCount { 0 };
	bool bLoggedFirstSubmit = false;
	std::atomic<uint64> RhiEpoch { 1 };

	/** The base mesh the pool is currently running, and the terrain it belongs to. */
	FArcTerrainCBTBaseMesh ActiveBaseMesh;

	/** True from the frame the pool is built until the bulk refine runs. It WAITS for a main view: it
	 *  descends against the metric, which needs one. Rooting the faces does not, and does not wait. */
	bool bRefinePending = false;

	struct FDistanceCullStatsState
	{
		bool bRequested = false;
		bool bEnabled = false;
		double RadiusCm = 0.0;
	};

	/** The subset of the drained counter block STATGROUP_ArcTerrain publishes. Drawn/Culled/SplitApplied/
	 *  MergeApplied are tallied on the GPU only while arc.Terrain.CBT.Stats is set and read 0 otherwise;
	 *  Live and PoolAllocated are written by load-bearing passes every frame. */
	struct FCounterStats
	{
		uint32 Live = 0;
		uint32 PoolAllocated = 0;
		uint32 Drawn = 0;
		uint32 Culled = 0;
		uint32 SplitApplied = 0;
		uint32 MergeApplied = 0;
		FDistanceCullStatsState DistanceCull;
	};
	FCounterStats CounterStats;
	/** CPU metadata captured beside the one in-flight general counter readback. */
	FDistanceCullStatsState PendingCounterDistanceCull;

	/** Non-blocking readback of the per-frame pool counters (pool-full events, twin validation). */
	TUniquePtr<class FRHIGPUBufferReadback> CounterReadback;
	bool bCounterReadbackPending = false;
	/** Invalidated in-flight general counter copies remain owned until their GPU fences complete. */
	TArray<TUniquePtr<class FRHIGPUBufferReadback>> RetiredCounterReadbacks;
	bool bWarnedPoolFull = false;
	bool bWarnedTwinValidation = false;
	uint64 BoundTerrainId = 0;

	static constexpr int32 GPUTimingRingSize = 16;
	static constexpr int32 GPUTimingMode1BoundaryCount = 2;
	enum class EGPUTimingBoundary : uint8
	{
		Begin,
		PreworkEnd,
		ClassifyEnd,
		MergeSelectEnd,
		SplitApplyEnd,
		MergeStageEnd,
		MergeCommitEnd,
		BaseRefineEnd,
		ReduceEnd,
		BatchEnd,
		CompactEnd,
		LinkValidateEnd,
		ReadbackTailEnd,
		Count
	};
	static constexpr int32 GPUTimingMode2BoundaryCount = static_cast<int32>(EGPUTimingBoundary::Count);
	static constexpr int32 GPUTimingStageCount = GPUTimingMode2BoundaryCount - 1;
	static constexpr int32 GPUTimingTopologyFirstStage =
		static_cast<int32>(EGPUTimingBoundary::ClassifyEnd);
	static constexpr int32 GPUTimingTopologyStageCount = 3;
	enum class EGPUTimingInlineDispatch : uint8
	{
		MergeSelect,
		SplitAdmissionPriority1,
		SplitAdmissionPriority2,
		SplitAdmissionPriority3,
		SplitAdmissionPriority4,
		SplitAdmissionPriority5,
		SplitAdmissionPriority6,
		SplitPreparePriority6,
		SplitPreparePriority5,
		SplitPreparePriority4,
		SplitPreparePriority3,
		SplitPreparePriority2,
		SplitPreparePriority1,
		SplitReserve,
		SplitApply,
		MergeStage,
		Count
	};
	static constexpr int32 GPUTimingInlineDispatchCount =
		static_cast<int32>(EGPUTimingInlineDispatch::Count);
	static constexpr int32 GPUTimingInlineQueryCount = GPUTimingInlineDispatchCount * 2;
	static constexpr int32 GPUTimingSplitPriorityPassCount = 6;
	enum class EGPUTimingInlineGroup : uint8
	{
		MergeSelect,
		SplitAdmission,
		SplitPrepare,
		SplitReserve,
		SplitApply,
		MergeStage,
		Count
	};
	static constexpr int32 GPUTimingInlineGroupCount =
		static_cast<int32>(EGPUTimingInlineGroup::Count);

	struct FGPUTimingSlot
	{
		TStaticArray<FRHIPooledRenderQuery, GPUTimingMode2BoundaryCount> Queries;
		TStaticArray<FRHIPooledRenderQuery, GPUTimingInlineQueryCount> InlineQueries;
		TUniquePtr<class FRHIGPUBufferReadback> CounterReadback;
		uint32 SourceFrame = MAX_uint32;
		uint64 TerrainId = 0;
		uint32 Generation = 0;
		uint8 Mode = 0;
		uint8 BoundaryCount = 0;
		uint8 MotionRefineIterations = 0;
		bool bShaderStats = false;
		bool bBulkRefine = false;
		bool bCounterReadbackExpected = false;
		bool bInlineDispatchTiming = false;
		bool bPending = false;
	};

	struct FGPUTimingAggregate
	{
		bool bActive = false;
		double StartTimeSeconds = 0.0;
		uint32 FirstSourceFrame = MAX_uint32;
		uint32 LastSourceFrame = 0;
		uint32 MaxReadbackLag = 0;
		uint32 MaxTotalSourceFrame = MAX_uint32;
		uint64 MaxTotalMicroseconds = 0;
		bool bMaxShaderStats = false;
		bool bMaxBulkRefine = false;
		bool bMaxHasCorrelatedCounters = false;
		bool bMaxHasInlineDispatchTiming = false;
		uint8 MaxMotionRefineIterations = 0;
		TStaticArray<uint64, GPUTimingStageCount> MaxTotalStageMicroseconds{};
		TStaticArray<uint64, GPUTimingInlineDispatchCount> MaxTotalDispatchMicroseconds{};
		TStaticArray<uint64, GPUTimingInlineGroupCount> MaxTotalDispatchGroupMicroseconds{};
		TStaticArray<uint32, FArcTerrainCBTBuffers::PoolCounterNum> MaxTotalCounters{};
		TArray<uint64> TotalMicroseconds;
		TArray<uint64> TopologyMicroseconds;
		TArray<uint64> TopologyActiveMicroseconds;
		TStaticArray<TArray<uint64>, GPUTimingStageCount> StageMicroseconds;
		TStaticArray<TArray<uint64>, GPUTimingInlineDispatchCount> DispatchMicroseconds;
		TStaticArray<TArray<uint64>, GPUTimingInlineGroupCount> DispatchGroupMicroseconds;
	};

	/** Pool lifetime extends to ReleaseRHI because ordinary terrain release can leave queries in flight. */
	FRenderQueryPoolRHIRef GPUTimingQueryPool;
	TStaticArray<FGPUTimingSlot, GPUTimingRingSize> GPUTimingSlots;
	FGPUTimingAggregate GPUTimingAggregate;
	int32 NextGPUTimingSlot = 0;
	uint32 GPUTimingGeneration = 1;
	uint32 GPUTimingDroppedSamples = 0;
	uint8 GPUTimingMode = 0;
	bool bWarnedGPUTimingUnsupported = false;
	bool bWarnedAsyncGPUTiming = false;
	bool bWarnedAsyncLatencyLog = false;

	/** Metadata is snapshotted beside the enqueue, never reconstructed when the GPU copy drains later. */
	struct FLatencySampleMetadata
	{
		uint32 EpisodeId = 0;
		uint32 SourceFrame = MAX_uint32;
		uint32 LoggerGeneration = 0;
		uint8 MotionCause = 0;
		uint8 MotionRefineRequestedIterations = 0;
		uint8 MotionRefineScheduledIterations = 0;
		float TurnDegrees = 0.0f;
		float TurnDegreesPerSecond = 0.0f;
		float MoveCm = 0.0f;
		float MoveCmPerSecond = 0.0f;
		FVector MoveDirection = FVector::ZeroVector;
		float ForwardSpeedCmPerSecond = 0.0f;
		float LateralSpeedCmPerSecond = 0.0f;
		FVector ViewOrigin = FVector::ZeroVector;
		FVector ViewDirection = FVector::ForwardVector;
		FVector2f ViewportSize = FVector2f::ZeroVector;
		float PixelsPerRadian = 0.0f;
		int32 PoolDepth = 0;
		uint32 PoolCapacity = 0;
		int32 NumFaces = 0;
		int32 MaxSubdivisionDepth = 0;
		int32 ResidentCapDepth = 0;
		float TargetTrianglePixels = 0.0f;
		float MaxTriangleEdgePixels = 0.0f;
		float TargetErrorPixels = 0.0f;
		float FrustumCullMargin = 0.0f;
		float TessFrustumCullMargin = 0.0f;
		bool bBulkRefineSubmitted = false;
		bool bFrozen = false;
		bool bListBoundObserved = false;
	};

	struct FLatencyReadbackSlot
	{
		TUniquePtr<FRHIGPUBufferReadback> Readback;
		FLatencySampleMetadata Metadata;
		bool bPending = false;
	};

	static constexpr int32 LatencyReadbackRingSize = 16;
	TStaticArray<FLatencyReadbackSlot, LatencyReadbackRingSize> LatencyReadbacks;
	int32 NextLatencyReadbackSlot = 0;

	/** In-flight copies from invalid logger generations; ownership moves here without waiting on the GPU. */
	static constexpr int32 RetiredLatencyReadbackLimit = 32;
	TArray<FLatencyReadbackSlot> RetiredLatencyReadbacks;
	bool bLatencyRetirementBlocked = false;

	struct FLatencyPendingGapRange
	{
		uint32 FirstFrame = MAX_uint32;
		uint32 LastFrame = MAX_uint32;
		uint32 ExplicitGapCount = 0;
		uint32 EpisodeId = 0;
		uint32 LoggerGeneration = 0;
		FString Reason;
		bool bOverflowMarker = false;
	};
	static constexpr int32 PendingLatencyGapRangeLimit = 32;
	TArray<FLatencyPendingGapRange> PendingLatencyGapRanges;

	/** Generation invalidates in-flight diagnostic copies when logging is disabled without waiting on GPU. */
	uint32 LatencyLoggerGeneration = 1;
	bool bLatencyLoggerWasEnabled = false;

	/** Shared motion-policy baseline, advanced even when the diagnostic latency logger is disabled. */
	FArcTerrainCBTMainView LastSubmittedMotionView;
	double LastSubmittedMotionTime = 0.0;
	bool bHasLastSubmittedMotionView = false;

	/** Separately armed so enabling the logger never compares against pre-enable camera history. */
	bool bLatencyMotionArmed = false;

	struct FLatencyEpisode
	{
		bool bActive = false;
		bool bExact = true;
		bool bHasSample = false;
		uint32 Id = 0;
		uint32 StartFrame = MAX_uint32;
		uint32 LastTriggerFrame = MAX_uint32;
		uint32 LastProcessedSourceFrame = MAX_uint32;
		uint32 ConsecutiveZeroDebtFrames = 0;
		uint32 SampleGaps = 0;
		uint8 Cause = 0;
		float PeakTurnDegrees = 0.0f;
		float PeakTurnDegreesPerSecond = 0.0f;
		float PeakMoveCmPerSecond = 0.0f;
		double DistanceCm = 0.0;
		uint32 PeakVisibleDebt = 0;
		uint32 PeakCappedDebt = 0;
		uint64 TotalSplitApplied = 0;
		uint64 TotalSplitDeferred = 0;
		uint64 TotalSplitRefused = 0;
		uint64 TotalMotionRefineScheduledIterations = 0;
		uint64 TotalMotionRefineApplied = 0;
		uint64 TotalMotionRefineDeferred = 0;
		uint64 TotalMotionRefineRefused = 0;
		uint8 MinMotionRefineRequested = MAX_uint8;
		uint8 MaxMotionRefineRequested = 0;
		bool bPoolFull = false;
		float PeakAreaRatio = 0.0f;
		float PeakEdgeRatio = 0.0f;
		float PeakErrorRatio = 0.0f;
		uint32 PeakAreaLevelEstimate = 0;
	};
	FLatencyEpisode LatencyEpisode;
	uint32 NextLatencyEpisodeId = 1;

	/** True until the frame after Initialize, when the pool init pass has run. */
	bool bPoolInitPending = false;
	bool bRebindUpdatePending = false;

	/** GFrameNumberRenderThread of the last frame that advanced the pool. */
	uint32 LastSubmittedFrame = MAX_uint32;

	/** GFrameNumberRenderThread of the last family entitled to advance the tree. A hit-proxy pass and a
	 *  capture-only family broadcast the same pre-render delegate but never set this, so neither can spend
	 *  the frame's single submission and leave the viewport's gather with a list it must refuse. */
	uint32 EligibleFamilyFrame = MAX_uint32;

	/** GFrameNumberRenderThread of the last gather that actually bound the compacted list. CBTStats/A
	 *  prints it, because a true culled count says nothing about whether anything drew the list. */
	uint32 LastCulledBindFrame = MAX_uint32;

	/** Keeps FArcTerrainCBTViewExtension registered for the lifetime of the pool. */
	TSharedPtr<FArcTerrainCBTViewExtension, ESPMode::ThreadSafe> ViewExtension;

	/** Last captured main view. STICKY: a frame that renders only scene captures leaves the previous
	 *  main view in place rather than dropping the tree onto a capture's frustum. Until the first one
	 *  arrives the classify pass and the bulk refine are both skipped and the tree holds its
	 *  triangulation — every other pass, rooting included, still runs. */
	FArcTerrainCBTMainView MainView;

	/** The eye-relative clip matrix as the last classify dispatch received it. Kept for the probe log:
	 *  it is composed on the CPU and no GPU counter can report it back. */
	FMatrix44f LastEyeLocalToClip = FMatrix44f::Identity;

	/** FPlatformTime::Seconds of the last arc.Terrain.CBT.Stats line and of the last no-main-view
	 *  warning — both conditions persist across frames, so both are throttled. */
	double LastStatsLogTime = 0.0;
	double LastNoMainViewWarnTime = 0.0;
};

/** Single global instance. */
extern TGlobalResource<FArcTerrainCBTRendererExtension> GArcTerrainCBTRendererExtension;
