// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcTerrainTypes.h"
#include "Preview/ArcTerrainPreviewManager.h"
#include "Render/ArcTerrainDensityTiers.h"
#include "Render/ArcTerrainTileAtlas.h"
#include "Tiles/ArcTerrainSourceCache.h"
#include "Tiles/ArcTerrainTileSet.h"
#include "ArcTerrainInvokers.h"
#include "ArcTerrainTileAccess.h"
#include "Containers/MpscQueue.h"
#include "IO/IoHash.h"
#include "Tasks/Task.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_EDITOR
#include "ArcTerrainTileBake.h" // FArcTerrainPendingTileBake / FArcTerrainTileRailState
#include "DataMaps/ArcTerrainDataMapBake.h"
#include "Weights/ArcTerrainWeightBuilder.h" // FArcTerrainWeightLayerIdentity
class UArcTerrainStampComponent;
#endif

#include "Tiles/ArcTerrainCellState.h"
#include "Collision/ArcTerrainCollisionPatch.h"
#include "Lighting/ArcTerrainLightingPatch.h"
#include "ArcTerrainSubsystemInternal.h"
#include "Render/ArcTerrainAtlasPrepare.h"
#include "Runtime/ArcTerrainRuntimeWorkCoordinator.h"
#include "Tiles/ArcTerrainStreamCache.h"
#include "ArcTerrainSubsystem.generated.h"

class AArcTerrain;
class FArcTerrainHeightfieldRef;
class FMaterialResource;
class FMaterialShaderMap;
class FSceneInterface;
class UArcTerrainCollisionComponent;
class UArcTerrainFarOcclusionComponent;
class UArcTerrainLightingComponent;
class UArcTerrainMeshModifier;
class UArcTerrainStampComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UArcTerrainSettings;
class UArcTerrainAtlasTexture;
class UStaticMeshComponent;
struct FArcTerrainAtlasSnapshot;
enum class EArcTerrainDataMap : uint8;
struct FArcTerrainDataMapBakeScope;

struct FArcTerrainBakeTileRef
{
	EArcTerrainRail Rail = EArcTerrainRail::Height;
	int32 Tier = 0;
	FIntPoint Key = FIntPoint::ZeroValue;

	bool operator==(const FArcTerrainBakeTileRef& Other) const
	{
		return Rail == Other.Rail && Tier == Other.Tier && Key == Other.Key;
	}
};

struct FArcTerrainResidencyCoverageSpan
{
	FIntPoint SectorKey = FIntPoint::ZeroValue;
	FIntPoint MinCell = FIntPoint::ZeroValue;
	FIntPoint MaxCell = FIntPoint::ZeroValue;
};

struct FArcTerrainSectorContentRecord
{
	TArray<FArcTerrainResidencyCoverageSpan> Spans;
	int32 RefCount = 0;
};

struct FArcTerrainWeightResidencyKey
{
	int32 Tier = 0;
	FIntPoint GroupKey = FIntPoint::ZeroValue;

	bool operator==(const FArcTerrainWeightResidencyKey& Other) const
	{
		return Tier == Other.Tier && GroupKey == Other.GroupKey;
	}
};

inline uint32 GetTypeHash(const FArcTerrainWeightResidencyKey& Key)
{
	return HashCombineFast(GetTypeHash(Key.GroupKey), static_cast<uint32>(Key.Tier));
}

struct FArcTerrainWeightResidencyRecord
{
	TMap<int32, int32> SourceDimCounts;
	int32 MaxSourceDim = 0;
};

struct FArcTerrainCachedInvokerWindow
{
	FIntPoint IntegerCentre = FIntPoint::ZeroValue;
	ArcTerrainSubsystemPrivate::FSectorWindow InnerWindow;
	ArcTerrainSubsystemPrivate::FSectorWindow OuterWindow;
};

struct FArcTerrainResidencySelectionCache
{
	TArray<FArcTerrainCachedInvokerWindow> Invokers;
	TArray<FVector> QuantizedPositions;
	uint64 ConfigRevision = 0;
	uint64 ContentIndexRevision = 0;
	uint64 LocalTileAuthorityRevision = 0;
	int32 SectorShift = INDEX_NONE;
	double PreviewRadius = 0.0;
	double PreviewUnloadRadius = 0.0;
};

struct FArcTerrainCollisionSelectionCache
{
	TArray<FArcTerrainCachedInvokerWindow> Invokers;
	TArray<FVector> QuantizedPositions;
	uint64 ConfigRevision = 0;
	double CollisionRadius = 0.0;
	double CollisionUnloadRadius = 0.0;
	double PatchSize = 0.0;
};

struct FArcTerrainCollisionReadyItem
{
	FArcTerrainCollisionKey Key = FArcTerrainCollisionKey::ZeroValue;
	double DistSq = 0.0;
};

struct FArcTerrainCollisionRetryItem
{
	double RetryAfter = 0.0;
	FArcTerrainCollisionKey Key = FArcTerrainCollisionKey::ZeroValue;
};

struct FArcTerrainLightingSelectionCache
{
	TArray<FVector> Positions;
	FTransform TerrainTransform;
	TWeakObjectPtr<UMaterialInterface> Material;
	const FMaterialResource* MaterialResourceIdentity = nullptr;
	const FMaterialShaderMap* MaterialShaderMapIdentity = nullptr;
	FGuid MaterialId;
	double LightingRadius = 0.0;
	double UnloadRadius = 0.0;
	double CellSize = 0.0;
	double NearBandEdge = 0.0;
	double NearSinkBandWidth = 0.0;
	float Hysteresis = 1.0f;
	uint64 ConfigGeneration = 0;
	bool bHasInvoker = false;
};

struct FArcTerrainLightingReadyItem
{
	FArcTerrainCellKey Key = FArcTerrainCellKey::ZeroValue;
	double DistSq = 0.0;
};

struct FArcTerrainLightingRetryItem
{
	double RetryAfter = 0.0;
	FArcTerrainCellKey Key = FArcTerrainCellKey::ZeroValue;
};

struct FArcTerrainFarBlockSelectionCache
{
	TArray<FVector> Positions;
	FTransform TerrainTransform;
	double FarRadius = 0.0;
	double BlockSize = 0.0;
	double FarSinkBandWidth = 0.0;
	float Hysteresis = 1.0f;
	int32 BlockCells = 0;
	uint64 ConfigGeneration = 0;
	bool bHasInvoker = false;
};

struct FArcTerrainFarBlockReadyItem
{
	FArcTerrainFarBlockKey Key = FArcTerrainFarBlockKey::ZeroValue;
	double DistSq = 0.0;
};

struct FArcTerrainFarBlockRetryItem
{
	double RetryAfter = 0.0;
	FArcTerrainFarBlockKey Key = FArcTerrainFarBlockKey::ZeroValue;
};

enum class EArcTerrainTileAuthorityReason : uint8
{
	TileReleased,
	TileArrived,
	PendingHeightChanged,
	PendingHeightReset,
	StampBoundsChanged,
	LayoutChanged,
	Count
};

enum class EArcTerrainPipelineAtlasEffect : uint8
{
	HeightDirtied,
	WeightDirtied,
	AdmissionWoken,
	Count
};

enum class EArcTerrainPipelineAtlasIgnore : uint8
{
	StampBoundsOrData,
	GridUnavailable,
	TierInvalid,
	NoMatchingTarget,
	AlreadySatisfied,
	Count
};

enum class EArcTerrainPipelineCollisionEnqueue : uint8
{
	Enqueued,
	AlreadyQueued,
	HeightSourceMissing,
	Count
};

struct FArcTerrainPipelineCounters
{
	uint64 StreamingSkippedAlreadyLoaded = 0;
	uint64 StreamingAsyncRequests = 0;
	uint64 StreamingValidCallbacks = 0;
	uint64 StreamingArrivalEntries = 0;
	uint64 AuthorityProduced[static_cast<int32>(EArcTerrainTileAuthorityReason::Count)] = {};
	uint64 ResidencyProcessEpochObservations = 0;
	uint64 ResidencyLocalEpochObservations = 0;
	uint64 ResidencyReconcileChanged = 0;
	uint64 ResidencyReconcileUnchanged = 0;
	uint64 AtlasAuthorityConsumed[static_cast<int32>(EArcTerrainTileAuthorityReason::Count)] = {};
	uint64 AtlasEffects[static_cast<int32>(EArcTerrainPipelineAtlasEffect::Count)] = {};
	uint64 AtlasIgnored[static_cast<int32>(EArcTerrainPipelineAtlasIgnore::Count)] = {};
	uint64 CollisionSyncAttempts = 0;
	uint64 CollisionQueuedAttempts = 0;
	uint64 CollisionDequeued = 0;
	uint64 CollisionEnqueue[static_cast<int32>(EArcTerrainPipelineCollisionEnqueue::Count)] = {};
	uint64 CollisionCountSaturated = 0;
	uint64 CollisionDeadlineSaturated = 0;
};

struct FArcTerrainPipelineTelemetry
{
	uint64 StateAgeTicks = 0;
	uint64 ConsumedLoadedMutationSerial = 0;
	uint64 ConsumedCollisionCountSaturationSerial = 0;
	uint64 ConsumedCollisionDeadlineSaturationSerial = 0;
	FArcTerrainPipelineCounters Interval;
	FArcTerrainPipelineCounters Lifetime;

	void Increment(uint64 FArcTerrainPipelineCounters::* Counter)
	{
		++(Interval.*Counter);
		++(Lifetime.*Counter);
	}
	void IncrementBy(uint64 FArcTerrainPipelineCounters::* Counter, uint64 Delta)
	{
		Interval.*Counter += Delta;
		Lifetime.*Counter += Delta;
	}

	void IncrementAuthorityProduced(EArcTerrainTileAuthorityReason Reason);
	void IncrementAtlasAuthorityConsumed(EArcTerrainTileAuthorityReason Reason);
	void IncrementAtlasEffect(EArcTerrainPipelineAtlasEffect Effect);
	void IncrementAtlasIgnored(EArcTerrainPipelineAtlasIgnore Reason);
	void IncrementCollisionEnqueue(EArcTerrainPipelineCollisionEnqueue Outcome);
	void ResetInterval() { Interval = FArcTerrainPipelineCounters(); }
};

struct FArcTerrainPipelineSnapshot
{
	FString WorldLabel;
	FString TerrainPath;
	uint64 StateAgeTicks = 0;
	FArcTerrainPipelineCounters Interval;
	FArcTerrainPipelineCounters Lifetime;
	FArcTerrainTileStoreStats Store;
	int32 ResidentHeightByTier[ArcTerrain::MaxTiers] = {};
	int32 ResidentWeightByTier[ArcTerrain::MaxTiers] = {};
	int32 PendingAtlasAuthorityEvents = 0;
	int32 WokenHeightAdmissions = 0;
	int32 WokenWeightAdmissions = 0;
	int32 DirtyHeightSectors = 0;
	int32 DirtyWeightGroups = 0;
	int32 DirtyAtlasCells = 0;
	int32 PendingRefillCells = 0;
	int32 AtlasInFlight = 0;
	int32 PendingAtlasFills = 0;
	bool bCbtTopologyInFlight = false;
	bool bCbtUnpublished = false;
	bool bResidencyRevisionLag = false;
	bool bDesiredAdmissionRevisionLag = false;
	bool bLayoutGenerationLag = false;
	bool bRenderKnobRevisionLag = false;
	int32 PendingHeightSourceReady = 0;
	int32 PendingHeightSourceMissing = 0;
	int32 AdmittedHeightReady = 0;
	int32 AdmittedHeightAwaiting = 0;
	int32 StaleSleeping = 0;
	int32 CellsCompositePublished = 0;
	int32 CellsCompositeLagging = 0;
	int32 CellsTilePublished = 0;
	int32 CellsTileLagging = 0;
	int32 CellsRefillPending = 0;
	int32 CellsCompositeRetry = 0;
	bool bCbtPublished = false;
	bool bCbtResourceGenerationMatch = false;
	int32 CollisionPatches = 0;
	int32 CollisionReadySet = 0;
	int32 CollisionReadyOrdered = 0;
	int32 CollisionRetry = 0;
	int32 CollisionAuthorityEvents = 0;
	int32 CollisionComponents = 0;
	int32 CollisionHeightSourceLatches = 0;
	int32 CollisionEligibleButUnqueued = 0;
	int32 CollisionBudgetCount = 0;
	float CollisionBudgetMs = 0.0f;
};

struct FArcTerrainTileAuthorityEvent
{
	EArcTerrainRail Rail = EArcTerrainRail::Height;
	int32 Tier = 0;
	FIntPoint TileKey = FIntPoint::ZeroValue;
	EArcTerrainTileAuthorityReason Reason = EArcTerrainTileAuthorityReason::TileArrived;
};

void PublishLocalTileAuthorityEvent(FArcTerrainState& State, const FArcTerrainTileAuthorityEvent& Event);

struct FArcTerrainAtlasSchedule
{
	uint64 LayoutGeneration = 1;
	uint64 ResidencyPublicationRevision = 0;
	uint64 ConsumedResidencyPublicationRevision = 0;
	uint64 DesiredAdmissionRevision = 0;
	uint64 ConsumedDesiredAdmissionRevision = 0;
	uint64 ConsumedLayoutGeneration = 0;
	uint64 RenderKnobRevision = 1;
	uint64 ConsumedRenderKnobRevision = 0;

	TSet<FArcTerrainSectorId> PendingHeightAdmissions;
	TSet<FArcTerrainSectorId> DirtyHeightSectors;
	TSet<FArcTerrainSectorId> RefusedHeightSectors;
	TSet<FArcTerrainSectorId> WokenHeightAdmissions;

	TSet<FArcTerrainWeightGroupId> PendingWeightAdmissions;
	TSet<FArcTerrainWeightGroupId> DirtyWeightGroups;
	TSet<FArcTerrainWeightGroupId> RefusedWeightGroups;
	TSet<FArcTerrainWeightGroupId> WokenWeightAdmissions;

	TSet<FArcTerrainCellKey> DirtyAtlasCells;
	TSet<FArcTerrainAtlasInFlightKey> InFlight;
	bool bCbtTopologyInFlight = false;
	uint64 PendingCbtTopologySignature = 0;
	uint64 PendingCbtLayoutGeneration = 0;
};

struct FArcTerrainCBTTopologyCache
{
	uint64 Signature = 0;
	int32 NumFaces = 0;
	int32 FaceQuads = 0;
	TArray<uint32> Neighbors;
	TArray<FIntPoint> FaceTexelOrigins;
};

struct FArcTerrainCBTPublishCache
{
	uint64 TopologySignature = 0;
	uint64 ResourceGeneration = 0;
	uint64 RenderKnobRevision = 0;
	FMatrix LocalToWorld = FMatrix::Identity;
	FArcTerrainResidencyRings Rings;
	bool bPublished = false;
};

#if WITH_EDITOR
struct FArcTerrainBakeCompletionToken
{
	TWeakObjectPtr<AArcTerrain> Terrain;
	FArcTerrainBakeTileRef Ref;
	uint64 ReservationId = 0;
};
#endif

/** One registered stamp. (Composite-order Priority ties break by object path name — stable across
 *  sessions — not by registration order.) */
struct FArcTerrainStampEntry
{
	TWeakObjectPtr<UArcTerrainStampComponent> Stamp;
	FBox Bounds = FBox(ForceInit);
	TArray<FArcTerrainBakeTileRef> CoveredTiles;
	TArray<FArcTerrainResidencyCoverageSpan> ResidencyCoverage;
	TArray<FArcTerrainCollisionKey> CollisionCoveredPatches;
	int32 CachedMaxWeightSourceDim = 0;
};

inline uint32 GetTypeHash(const FArcTerrainBakeTileRef& Ref)
{
	return HashCombineFast(
		HashCombineFast(GetTypeHash(Ref.Key), static_cast<uint32>(Ref.Tier)),
		static_cast<uint32>(Ref.Rail));
}

/** One sector granted residency this tick, in admission order (pinned first, then nearest). */
struct FArcTerrainResidentSector
{
	FArcTerrainSectorId Id;

	/** Nearest invoker distance² of any cell in the sector (world cm²). Above tier 0 there are no cells:
	 *  it is the distance to the sector's own rectangle. */
	double DistSq = TNumericLimits<double>::Max();

	/** Any cell in the sector is edit-pinned. A pin promotes the WHOLE sector — the pin set is exempt
	 *  from the residency cap, which is exactly why the atlas needs its own refusal policy. */
	bool bPinned = false;
};

/** One weight group granted residency this tick. Its own budget and its own ranking: the weight raster is
 *  16x denser per unit area than the height raster, so it streams on a shorter radius of its own. */
struct FArcTerrainResidentWeightGroup
{
	int32 Tier = 0;

	FIntPoint Key = FIntPoint::ZeroValue;

	/** Nearest invoker distance² of any cell in the group (world cm²). */
	double DistSq = TNumericLimits<double>::Max();

	bool bPinned = false;
};

/** Everything the subsystem tracks for one terrain actor. */
struct FArcTerrainState
{
	FArcTerrainPipelineTelemetry PipelineTelemetry;
	TArray<FArcTerrainStampEntry> Stamps;
	uint64 MeshModifierRevision = 1;
	TMap<FArcTerrainCellKey, FArcTerrainCellState> Cells;

	/** One record per entry of Cells, contiguous. NO PHASE WALKS IT: every phase takes its cells from one
	 *  of the bounded sets below, so tick cost follows what is visible and what changed, never how much
	 *  terrain exists. */
	TArray<FArcTerrainCellHot> HotCells;

	/** Keys of the cells carrying an edit pin. THE ONLY PRODUCER OF PINNED SECTORS: the residency phase
	 *  derives its wanted set from invoker geometry alone and never scans for a pin, so a pin outside
	 *  the ring reaches residency through this set or not at all. Written by PinCellsInBounds; entries
	 *  whose PinnedUntil has expired (or whose cell is gone) are dropped by the residency phase. */
	TSet<FArcTerrainCellKey> PinnedCells;

	/** HotCells indices of the LIVE cells of each sector, keyed at SectorCellsShift. It answers how near
	 *  the nearest of a sector's live cells is, and — since a cell lives exactly as long as its sector is
	 *  granted — it is also the residency phase's TEARDOWN DOMAIN: its size follows MaxResidentSectors,
	 *  not the authored content. Maintained by FindOrAddCell / RemoveCell / EmptyCells. */
	TMap<FIntPoint, TArray<int32>> SectorCells;

	/** SectorShift SectorCells is keyed at (INDEX_NONE = never built). The resolved shift can move under
	 *  a device-limit clamp or an authored edit, which re-derives the map from HotCells. */
	int32 SectorCellsShift = INDEX_NONE;

	/** THE COLLISION REGISTRY, on the CollisionPatchExtent lattice and disjoint from Cells: every patch
	 *  holding a heightfield, mesh-modifier bodies or a pending build. It is also the collision phase's
	 *  teardown domain — a patch that leaves the ring is reached through this map, never through the
	 *  invoker window, which no longer names it. Pruned by that phase. */
	TMap<FArcTerrainCollisionKey, FArcTerrainCollisionPatch> CollisionPatches;

	/** THE LIGHTING REGISTRY, on the CELL lattice and disjoint from Cells: every cell holding a lighting
	 *  primitive or a pending build. Also the lighting phase's teardown domain — a cell that leaves the
	 *  ring is reached through this map. */
	TMap<FArcTerrainCellKey, FArcTerrainLightingPatch> LightingPatches;

	/** THE FAR-OCCLUSION REGISTRY, on the BLOCK lattice: every block holding an occlusion primitive or a
	 *  pending build, and the far ring's teardown domain. */
	TMap<FArcTerrainFarBlockKey, FArcTerrainFarBlockPatch> FarBlockPatches;

	/** Keys an edit touched since the bake phase last drained them, deduped by FArcTerrainCellHot::
	 *  bQueued. MarkBoundsDirty is the producer and expands its touched set by ONE CELL: the composite's
	 *  apron reuses the neighbour's global texel indices, so an edit at a cell edge moves texels the
	 *  neighbour owns.
	 *
	 *  What it buys a cell is the tick of survival its bQueued bit grants against the residency phase's
	 *  teardown — long enough for the edit's pin to promote the sector. The tile rails do NOT read it:
	 *  they bake on their own lattice, out of the three pending-tile sets. */
	TArray<FArcTerrainCellKey> DirtyQueue;

	/** Indices into HotCells of the cells whose sector is resident, published by phase 1 AFTER its
	 *  evictions — RemoveCell relocates a record by RemoveAtSwap, so a list captured before them could
	 *  name a cell that moved. It is also the AUTHORITY on which cells carry bResident: phase 1 clears the
	 *  bit over the previous tick's list before granting the new one, which only holds because eviction
	 *  (the sole RemoveCell caller) runs inside that same phase. */
	TArray<int32> ResidentCells;

	/** Cell keys whose atlas weights have to be written again from a tile that arrived after their group was
	 *  admitted. THE MARK FOR GROUND WITH NO CELL — a cell carries its own
	 *  (FArcTerrainCellState::bAtlasRefillPending), and ground drawn from bytes alone has nowhere to. Filled
	 *  by OnStreamedTileArrived, consumed and emptied by the atlas pass. */
	TSet<FArcTerrainCellKey> PendingRefillCells;

	FArcTerrainCellHot& Hot(const FArcTerrainCellState& Cell)
	{
		return HotCells[Cell.HotIndex];
	}

	const FArcTerrainCellHot& Hot(const FArcTerrainCellState& Cell) const
	{
		return HotCells[Cell.HotIndex];
	}

	/** The only three ways Cells/HotCells may change size — each keeps the two directions of the index
	 *  correspondence in step. Defined in ArcTerrainSubsystem.cpp. */
	FArcTerrainCellState& FindOrAddCell(FArcTerrainCellKey Key);
	void RemoveCell(FArcTerrainCellKey Key);
	void EmptyCells();

	/** Appends Key to DirtyQueue unless its cell is absent or already queued. */
	void EnqueueCell(FArcTerrainCellKey Key);

	/** Sectors granted residency by the last Tick's phase 1: tier 0 first (pinned, then nearest), then each
	 *  coarse tier farthest-first. Phase 2b admits them in this order, so eviction sheds the far end — and
	 *  tier 0 going first is what stops a full pool of coarse sectors being admitted and immediately
	 *  dropped by the tier-0 candidates behind them. */
	TArray<FArcTerrainResidentSector> ResidentSectors;

	/** Weight groups granted residency by the last Tick's phase 1, sorted pinned-first then nearest-first. */
	TArray<FArcTerrainResidentWeightGroup> ResidentWeightGroups;

	/** Sector -> atlas slice registry + refusal policy. Created lazily on the first atlas pass. */
	TSharedPtr<FArcTerrainTileAtlas> TileAtlas;

	/** THE resolved density ladder, republished by TickTileStreaming from the layout it already resolves and
	 *  CLEARED wherever it cannot be — no invoker, or a config edit. A tier absent from it does not exist:
	 *  no shift, no lattice, no tile. */
	FArcTerrainTierLadder TierLadder;

	FArcTerrainWeightLadder WeightLadder;

	TMap<int32, int32> WeightSourceDimCounts;
	int32 MaxWeightSourceDim = 0;
	int32 ResolvedWeightQuads = 0;
	uint64 DerivedGridRevision = 0;
	uint64 StreamLayoutRevision = 0;
	bool bDerivedGridRebuildPending = false;
	FArcTerrainStreamCache StreamCache;

	TMap<FIntPoint, FArcTerrainSectorContentRecord> SectorContentIndex;
	uint64 ContentIndexRevision = 0;
	TMap<FArcTerrainWeightResidencyKey, FArcTerrainWeightResidencyRecord> WeightResidencyIndex;
	int32 IndexedSectorShift = INDEX_NONE;
	int32 IndexedWeightShifts[ArcTerrain::MaxTiers] = {};
	int32 NumIndexedWeightTiers = 0;
	FArcTerrainResidencySelectionCache ResidencySelectionCache;
	uint32 ConsumedResidencyManifestEpoch = 0;
	uint32 ConsumedResidencyLoadedEpoch = 0;
	TArray<FArcTerrainTileAuthorityEvent> PendingTileAuthorityEvents;
	TArray<FArcTerrainTileAuthorityEvent> PendingAtlasAuthorityEvents;
	FArcTerrainAtlasSchedule AtlasSchedule;
	FArcTerrainCBTTopologyCache CbtTopologyCache;
	FArcTerrainCBTPublishCache CbtPublishCache;
	uint64 LocalTileAuthorityRevision = 0;
	uint64 ResidencyConfigRevision = 0;
	TSet<FArcTerrainCellKey> DeferredResidencyEvictions;
	TSet<FArcTerrainCellKey> ReadyResidencyEvictions;
	TSet<FArcTerrainCellKey> PendingResidencyAdmissions;
	TSet<FArcTerrainCellKey> CompositeReadyCells;
	TSet<FArcTerrainCellKey> CompositeRetryCells;
	double NextPinExpiry = 0.0;
	double NextCompositeRetry = 0.0;
	uint64 ResidentSetRevision = 0;
	bool bPinMembershipDirty = false;
	TArray<ArcTerrainSubsystemPrivate::FWantedSector> CachedWantedSectors;
	TMap<FArcTerrainSectorId, int32> CachedWantedSectorIndex;
	TArray<ArcTerrainSubsystemPrivate::FWantedWeightGroup> CachedWantedWeightGroups;
	TMap<FArcTerrainWeightGroupId, int32> CachedWantedWeightIndex;

	TMap<TObjectKey<UArcTerrainStampComponent>, int32> StampByObject;
	TMap<FArcTerrainCollisionKey, TArray<TWeakObjectPtr<UArcTerrainMeshModifier>>> CollisionMeshModifiers;
	FArcTerrainCollisionSelectionCache CollisionSelectionCache;
	TSet<FArcTerrainCollisionKey> CollisionWindowUnion;
	TSet<FArcTerrainCollisionKey> CollisionBootstrapKeys;
	TSet<FArcTerrainCollisionKey> CollisionReadySet;
	TArray<FArcTerrainCollisionReadyItem> CollisionReadyOrdered;
	TArray<FArcTerrainCollisionRetryItem> CollisionRetryHeap;
	TArray<FArcTerrainTileAuthorityEvent> PendingCollisionAuthorityEvents;
	uint64 NextCollisionPatchGeneration = 1;
	uint32 CollisionTerrainId = 0;
	double NextCollisionRetry = 0.0;
	bool bCollisionBootstrapActive = false;
	bool bCollisionRanksDirty = false;

	FArcTerrainLightingSelectionCache LightingSelectionCache;
	TSet<FArcTerrainCellKey> LightingWindowUnion;
	TSet<FArcTerrainCellKey> LightingReadySet;
	TArray<FArcTerrainLightingReadyItem> LightingReadyOrdered;
	TArray<FArcTerrainLightingRetryItem> LightingRetryHeap;
	TArray<FArcTerrainTileAuthorityEvent> PendingLightingAuthorityEvents;
	TMap<FIntPoint, TSet<FArcTerrainCellKey>> LightingHeightTileDependents;
	TMap<FIntPoint, TSet<FArcTerrainCellKey>> LightingWeightTileDependents;
	uint64 NextLightingPatchGeneration = 1;
	uint64 LightingConfigGeneration = 1;
	uint64 LightingMaterialRevision = 1;
	double NextLightingRetry = 0.0;
	bool bLightingRanksDirty = false;

	FArcTerrainFarBlockSelectionCache FarBlockSelectionCache;
	TSet<FArcTerrainFarBlockKey> FarBlockWindowUnion;
	TSet<FArcTerrainFarBlockKey> FarBlockReadySet;
	TArray<FArcTerrainFarBlockReadyItem> FarBlockReadyOrdered;
	TArray<FArcTerrainFarBlockRetryItem> FarBlockRetryHeap;
	TArray<FArcTerrainTileAuthorityEvent> PendingFarAuthorityEvents;
	TMap<FIntPoint, TSet<FArcTerrainFarBlockKey>> FarHeightTileDependents;
	uint64 NextFarBlockGeneration = 1;
	uint64 FarBlockConfigGeneration = 1;
	double NextFarBlockRetry = 0.0;
	bool bFarBlockRanksDirty = false;

	FArcTerrainRenderOwnerIdentity RenderOwner;
	FArcTerrainRenderIdentity PublishedRenderIdentity;
	uint64 ResourceRevision = 0;
	uint64 LastObservedRhiEpoch = 0;

#if WITH_EDITOR
	/** Per-rail, per-tier tile-bake bookkeeping, indexed by EArcTerrainRail then tier. The in-flight bake
	 *  lives HERE and not on a cell: the written unit is the tile. */
	FArcTerrainTileRailState TileRails[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers];

	TMap<FArcTerrainBakeTileRef, TArray<TWeakObjectPtr<UArcTerrainStampComponent>>> Contributors;

	TMap<FArcTerrainBakeTileRef, uint32> PendingTileInvalidations;

	TArray<ArcTerrainDataMaps::FArcTerrainDataMapWindow> DataMaskWindows;
	FIoHash DataMaskWindowFingerprint;
	uint32 DataWindowRevision = 0;

	int32 BakeLaneCursor = 0;

	/** Monotonic terrain-local pass count used to defer a newly dirtied key without scanning the queue. */
	uint64 BakePass = 0;

	/** Last resolved settle count; producers arm new keys against the next TickTerrainTileBakes pass. */
	int32 BakeSettleTicks = 30;

	/** Active authored-height contract violations already reported, per height tier. A compliant rewrite
	 *  removes its key and rearms the warning; changing the resolved contract clears every tier. */
	TSet<FIntPoint> WarnedHeightBoundsTiles[ArcTerrain::MaxTiers];
	FVector2f WarnedHeightBoundsRangeCm = FVector2f::ZeroVector;
	bool bHeightBoundsWarningRangeInitialized = false;

	/** THE BAKE DOMAIN: the tiles an edit reached and the rail has not answered for.
	 *  Keyed on the TILE because a cell need not exist under it — the bake follows what the modifier stack
	 *  moved, never where the camera is. An entry leaves only when its
	 *  rail reaches a decision (a written tile, a dropped one, a latched refusal) or when the gather the
	 *  recipe asks for cannot complete. A budget refusal leaves the entry, so the next tick takes it again.
	 *
	 *  OVERLAPPING FArcTerrainTileRailState::Pending, which answers a different question: these sets say
	 *  what is to be done, that map says what is already running. A tile whose evaluation is in flight is
	 *  in BOTH until its payload is consumed — nothing removes it here when the work starts — which is what
	 *  the candidate list's Seen dedup exists for. */
	TSet<FIntPoint> PendingHeightTiles[ArcTerrain::MaxTiers];
	TSet<FIntPoint> PendingWeightTiles[ArcTerrain::MaxTiers];
	TSet<FIntPoint> PendingDataTiles;

	TSet<FIntPoint>& PendingTiles(EArcTerrainRail Rail, int32 Tier);
#endif
};

namespace ArcTerrainSubsystemPrivate
{
	/** Per-terrain resolved residency knobs: the authored AArcTerrain properties plus the derivation and
	 *  clamp math every consumer would otherwise repeat. */
	struct FResolvedResidency
	{
		double PreviewRadius = 0.0;
		double PreviewUnloadRadius = 0.0; // PreviewRadius * max(Hysteresis, 1)
		double CollisionRadius = 0.0;     // min(authored, PreviewRadius) — ENTRY only
		double CollisionUnloadRadius = 0.0; // CollisionRadius * max(Hysteresis, 1) — SUSTAIN only
		int32 MaxResidentSectors = 0;     // ArcTerrain::SumAuthoredResidentSectors — the WHOLE ladder's ask
		int32 MaxResidentWeightCells = 0; // max(authored, 1) — its own budget, not a multiple of the above
	};

	/** THE residency-radius authority. Exported so a diagnostic reads the radii the phases actually run
	 *  on instead of re-deriving them from the actor's properties. */
	ARCTERRAIN_API FResolvedResidency ResolveResidency(const AArcTerrain& Terrain);

	/** THE ONE ATLAS-LAYOUT AUTHORITY. Exported because the tier ladder resolves against the LAYOUT's
	 *  sector shift and sector budget, not the actor's — both come back lowered. */
	ARCTERRAIN_API FArcTerrainTileAtlasLayout ResolveTerrainAtlasLayout(const AArcTerrain& Terrain,
		const FArcTerrainState& State, const FResolvedResidency& Residency);

	/** Key window covering every unit rect that can lie within WorldRadius of the invoker, on a lattice of
	 *  CellSize << SectorShift. The local radius uses the terrain's SMALLEST XY axis scale, so a non-uniform
	 *  scale widens the window rather than clipping it — the per-cell distance test inside is what admits a
	 *  sector. MaxSpan is the caller's own bound on the walk, in keys either side of the invoker. */
	ARCTERRAIN_API FSectorWindow ComputeSectorWindow(const FVector& WorldInvoker, double WorldRadius,
		int32 SectorShift, double CellSize, const FTransform& TerrainTransform, double MaxSpan);

	/** Test-only accessor (weightmap-decouple Stage 2 Step 8): exposes the production
	 *  ArcTerrainSubsystem.cpp resolver for deterministic tests. Runtime code (not editor-only —
	 *  CompositeCell calls this in PIE/game too). Takes NO FArcTerrainCellKey — the terrain-wide
	 *  weight-quad resolution is Key-independent BY CONSTRUCTION (the cross-cell seam invariant I1
	 *  the plan's non-pow2-seam test exercises). */
	ARCTERRAIN_API int32 ResolveWeightQuads(const FArcTerrainConfig& Config, const FArcTerrainState& State);

	/** Test-only accessor (weightmap-decouple Stage 2 Step 8): the PURE formula BuildCompositeJob uses
	 *  for both the height and weight tiles' global apron-texel origin (Key * QuadsPerSide - 1). Exposed
	 *  so the cross-cell seam invariant (I1 — shared border texel takes the SAME global index in both
	 *  neighbouring cells at a uniform, non-power-of-two QuadsPerSide) is testable without the GPU/stamp
	 *  machinery the rest of BuildCompositeJob needs. */
	ARCTERRAIN_API FIntPoint ComputeGlobalTexelOrigin(FArcTerrainCellKey Key, int32 QuadsPerSide);

	/** Inclusive cell rectangle one stamp covers. */
	struct FContentCellRange
	{
		FIntPoint Min = FIntPoint::ZeroValue;
		FIntPoint Max = FIntPoint::ZeroValue;
	};

	/** THE weight-ring candidate rule, per ladder tier: tier 0 walks the WeightSliceShift lattice on its
	 *  band pair, qualifies a group on intersecting ContentRanges and promotes an edit-pinned group
	 *  regardless of distance; a coarse tier walks its own SliceShift lattice on its band and qualifies a
	 *  group on its coarse tile existing. Exported so the selection is testable without an atlas or a GPU. */
	ARCTERRAIN_API void GatherWantedWeightGroups(const AArcTerrain& Terrain,
		const FArcTerrainState& State, const FArcTerrainTileAtlasLayout& Layout,
		const FArcTerrainWeightLadder& WeightLadder, const FTransform& TerrainTransform,
		TConstArrayView<FVector> InvokerPositions, TConstArrayView<FContentCellRange> ContentRanges,
		double Now, TArray<FWantedWeightGroup>& OutGroups);

	/** THE HALF-TEXEL SAMPLE MAP, the one authority: footprint UV [0,1] -> the texel centers of a Dims
	 *  raster. Exported because the data-map bake must map a stamp's raster exactly as the composite
	 *  shader's inputs do. */
	ARCTERRAIN_API void ComputeSampleMap(FIntPoint Dims, FVector2f& OutUVScale, FVector2f& OutUVBias);

	/** THE COMPOSITE RASTER AUTHORITY, exposed (GPU-grass plan Step 0A). Quads per side of every
	 *  cell's composited height tile — tile texels = this + 3. Exported so no consumer re-derives
	 *  the cell extent over the texel size and creates a second raster authority. */
	ARCTERRAIN_API int32 CompositeQuads(const FArcTerrainConfig& Config);

	/** Stamps intersecting CellBounds (edge-inclusive; disabled/stale skipped), sorted ascending by
	 *  Priority with object-path tie-break — the composite order. The
	 *  single authority on that order; a second gather that disagreed would key a cell on one order
	 *  and build it in another. */
	ARCTERRAIN_API void GatherSortedStamps(const FArcTerrainState& State, const FBox& CellBounds,
		TArray<UArcTerrainStampComponent*>& OutStamps);
	ARCTERRAIN_API bool IsStampOrderedBefore(const UArcTerrainStampComponent& A,
		const UArcTerrainStampComponent& B);

#if WITH_EDITOR
	/** Every weight-layer ChannelIndex a stamp covering Bounds resolves against the terrain's channel set,
	 *  de-duped — the channels the weight composite over that area writes. */
	ARCTERRAIN_API void GatherPresentWeightChannels(const FArcTerrainConfig& Config,
		TConstArrayView<UArcTerrainStampComponent* const> SortedStamps, TArray<int32>& OutPresentChannels);

	/** One entry per (covering stamp, layer) pair over Bounds that resolves to a channel — the
	 *  texturing-only key ingredients ComputeCellCollisionKey excludes. Stable by channel-name hash, NOT
	 *  de-duped. */
	ARCTERRAIN_API void GatherPresentWeightLayerIdentities(const FArcTerrainConfig& Config,
		TConstArrayView<UArcTerrainStampComponent* const> SortedStamps,
		TArray<ArcTerrainWeights::FArcTerrainWeightLayerIdentity>& OutIdentities);

	void GatherBakeTileRefsForBounds(const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FBox& LocalBounds, TArray<FArcTerrainBakeTileRef>& OutRefs);
#endif
}

/**
 * World subsystem driving the raster terrain backend: owns the per-terrain cell states, the
 * render-target pool + GPU stamp-source cache, the invoker-driven residency rings and the
 * per-PATCH heightfield collision pipeline (its own lattice, no cell required). Each Tick: gather
 * invokers -> preview ring residency (LRU-capped, farthest-first eviction) -> deadline+count-budgeted
 * recomposite of dirty resident cells (nearest first) -> collision ring drive. Runs in Game, PIE and
 * the editor (pattern: UArcTQSQuerySubsystem).
 */
UCLASS()
class ARCTERRAIN_API UArcTerrainSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Out-of-line destructor: keeps the state maps' element destructors out of the UHT gen.cpp.
	 *  Defaulted in the .cpp. */
	virtual ~UArcTerrainSubsystem() override;

	//~ Begin USubsystem / FTickableGameObject interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End USubsystem / FTickableGameObject interface

	/** Terrain lifecycle (wired from AArcTerrain::PostRegisterAllComponents / Destroyed). */
	void RegisterTerrain(AArcTerrain& Terrain);
	void UnregisterTerrain(AArcTerrain& Terrain);
	void NotifyLocalTileAuthority(const AArcTerrain& Terrain, const FArcTerrainTileAuthorityEvent& Event);

	/** Live terrain-material swap (WITH_EDITOR, from AArcTerrain::PostEditChangeProperty): drop the
	 *  preview manager's cached per-terrain resolution and rebind this terrain's cells' MIDs against
	 *  the newly-resolved material — no map reload. */
	void OnTerrainMaterialChanged(AArcTerrain& Terrain);

	/** Live Config flush (WITH_EDITOR, from AArcTerrain::PostEditChangeProperty while still unlocked):
	 *  the spatial layout changed under the terrain's cached cells, so release every cell's tiles +
	 *  collision + preview and empty the cell set. The next Tick re-seeds flat cells against the new
	 *  Config. No-op once the terrain locks (Config is frozen at first stamp). */
	void OnTerrainConfigChanged(AArcTerrain& Terrain);

	/** Live collision-raster change (WITH_EDITOR, from AArcTerrain::PostEditChangeProperty): drop the
	 *  whole patch registry so phase 3 rebuilds it on the new lattice. Nothing dirties a patch when only
	 *  CollisionPatchQuads/CollisionPatchExtent move, so already-built patches would otherwise keep the
	 *  previous raster indefinitely — and on an extent edit their keys would name the wrong ground. */
	void OnTerrainCollisionRasterChanged(AArcTerrain& Terrain);

	/** Live lighting-knob change (WITH_EDITOR, from AArcTerrain::PostEditChangeProperty): radius,
	 *  tolerances and the skirt multiplier are folded into a built mesh and nothing dirties a cell when
	 *  they move, so the whole registry is dropped and the next Tick rebuilds it from the ring. */
	void OnTerrainLightingChanged(AArcTerrain& Terrain);

	void BakeLightingDerivedKnobs(AArcTerrain& Terrain);

	/** Live height-options change (WITH_EDITOR, from AArcTerrain::PostEditChangeProperty): BaseHeightCm
	 *  is the level the composite starts from, so every cell re-composites and every tile rail's
	 *  decisions are dropped so it re-keys. Only the bake recipe changed (NOT the spatial frame), so
	 *  collision, tiles and preview are left intact (the lighter counterpart to
	 *  OnTerrainConfigChanged's spatial teardown). */
	void OnTerrainHeightOptionsChanged(AArcTerrain& Terrain);

	void OnTerrainRenderChanged(AArcTerrain& Terrain);

	/** Live density-ladder change (WITH_EDITOR, from AArcTerrain::PostEditChangeProperty): S(T) moves under
	 *  every coarse tier at once, so every coarse sector registration, coarse tile key and coarse bake
	 *  decision names different ground. A ladder edit dirties no bounds, so without this the decision memo
	 *  holds those tiles current under a key it never recomputes. Tier 0 is untouched — S(0) is 0 whatever
	 *  the ladder says. */
	void OnTerrainHeightTiersChanged(AArcTerrain& Terrain);

	/** The weight-rail twin of OnTerrainHeightTiersChanged: same drop set, same call shape, but over the
	 *  weight ladder and weight groups. */
	void OnTerrainWeightTiersChanged(AArcTerrain& Terrain);

	void OnTerrainStreamLayoutChanged(AArcTerrain& Terrain);

	/** Stamp lifecycle. Config is always editable; a spatial-frame edit fully invalidates via
	 *  AArcTerrain::PostEditChangeProperty, so registering a stamp freezes nothing. */
	void RegisterStamp(UArcTerrainStampComponent& Stamp, AArcTerrain& Terrain);
	void UnregisterStamp(UArcTerrainStampComponent& Stamp, const FBox& LastBounds);

	/** SINGLE invalidation entry (routed from UArcTerrainStampComponent::NotifyStampChanged):
	 *  dirties every cell intersecting OldLocalBounds ∪ NewLocalBounds (terrain-local boxes). */
	void OnStampChanged(UArcTerrainStampComponent& Stamp, const FBox& OldLocalBounds, const FBox& NewLocalBounds);

	/** ArcTerrainSubsystemPrivate::ResolveWeightQuads for a tracked terrain — weight texels per CELL edge,
	 *  the runtime-resolved value the weight tile lattice is built on. 0 when the terrain is untracked. */
	int32 GetResolvedWeightQuads(const AArcTerrain& Terrain) const;

	/** The ladder the tick last published for this terrain; empty for an untracked one. GAME THREAD ONLY,
	 *  and BY VALUE: the state it comes from is copy-assigned every tick and its map rehashes on register. */
	FArcTerrainTierLadder GetResolvedTierLadder(const AArcTerrain& Terrain) const;

	/** The weight rail's ladder on the same terms as GetResolvedTierLadder. */
	FArcTerrainWeightLadder GetResolvedWeightTierLadder(const AArcTerrain& Terrain) const;

	bool BuildHeightAtlasSnapshot(const AArcTerrain& Terrain, FVector CentreWorldPos,
		FArcTerrainAtlasSnapshot& OutSnapshot) const;

	UArcTerrainAtlasTexture* GetHeightAtlasFacade(AArcTerrain& Terrain);

	/**
	 * One authored data mask read at a world position: the CPU sampler the whole data-map layer exists
	 * for. GAME THREAD ONLY; the intended consumers are editor bakers.
	 *
	 * Returns FALSE — never a silent 0 or 1 — when the data tile covering the position is not loaded,
	 * was never baked, or carries no plane for this mask. INSIDE a loaded plane every texel is a
	 * reading: the tile rail encodes saturated, so an area no source wrote reads a genuine 0.
	 *
	 * Never blocks: a tile the manifest knows but the stream has not brought in reads as absent.
	 */
	bool SampleDataMap(AArcTerrain& Terrain, EArcTerrainDataMap Map, const FVector& WorldPosition,
		float& OutValue) const;

	/**
	 * Bulk form of SampleDataMap over a region. GAME THREAD ONLY.
	 *
	 * THE GRID CONTRACT, so a caller never has to guess where a texel sits: OutValues is row-major
	 * OutDims.X * OutDims.Y and samples the terrain-local AABB of WorldBounds INCLUSIVE of both
	 * corners. Texel (0, 0) is at that AABB's (Min.X, Min.Y) and texel (OutDims.X-1, OutDims.Y-1) at
	 * (Max.X, Max.Y), so the spacing is Extent / (Dims - 1) on each axis and the caller re-derives the
	 * whole placement from its own WorldBounds plus OutDims. An axis of zero extent gets one texel.
	 *
	 * RESOLUTION is the data rail's own tile raster (DataTileExtent over TexelSize), so nothing is
	 * undersampled below what was baked. The grid is capped per axis and in total (a warning names the
	 * region when the cap coarsens it).
	 *
	 * Texels with no reading — a data tile that is not loaded, or one carrying no plane for this mask —
	 * are 0.0f in OutValues AND clear (false) at the matching index of OutAvailable, row-major
	 * DimsX * DimsY exactly like OutValues; a caller MUST consult OutAvailable, because the value alone
	 * is never distinguishable from a genuine 0 reading. Returns false (OutValues/OutAvailable emptied,
	 * OutDims zeroed) when the arguments are invalid, the region spans an unreasonable number of cells,
	 * or NO texel had a reading at all.
	 */
	bool SampleDataMapRegion(AArcTerrain& Terrain, EArcTerrainDataMap Map, const FBox& WorldBounds,
		TArray<float>& OutValues, TBitArray<>& OutAvailable, FIntPoint& OutDims) const;

	/**
	 * Composited terrain-local heights (cm) over a rectangle of the terrain's HEIGHT TEXEL lattice, from
	 * the CPU composite. GAME THREAD ONLY (the gather reads stamp descriptors and the CPU source cache).
	 *
	 * The rectangle is in texel INDICES, inclusive of both corners, rather than centimetres, so a consumer
	 * reads exactly the samples the height atlas holds and its surface agrees with the one the vertex
	 * factory draws.
	 *
	 * A non-square rectangle is padded to a square on the longer side; OutDims is that full square and
	 * OutHeights is row-major OutDims.X * OutDims.Y with sample (0, 0) at MinTexel and spacing
	 * FArcTerrainConfig::TexelSizeCm.
	 *
	 * bOutComplete is false when a covering stamp's height did not resolve on the CPU: the heights are a
	 * real evaluation missing that stamp's contribution, so a caller must not persist or cache them.
	 */
	bool SampleTerrainHeightTexels(const AArcTerrain& Terrain, FIntPoint MinTexel, FIntPoint MaxTexel,
		TArray<float>& OutHeights, FIntPoint& OutDims, bool& bOutComplete);
	bool SampleTerrainHeightTexelsBeforeStamp(const AArcTerrain& Terrain,
		const UArcTerrainStampComponent& BeforeStamp, FIntPoint MinTexel, FIntPoint MaxTexel,
		TArray<float>& OutHeights, FIntPoint& OutDims, bool& bOutComplete);

	/** Roots a pool-created render target against GC (see FArcTerrainRenderTargetPool). */
	void AddOwnedRenderTarget(UTextureRenderTarget2D& RenderTarget);

	/**
	 * arc.Terrain.DiagAt: everything this subsystem knows about ONE world position — the cell and its atlas
	 * publication tags, each rail's tile key with its manifest entry, file, header, lattice match and load
	 * state, and the collision patch. Answers "why is this piece of ground flat" without a second query.
	 *
	 * Reads the three tile files, so it is a console path and never a per-tick one. Independent of
	 * arc.Terrain.Diag: an explicit ask always answers.
	 */
	void LogPointDiagnostics(const FVector& WorldPosition);

#if WITH_EDITOR
	bool GetDataMapSourceRegion(const AArcTerrain& Terrain, EArcTerrainDataMap Map,
		FBox& OutTerrainLocalBounds, FArcTerrainTileGrid& OutDataGrid);
	bool SampleDataMapTexelsFromSources(const AArcTerrain& Terrain, EArcTerrainDataMap Map,
		FIntPoint MinTexel, FIntPoint MaxTexel, TArray<float>& OutValues, FIntPoint& OutDims,
		bool& bOutComplete);

	/** True when the live subsystem currently tracks this terrain/cell pair. Diagnostics only. */
	/**
	 * Bounding cell rectangle (max-EXCLUSIVE) of the UNION of the terrain's registered stamp footprints
	 * (UArcTerrainStampComponent::ComputeBounds over enabled, resolvable stamps), snapped to cell indices
	 * (floor of the union min / ceil of the union max over CellSize) and guaranteed at least one cell per
	 * axis. This is the "what to bake" default region now that the terrain is an infinite plane with no
	 * fixed CellBounds. Returns false (OutRegion left empty) when the
	 * terrain is untracked, its CellSize is invalid, or it has no enabled stamp with a valid footprint —
	 * an empty terrain has no finite region to bake.
	 */
	bool ComputeStampFootprintCellRegion(const AArcTerrain& Terrain, FIntRect& OutRegion) const;

	void RequestBakeWake();
	void EnqueueBakeCompletion(FArcTerrainBakeCompletionToken&& Token);
	void ArmBakeRetryTimer(double RetryAfter);
	void ClearBakeRetryTimer();
	void HandleBakeRetryTimer();

#endif // WITH_EDITOR

private:
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void TryClaimRendererWorld();
	void RetireRendererOwnership(bool bReleaseWorld, bool bWaitForRenderThread);
	void RefreshRendererRhiEpoch();

	void TickEditPins(double Now);
	void TickResidency(
		TConstArrayView<FVector> InvokerPositions,
		double Now,
		const UArcTerrainSettings& Settings,
		TArray<ArcTerrainSubsystemPrivate::FCompositeCandidate>& OutCandidates,
		int32& OutNumSectorCells,
		int32& OutNumGrantedSectors,
		int32& OutNumEvictCandidates,
		int32& OutNumEvicted,
		int32& OutNumHeldByPin,
		int32& OutNumHeldByQueue,
		int32& OutNumCompositeCandidates);

	void TickCollision(
		TConstArrayView<FVector> InvokerPositions,
		bool bInSyncWindow,
		const UArcTerrainSettings& Settings,
		int32& OutNumCollisionBuilds,
		int32& OutNumCollisionAttempts);
	void AttributeCollisionSaturation(FArcTerrainState& State);

	int32 ResolveStampMaxWeightSourceDim(const UArcTerrainStampComponent& Stamp) const;
	void AddWeightSourceDim(FArcTerrainState& State, int32 SourceDim);
	void RemoveWeightSourceDim(FArcTerrainState& State, int32 SourceDim);
	void RefreshStampWeightSourceDim(FArcTerrainState& State, FArcTerrainStampEntry& Entry);
	void RefreshResolvedWeightQuads(AArcTerrain& Terrain, FArcTerrainState& State);
	void FlushPendingDerivedGridRebuild(AArcTerrain& Terrain, FArcTerrainState& State);

	/**
	 * Tick phase 5: drains the cell edit queue — the only place the bQueued survival hold is released —
	 * and, in editor worlds, drives the three on-disk tile-bake rails over their pending-tile sets.
	 */
	void TickCellBakeLifecycle(UWorld* World, TConstArrayView<FVector> InvokerPositions);

	/**
	 * Tick phase 2b: sector admission into the tile atlas. Evicts sectors that left residency, admits
	 * the resident ones nearest-first, and emits ONE fill job per terrain.
	 *
	 * ADMISSION IS AN EXPLICIT PASS, not a piggyback on the composite: the composite loop is budgeted
	 * (MaxCompositesPerFrame + deadline), so a sector admitted from cells that composited a minute ago
	 * would draw nothing but the slice clear. The piggyback survives as the single-cell update path
	 * (FArcTerrainCellState::bAtlasRefillPending).
	 */
	void TickTileAtlases();

	/**
	 * Tick phase 1b: the disk stream, STATELESS with respect to cells. Per terrain and per rail, the key
	 * window on that rail's own lattice around each invoker at StreamRadius * StreamHysteresis, tested
	 * against the manifest — the window is walked, never the manifest, which holds tens of thousands of
	 * entries over a 10 km terrain, and its span follows the radius on that lattice. A key inside
	 * StreamRadius that is not loaded is requested; a loaded key beyond the outer radius has its CPU
	 * PAYLOAD released unless a consumer outside the cache still holds it.
	 *
	 * NO STREAMING CEILING: the only bound is a fixed cap on the WALK (GMaxStreamWindowSpan keys either
	 * side of an invoker), which a degenerate terrain scale reaches and an authored radius does not.
	 *
	 * ATLAS SLICES ARE NOT ITS BUSINESS — PreviewRadius owns those. The coupling runs the other way: an
	 * unstamped cell whose height tile the manifest names is publishable only once the stream has brought
	 * that tile in, so the effective stream radius is floored at PreviewRadius * PreviewRadiusHysteresis.
	 * Below that floor the sectors between the two radii would never satisfy admission and never draw.
	 */
	void TickTileStreaming(TConstArrayView<FVector> InvokerPositions);

	void InvalidateStreamWindows(FArcTerrainState& State);
	void ArmStreamReleaseRetry(double RetryAt);
	void HandleStreamReleaseRetry();
	void ClearStreamReleaseTimer();

	/** THE 1 -> 2 TRANSITION, and it happens here rather than at the request: the bytes are in memory, so
	 *  the cells the tile covers are queued for the atlas fill, which reads the payload itself. */
	void OnStreamedTileArrived(AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey);

	/** Hand the base mesh — face topology plus the per-tier residency rings the predicate samples
	 *  through — to the bisector pool. */
	void PublishCBTBaseMesh(const AArcTerrain& Terrain, FArcTerrainState& State,
		const FArcTerrainTileAtlasLayout& Layout, const FArcTerrainFaceGrid& Grid,
		const FArcTerrainResidencyRings& Rings);

	/** Tell the bisector pool this terrain no longer has a base mesh, releasing the pool and the atlas
	 *  resources its published mesh keeps alive. */
	void InvalidateCBTBaseMesh(FArcTerrainState& State);

	/**
	 * THE READINESS PREDICATE gating sector admission: may this cell be published into a slice?
	 *
	 * A cell with NO stamp over it is FLAT BY DEFINITION, not missing — its slice region keeps the
	 * clear (which holds the base level) and it never blocks its sector. Holding "all cells complete"
	 * literally would permanently disable a 1024 m band along the authored terrain's edge. Only a height
	 * tile the manifest knows but the stream has not brought in holds admission.
	 * Composite state is deliberately NOT a term: a cell composites into the slice this predicate hands
	 * out, so consulting it would gate the slice on itself.
	 *
	 * The lattice is the caller's, resolved once per terrain; an invalid one means the rail has no
	 * tiles at all and cannot hold the cell out.
	 */
	bool IsCellPublishable(AArcTerrain& Terrain, FArcTerrainCellKey Key, const FArcTerrainCellState& Cell,
		const FArcTerrainTileGrid& HeightGrid) const;

	/** Resolve one cell's HEIGHT source + destination sub-rect for a fill job, against the height slice of
	 *  the sector holding the cell. False when the cell has no height source at all (it keeps the slice
	 *  clear = flat at the base level), including when that sector is not admitted. The payload is the one
	 *  the caller already resolved for the cell, so the fill and the serial it stamps read one lookup.
	 *
	 *  Cell may be NULL, and that is the drawn-from-bytes state, not a hole: ground with a loaded tile and
	 *  no modifier over it materialises no cell, and its region is filled from the payload alone. */
	bool BuildCellHeightFill(FArcTerrainCellKey Key, const FArcTerrainCellState* Cell,
		const FArcTerrainTileGrid& HeightGrid, const FArcTerrainTilePayloadPtr& HeightTile,
		const FArcTerrainTileAtlasLayout& Layout, const FArcTerrainTileAtlas& Atlas,
		FArcTerrainAtlasCellFill& OutFill) const;

	/** Resolve one cell's WEIGHT sources + destination sub-rect for a fill job, against the weight slice of
	 *  the group holding the cell. False when that group is not admitted, or when no live tile and no baked
	 *  plane could be attached. Independent of the height rail in both directions: the cell's sector need
	 *  not be admitted for this to fill.
	 *
	 *  Cell may be NULL — see BuildCellHeightFill.
	 *
	 *  ChannelSet resolves the baked planes' channel-name hashes to the global indices the atlas addresses
	 *  by; without one no baked weight plane can be placed.
	 *
	 *  bOutWeightUnresolvable separates NOT ADDRESSABLE from NOT READY: true when the cell's weight slice
	 *  can never be filled from the payload it was handed (no channel set, or no plane id naming a channel
	 *  of it), so the caller drops the refill mark instead of re-walking the cell every tick. */
	bool BuildCellWeightFill(FArcTerrainCellKey Key, const FArcTerrainCellState* Cell,
		const UArcTerrainChannelSet* ChannelSet,
		const FArcTerrainTileGrid& WeightGrid, const FArcTerrainTilePayloadPtr& WeightTile,
		const FArcTerrainTileAtlasLayout& Layout, const FArcTerrainTileAtlas& Atlas,
		FArcTerrainAtlasCellFill& OutFill, bool& bOutWeightUnresolvable) const;

	/** THE SINGLE DIRTYING AUTHORITY: cells and the collision patches an edit's
	 *  terrain-local bounds cut. It takes the ACTOR because the collision lattice is authored there, not
	 *  in Config. */
	void MarkBoundsDirty(FArcTerrainState& State, const AArcTerrain& Terrain, const FBox& LocalBounds);

	/** Edit-pin every cell intersecting LocalBounds: PinnedUntil = max(existing, InPinnedUntil).
	 *  NOTE: a stamp footprint bigger than the tile-pool VRAM budget pins that many cells (the pin
	 *  set is exempt from MaxResidentSectors) — Tick logs a one-shot informational warning when the
	 *  pinned sector count exceeds the cap. */
	void PinCellsInBounds(FArcTerrainState& State, const FArcTerrainConfig& Config, const FBox& LocalBounds,
		double InPinnedUntil);

	/**
	 * Gather + sort intersecting stamps, resolve GPU sources, enqueue the RDG composite. The composited
	 * height goes STRAIGHT into the cell's sub-rect of its sector's atlas slice — the single height
	 * source — and Cell.AtlasHeightVersion records that publication.
	 *
	 * OutRetainedHeightTile, when non-null, additionally receives a pooled R32f staging tile carrying the
	 * same heights, NOT released here: the modal synchronous paths need a readable copy, and it is also
	 * the only destination a composite has before the atlas exists at all (PIE-entry bootstrap). The
	 * CALLER releases it back to RenderTargetPool.
	 *
	 * Returns false when nothing was enqueued — invalid config, tile allocation failure, or no
	 * destination at all (the cell's sector holds no atlas slice and no staging tile was asked for) — so
	 * the caller keeps the cell dirty and retries.
	 */
	bool CompositeCell(AArcTerrain& Terrain, FArcTerrainState& State, FArcTerrainCellKey Key,
		FArcTerrainCellState& Cell, UTextureRenderTarget2D** OutRetainedHeightTile = nullptr);

#if WITH_EDITOR
	/**
	 * Drive the three on-disk tile rails over their pending-tile sets plus whatever they already have in
	 * flight. One shared deadline permits one lane scan and the first candidate-entry examination for the
	 * tick, while NumHeavySteps continues to count DDC probes, evaluation launches, and completed writes.
	 *
	 * SettleTicks is arc.Terrain.SettleTicks: a tile is only taken once its DirtySerial has stood still
	 * for that many passes, which is what keeps a stamp drag from writing a whole tile per frame. A tile
	 * already in flight is exempt — its payload has to be consumed or it is evaluated for ever.
	 *
	 * TileDir is MakeTileDirForWrite's answer and is never empty here; the caller resolves it once.
	 */
	void TickTerrainTileBakes(AArcTerrain& Terrain, FArcTerrainState& State, const FString& TileDir,
		int32 SettleTicks,
		int32& NumHeavySteps, int32& NumBakeCompletions, int32& NumBakeLaneScans, int32& NumBakeCandidateExaminations,
		double HeavyDeadline, bool& bStoppedForBudget);

	/**
	 * One step of one tile's height payload. The evaluation runs off the game thread; the DDC probe, the
	 * write and the manifest entry are game-thread work.
	 *
	 * An incomplete gather (a covering stamp's source did not resolve) writes nothing and records no key,
	 * so the tile is re-attempted — the whole payload would otherwise be missing that stamp under a key
	 * that still matched.
	 */
	void ProbeAndRebuildHeightTile(AArcTerrain& Terrain, FArcTerrainState& State, const FString& TileDir,
		int32 Tier, FIntPoint TileKey, int32& NumHeightBakes, const UArcTerrainSettings& Settings);

	void ProbeAndRebuildWeightTile(AArcTerrain& Terrain, FArcTerrainState& State, const FString& TileDir,
		int32 Tier, FIntPoint TileKey, int32& NumWeightRebuilds, const UArcTerrainSettings& Settings);

	void WriteWeightTilePlanes(AArcTerrain& Terrain, const FString& TileDir, int32 Tier, FIntPoint TileKey,
		const FArcTerrainTileGrid& Grid,
		TConstArrayView<ArcTerrainWeights::FArcTerrainWeightPlane> Planes, const FIoHash& CacheKey,
		uint32 DecidedSerial, bool bFromCache, FArcTerrainState& State, FArcTerrainTileRailState& RailState,
		const UArcTerrainSettings& Settings,
		TSet<FIntPoint>& PendingKeys);

	/** The game-thread completion one tile's off-thread file write is launched with. Weak on both the
	 *  subsystem and the terrain: the write outlives the tick it started in. */
	TFunction<void(bool)> MakeTileWriteCompletion(AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, const FIoHash& RecipeKey, uint64 ReservationId, uint64 WriteGeneration);

	/** The game thread's half of a tile write: it is where the manifest entry is published, and the only
	 *  place a key leaves its rail's work list. */
	void FinishTileWrite(TWeakObjectPtr<AArcTerrain> WeakTerrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, const FIoHash& RecipeKey, uint64 ReservationId, uint64 WriteGeneration,
		bool bWritten);

	void ProbeAndRebuildDataTile(AArcTerrain& Terrain, FArcTerrainState& State, const FString& TileDir,
		FIntPoint TileKey, FArcTerrainDataMapBakeScope& BakeScope, int32& NumDataBakes,
		const UArcTerrainSettings& Settings);
#endif // WITH_EDITOR

	bool BuildAndApplyPatchCollision(AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainCollisionKey Key, FArcTerrainCollisionPatch& Patch, bool bAllowBlockingProbe);

	/** Drive one patch's collision for this tick: ring-exit teardown and mesh-modifier bodies, both
	 *  unbudgeted. Returns true when the patch WANTS a heightfield build — the caller ranks and budgets
	 *  those, so this never builds one itself. Entry gate is CollisionRadius, sustain gate
	 *  CollisionUnloadRadius (a patch that already holds a component). */
	bool UpdateCollisionPatch(AArcTerrain& Terrain, FArcTerrainState& State, FArcTerrainCollisionKey Key,
		FArcTerrainCollisionPatch& Patch, double InvokerDistSq, double CollisionRadius,
		double CollisionUnloadRadius, bool bInSyncWindow);

	/** Ensure the patch's transient collision component exists and swap the heightfield in
	 *  (RecreatePhysicsState via UArcTerrainCollisionComponent::SetHeightfield). */
	void ApplyPatchCollision(AArcTerrain& Terrain, FArcTerrainCollisionKey Key,
		FArcTerrainCollisionPatch& Patch, const TRefCountPtr<FArcTerrainHeightfieldRef>& Heightfield);

	/** Destroy the patch's collision component + mesh-modifier bodies (ring exit / teardown). */
	void DestroyPatchCollision(FArcTerrainCollisionPatch& Patch);

	/**
	 * Tick phase 1c: the lighting ring. Per terrain, on the CELL lattice and needing no cell: ring-exit
	 * teardown, tolerance banding, the up-to-date guard, and the budgeted dispatch of the mesh + material
	 * raster builds that answer it. The two tile lattices are resolved ONCE here and handed down — the
	 * weight rail's resolve scans every stamp on the terrain.
	 */
	void TickLightingPatches(TConstArrayView<FVector> InvokerPositions, double& OutLightingSeconds,
		double& OutFarBlockSeconds);

	void TickTerrainLightingPatches(AArcTerrain& Terrain, FArcTerrainState& State,
		TConstArrayView<FVector> InvokerPositions);

	/** Create/refresh the cell's component, textures and MID from a finished build, and register the
	 *  component when it is new (textures exist BEFORE registration — the distance-field path asks the
	 *  proxy for them the moment it appears). */
	void ApplyLightingBuild(AArcTerrain& Terrain, FArcTerrainCellKey Key, FArcTerrainLightingPatch& Patch,
		FArcTerrainLightingBuild& Build);

	/** Destroy the cell's lighting component and ABANDON any in-flight build — never wait on one. */
	void DestroyLightingPatch(FArcTerrainLightingPatch& Patch);

	/**
	 * Tick sub-phase 1d: the far occlusion ring, on the BLOCK lattice. Runs only under hardware ray traced
	 * Lumen — a BLAS nothing traces is pure cost. The height lattice is resolved ONCE per terrain here.
	 */
	void TickTerrainFarBlockPatches(AArcTerrain& Terrain, FArcTerrainState& State,
		TConstArrayView<FVector> InvokerPositions);

	/** Create/refresh the block's occlusion component from a finished build. */
	void ApplyFarBlockBuild(AArcTerrain& Terrain, FArcTerrainFarBlockPatch& Patch,
		FArcTerrainFarBlockBuild& Build);

	/** Destroy the block's occlusion component and ABANDON any in-flight build — never wait on one. */
	void DestroyFarBlockPatch(FArcTerrainFarBlockPatch& Patch);

	/** Make Patch.MeshModifierCollision match the patch's GatherSortedMeshModifiers set — the SAME
	 *  gather authority the DDC key and the geometry capture use, never a second walk. A pose-only delta
	 *  moves the existing bodies (SetWorldTransform); only a SET change — count, a collected body, mesh
	 *  or material override — destroys and recreates. GAME THREAD ONLY: the gather reads
	 *  UStaticMesh::GetBoundingBox, which waits on static-mesh compilation. */
	void SyncPatchMeshModifierCollision(AArcTerrain& Terrain, const FArcTerrainState& State,
		FArcTerrainCollisionKey Key, FArcTerrainCollisionPatch& Patch);

	/** Destroy every mesh-modifier body of the patch and empty the array (null weak entries tolerated). */
	void DestroyPatchMeshModifierCollision(FArcTerrainCollisionPatch& Patch);

	void SyncBuildPatchCollision(AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainCollisionKey Key, FArcTerrainCollisionPatch& Patch);

	/** Sync-build every collision patch within Radius (2D) of a world-space anchor. */
	void SyncBuildCollisionAroundAnchor(AArcTerrain& Terrain, FArcTerrainState& State,
		const FVector& WorldAnchor, double Radius);

	/** GC root for every render target the pool ever created (tiles + scratch). Un-rooted only in
	 *  Deinitialize, after FlushRenderingCommands. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> OwnedRenderTargets;

	TMap<TWeakObjectPtr<AArcTerrain>, FArcTerrainState> Terrains;

	TSet<TWeakObjectPtr<AArcTerrain>> TerrainsWithDirtyCells;
	void DrainCellDirtyQueues();

	FArcTerrainRenderTargetPool RenderTargetPool;
	FArcTerrainSourceCache SourceCache;
	FArcTerrainPreviewManager PreviewManager;
	FDelegateHandle WorldCleanupHandle;
	uint64 WorldInstanceToken = 0;
	FSceneInterface* ClaimedRendererSceneKey = nullptr;
	bool bRendererOwnershipReleased = false;

#if WITH_EDITOR
	struct FTileBakeLedgerEntry
	{
		EArcTerrainBakeReservationKind Kind = EArcTerrainBakeReservationKind::Probe;
		int64 ReservedBytes = 0;
		bool bAbandoned = false;
		bool bData = false;
		TFunction<bool()> IsComplete;
		FString WriteFence;
	};

	bool CanReserveTileBakeWork(EArcTerrainBakeReservationKind Kind, int64 ReservedBytes,
		const UArcTerrainSettings& Settings) const;
	bool CanReserveTileBakeWriteAfterEvaluation(uint64 EvaluationReservationId, int64 WriteReservedBytes,
		const UArcTerrainSettings& Settings) const;
	bool CanReserveTileBakeWriteAfterReservation(uint64 ReservationId, int64 WriteReservedBytes,
		const UArcTerrainSettings& Settings) const;
	bool IsTileWriteFenced(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey) const;
	uint64 AcquireTileBakeWriteReservation(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, int64 ReservedBytes);
	uint64 AcquireTileBakeReservation(EArcTerrainBakeReservationKind Kind, int64 ReservedBytes,
		TFunction<bool()>&& IsComplete = TFunction<bool()>(), bool bData = false);
	void ReleaseTileBakeReservation(uint64 ReservationId);
	void AbandonTileBakeReservation(uint64 ReservationId);
	void AbandonPendingTileBake(FArcTerrainTileRailState& RailState, FIntPoint TileKey);
	void AbandonRailPendingTileBakes(FArcTerrainTileRailState& RailState);
	void DrainAbandonedTileBakeReservations();
	void InvalidateRailWrites(FArcTerrainTileRailState& RailState);

	TMap<uint64, FTileBakeLedgerEntry> TileBakeLedger;
	uint64 NextTileBakeReservationId = 1;
	int32 TileBakeProbesInFlight = 0;
	int32 TileBakeEvaluationsInFlight = 0;
	int32 TileBakeWritesInFlight = 0;
	int64 TileBakeReservedBytes = 0;
	TMap<FString, uint64> ActiveTileWriteFences;

	/** UArcTerrainSettings::OnSettingChanged handler (editor only): forces an immediate source-cache
	 *  re-trim when SourceCacheMB is lowered (the cache otherwise only trims on insert). All other
	 *  scheduler knobs are read fresh per-tick, so they need no delegate. */
	void HandleTerrainSettingsChanged(UObject* Object, struct FPropertyChangedEvent& PropertyChangedEvent);
	FDelegateHandle SettingsChangedHandle;

	void HandleAssetReimport(UObject* ReimportedAsset);
	FDelegateHandle AssetReimportHandle;

	TWeakObjectPtr<AArcTerrain> NextBakeTerrain;
	bool bDataBakePassActive = false;
	int32 NumDataPendingTiles = 0;
	int32 NumDataInFlight = 0;
	int32 NumAbandonedDataReservations = 0;

	TMpscQueue<FArcTerrainBakeCompletionToken> BakeCompletionQueue;
	std::atomic<bool> bBakeWakeRequested { false };
	std::atomic<bool> bBakeShuttingDown { false };
	TSharedPtr<ArcTerrainCollision::FArcTerrainCollisionCompletionMailbox, ESPMode::ThreadSafe> CollisionMailbox;
	bool bBakeStoppedForBudget = false;
	FTimerHandle BakeRetryTimerHandle;
	double ArmedBakeRetryAfter = 0.0;

	struct FBakeProgressSnapshot
	{
		int32 Queue[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 Probes[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 Evaluations[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 Writes[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 Deferred[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
	};
	FBakeProgressSnapshot BakeProgressSnapshot;
	bool bBakeProgressSnapshotValid = false;
	bool bBakeProgressSessionActive = false;

	void DrainBakeCompletions();
	void RefreshBakeRetryTimer();
	void RefreshBakeProgressSnapshot();
	bool HasImmediateBakeWork() const;
	bool CanRunTileBake(const UWorld* World) const;
	bool HasPendingDataTileWork() const;

	void EndDataBakePass();
#endif

	FArcTerrainRuntimeOwnerId GetRuntimeOwnerId() const;
	void PublishRuntimeLaneLimits();
	void DrainRuntimeCompletions();
	void ApplyAtlasRuntimeCompletion(FArcTerrainRuntimeCompletion&& Completion);
	void ApplyLightingRuntimeCompletion(FArcTerrainRuntimeCompletion&& Completion);
	void ApplyFarRuntimeCompletion(FArcTerrainRuntimeCompletion&& Completion);
	bool SubmitAtlasPrepare(FArcTerrainState& State, FArcTerrainAtlasPrepareInput&& Input);
	bool SubmitCbtTopology(FArcTerrainState& State, const FArcTerrainCBTTopologyInput& Input);
	bool FlushPendingAtlasFills(AArcTerrain& Terrain, FArcTerrainState& State);
	void ReleasePendingAtlasFills(uint64 TerrainId);

	struct FPendingAtlasFill
	{
		uint64 TerrainId = 0;
		TSharedPtr<FArcTerrainAtlasPrepareResult, ESPMode::ThreadSafe> Prepared;
		FArcTerrainRuntimeReservationToken Token;
		EArcTerrainRuntimeWorkStatus Status = EArcTerrainRuntimeWorkStatus::Succeeded;
	};
	TArray<FPendingAtlasFill> PendingAtlasFills;

	/** End of the PIE/game-entry SYNC window (world seconds). During the window Tick sync-builds
	 *  collision under each player pawn (late-possession anchor — no GameMode delegate assumptions)
	 *  and skips ring-exit collision teardown so PlayerStart-anchored cells survive the first
	 *  invoker-less frames. */
	double SyncCollisionWindowEndTime = 0.0;
	uint32 NextCollisionTerrainId = 1;
	uint64 CollisionCountSaturationSerial = 0;
	uint64 CollisionDeadlineSaturationSerial = 0;

	/** One-shot: pinned-sector count of some terrain exceeded MaxResidentSectors (informational — pins
	 *  deliberately override the cap; a huge stamp just costs proportionally more tile VRAM). */
	bool bWarnedPinOverBudget = false;

	/** One-shot: pinned WEIGHT groups of some terrain exceeded MaxResidentWeightCells. */
	bool bWarnedWeightPinOverBudget = false;

	FTimerHandle StreamReleaseTimerHandle;
	double ArmedStreamReleaseRetryAt = 0.0;

	FArcTerrainInvokerCache InvokerCache;
};
