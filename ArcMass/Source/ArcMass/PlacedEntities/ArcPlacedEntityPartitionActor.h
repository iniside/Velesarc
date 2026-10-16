// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "ActorPartition/PartitionActor.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityManager.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityOverrideHost.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/ArchetypeTypes.h"
#include "ArcPlacedEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Mesh/ArcISMHolderRegistry.h"
#include "ArcMass/SkinnedMeshVisualization/ArcSkinnedMeshHolderRegistry.h"
#include "ArcPlacedEntityPartitionActor.generated.h"

class UMassEntityConfigAsset;
class UInstancedStaticMeshComponent;
class UNiagaraComponent;
class UMaterialInterface;
struct FMassEntityManager;
class IArcPersistenceBackend;
class UArcPersistenceSubsystem;
class AArcPlacedEntityPartitionActor;
struct FStreamableHandle;

#if WITH_EDITOR
enum class EArcPlacedEntityOutlinerEvent : uint8 { Rebuilt, TornDown, Destroyed };
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcPlacedEntityOutlinerDelegate,
	AArcPlacedEntityPartitionActor* /*Actor*/, EArcPlacedEntityOutlinerEvent);
#endif // WITH_EDITOR

/**
 * Partition actor that stores Mass entity config instances placed in the world.
 * In-editor, creates ISM components for visual preview and per-instance selection/manipulation.
 * Static-mesh entries use the componentless MassEntity element path; skinned/Niagara/composite
 * entries keep the existing ISMC-based preview path.
 */
UCLASS()
class ARCMASS_API AArcPlacedEntityPartitionActor : public APartitionActor, public IArcMassEntityManager, public IArcMassEntityManagerProvider, public IArcPlacedEntityOverrideHost
{
	GENERATED_BODY()

public:
	AArcPlacedEntityPartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, Category = "PlacedEntities")
	TArray<FArcPlacedEntityEntry> Entries;

#if WITH_EDITOR
	/** Fired when the componentless editor render state transitions (Rebuilt / TornDown / Destroyed).
	 *  Subscribers (e.g. UArcMassOutlinerRowManager) refresh their cached instance lists without polling. */
	FArcPlacedEntityOutlinerDelegate OnEditorStateChanged;
#endif

#if WITH_EDITORONLY_DATA
	/** Cosmetic Outliner nesting under whoever generated this host. Soft: a hard reference is a World Partition ActorDesc reference. */
	UPROPERTY()
	TSoftObjectPtr<AActor> OutlinerParent;
#endif

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	//~ Begin APartitionActor Interface
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override;
	virtual FGuid GetGridGuid() const override { return GridGuid; }
	void SetGridGuid(const FGuid& InGuid) { GridGuid = InGuid; }
	void SetGridName(FName InGridName) { GridName = InGridName; }
	//~ End APartitionActor Interface

	//~ Begin AActor Interface
	virtual void PostRegisterAllComponents() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostLoad() override;
	virtual AActor* GetSceneOutlinerParent() const override;
	//~ End AActor Interface

	/**
	 * Add a new instance of the given config at the given transform. Creates a new entry if needed.
	 * @param OutEntryIndex   Index into Entries[] for the matched/created entry, or INDEX_NONE on failure.
	 * @param OutInstanceIndex Index of the new transform within that entry's InstanceTransforms, or INDEX_NONE on failure.
	 */
	void AddInstance(UMassEntityConfigAsset* ConfigAsset, const FTransform& Transform, int32& OutEntryIndex, int32& OutInstanceIndex);

	/**
	 * Batched counterpart of AddInstance for generated content: appends every transform to the entry
	 * matching (ConfigAsset, OwnerGuid) and rebuilds the editor state once, where AddInstance rebuilds
	 * per transform. Matching on the owner too keeps a generator out of hand-placed and foreign-owner
	 * entries, which RemoveEntriesOwnedBy would then destroy wholesale.
	 * @return Index into Entries[] for the matched/created entry, or INDEX_NONE when ConfigAsset is null
	 *         or Transforms is empty.
	 */
	int32 AddInstances(UMassEntityConfigAsset* ConfigAsset, const FGuid& OwnerGuid, TConstArrayView<FTransform> Transforms);

	/** Rebuild all editor preview ISM components from Entries data. */
	void RebuildEditorPreviewISMCs();

	/**
	 * Rebuild both editor paths atomically.
	 * Order: TearDownEditorRenderState -> DestroyEditorPreviewISMCs -> RebuildEditorRenderState -> RebuildEditorPreviewISMCs.
	 * For delete/duplicate, the caller mutates Entries between teardown and this call.
	 */
	void RebuildAllEditorState();

	/** Mint a per-instance LinkGuid override if the entry's config has UArcLinkableGuidTrait and no guid override exists yet. Idempotent. */
	void AutoAssignLinkGuidIfNeeded(FArcPlacedEntityEntry& Entry, int32 InstanceIndex);

	/** Rebuild GuidToInstanceMap from all entries' PerInstanceOverrides. */
	void RebuildGuidIndex();

	/** Destroy all editor preview ISM components. */
	void DestroyEditorPreviewISMCs();

	/** Look up which entry/transform owns a given LinkGuid. Returns false if not found. */
	bool FindInstanceByGuid(const FGuid& Guid, int32& OutEntryIndex, int32& OutTransformIndex) const;

	// Removes entries whose FArcPlacedEntityEntry::OwnerGuid matches. Editor-only.
	// Returns true iff Entries is empty after the call.
	bool RemoveEntriesOwnedBy(const FGuid& OwnerGuid);

	// True iff any entry has !OwnerGuid.IsValid() — hand-placed content present.
	bool HasHandPlacedEntries() const;

	/** Returns the mesh-accurate world-space bounds for a MassEntity element instance. Used by GetBounds in the world interface. */
	bool GetMassEntityInstanceMeshBounds(const FArcMassEntityElementId& InId, FBoxSphereBounds& OutBounds) const;

	FTransform GetEntryComponentTransform(const FArcPlacedEntityEntry& Entry) const;
#endif

	FName GetGridName() const { return GridName; }

	const TArray<FArcPlacedEntityEntry>& GetEntries() const { return Entries; }

	/**
	 * Mark a placed entity instance as removed. Adds the instance to the removal overlay (ClassRemovals),
	 * queues a pending respawn if the entry has RespawnTimeSeconds > 0, and deferred-destroys the live entity
	 * if one exists.
	 * @param EntryIndex     Index into Entries[].
	 * @param TransformIndex Index into Entries[EntryIndex].InstanceTransforms[].
	 */
	void MarkInstanceRemoved(int32 EntryIndex, int32 TransformIndex);

	/**
	 * Mark a placed entity instance as removed by its Mass entity handle.
	 * Looks up (EntryIndex, TransformIndex) via the internal EntityToLocation map and delegates to the
	 * index-based overload. Logs a warning and returns early if the handle is not tracked.
	 */
	void MarkInstanceRemoved(FMassEntityHandle Entity);

	/**
	 * Records a slot as removed and schedules respawn — WITHOUT issuing the entity destroy.
	 * Called by UArcPlacedEntityOwnerObserver when a placed entity is destroyed by any path
	 * (pickup, gameplay kill, deferred destroy). Distinct from MarkInstanceRemoved which also
	 * issues the Mass deferred destroy itself.
	 *
	 * No-op when bIsDespawningOnEndPlay is true (cell unload, not a pickup-style removal).
	 */
	virtual void RecordRemovalForRespawn(int32 EntryIndex, int32 TransformIndex);

	/**
	 * Called ~1Hz by UArcPlacedEntityRespawnProcessor via the cell-tracker entity. Collects all entries
	 * from PendingRespawns whose RespawnAtUtc <= NowUtc, removes them from ClassRemovals, and re-spawns
	 * each via RestoreInstance.
	 * @param NowUtc  Current time as Unix seconds (FDateTime::UtcNow().ToUnixTimestamp()).
	 */
	void RestoreExpiredRemovals(int64 NowUtc);

	/**
	 * Called once at BeginPlay before SpawnEntities. Purges entries from PendingRespawns and ClassRemovals
	 * whose RespawnAtUtc already elapsed while the game was off. The subsequent SpawnEntities call will then
	 * naturally spawn those instances since they are no longer in the removal set.
	 * @param NowUtc  Current time as Unix seconds (FDateTime::UtcNow().ToUnixTimestamp()).
	 */
	void PurgeExpiredRemovals(int64 NowUtc);

	virtual void BeginDestroy() override;

private:
	//~ IArcMassEntityManager interface
	virtual bool CanEditMassEntityInstance(const FArcMassEntityElementId& InId) const override;
	virtual bool CanMoveMassEntityInstance(const FArcMassEntityElementId& InId, ETypedElementWorldType WorldType) const override;
	virtual bool GetMassEntityInstanceTransform(const FArcMassEntityElementId& InId, FTransform& OutTransform, bool bWorldSpace = false) const override;
	virtual bool SetMassEntityInstanceTransform(const FArcMassEntityElementId& InId, const FTransform& InTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false) override;
	virtual void NotifyMassEntityInstanceMovementStarted(const FArcMassEntityElementId& InId) override;
	virtual void NotifyMassEntityInstanceMovementOngoing(const FArcMassEntityElementId& InId) override;
	virtual void NotifyMassEntityInstanceMovementEnded(const FArcMassEntityElementId& InId) override;
	virtual void NotifyMassEntityInstanceSelectionChanged(const FArcMassEntityElementId& InId, bool bIsSelected) override;
	virtual bool DeleteMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds) override;
	virtual bool DuplicateMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds, TArray<FArcMassEntityElementId>& OutNewIds) override;

	//~ IArcMassEntityManagerProvider interface
	virtual IArcMassEntityManager* GetMassEntityManager(const FArcMassEntityElementId& InId) override;

	//~ IArcPlacedEntityOverrideHost interface
	virtual TArray<FArcPlacedEntityEntry>& GetPlacedEntityEntries() override { return Entries; }

#if WITH_EDITOR
	void OnPreBeginPIE(const bool bIsSimulating);
	void OnEndPIE(const bool bIsSimulating);

	/** True iff the entry should use the componentless MassEntity editor path (static mesh only, no composite/skinned/Niagara). */
	bool IsComponentlessEntry(const FArcPlacedEntityEntry& Entry) const;

	/** True iff the entry should use the skinned componentless MassEntity editor path (skinned mesh only, no composite/Niagara, not already a static componentless entry). */
	bool IsSkinnedComponentlessEntry(const FArcPlacedEntityEntry& Entry) const;

	/** True iff the entry should use the composite componentless MassEntity editor path (valid composite trait, not already a static/skinned componentless entry). */
	bool IsCompositeComponentlessEntry(const FArcPlacedEntityEntry& Entry) const;

	/** Get the editor world's Mass entity manager (editor context only). */
	FMassEntityManager* GetEditorEntityManager() const;

	/** Rebuild the componentless render state for all static-mesh entries. */
	void RebuildEditorRenderState();

	/** Tear down the componentless render state (deselects + destroys all MassEntity elements and holder entities). */
	void TearDownEditorRenderState();

	/** Incrementally update one instance's render transform without a full rebuild. */
	bool UpdateInstanceRender(int32 EntryIndex, int32 TransformIndex, const FTransform& WorldTransform);

	/** Resolve a FArcMassEntityElementId to (entry, transform) indices. Returns false if not found or out of range. */
	bool ResolveElementId(const FArcMassEntityElementId& InId, int32& OutEntryIndex, int32& OutTransformIndex) const;

	/** Override for undo/redo — rebuilds both preview paths. */
	virtual void PostEditUndo() override;

	/** Arm a 0.25s periodic ticker that re-runs RebuildEditorRenderState once any deferred mesh is ready. No-op if already armed or bNeeded==false. */
	void EnsureRenderReadinessPoll(bool bNeeded);

	/** Returns true iff at least one componentless entry has a mesh that is now render-ready but has no live instance yet. */
	bool AnyDeferredComponentlessEntryNowReady() const;

	/** Called when a material finishes compiling; schedules a debounced RebuildEditorRenderState. */
	void OnMaterialCompilationFinished(UMaterialInterface* Material);

	/** Kick an async load of all mesh/material soft paths for componentless entries that are not yet resident. */
	void RequestEditorMeshLoad();
#endif

	void SpawnEntities();
	void DespawnEntities();

	/** Extract the tracker-entity build from BeginPlay so it can be called from both the immediate and deferred paths. */
	void SetupTrackerEntity();

	/** Persist the current removal overlay and pending respawn queue to the backend. */
	void SaveRemovals();

	/** Load the removal overlay and pending respawn queue from the backend. */
	void LoadRemovals();

	/** Resolve the persistence backend via UArcPersistenceSubsystem. Returns nullptr if unavailable. */
	IArcPersistenceBackend* GetPersistenceBackend() const;

	/**
	 * If Entity has both FArcLinkableGuidFragment and FArcMassPersistenceFragment, copies LinkGuid into PersistenceGuid.
	 * Called after each spawner-created entity to unify placed-entity GUIDs.
	 */
	void ApplyLinkGuidFixup(FMassEntityHandle Entity, FMassEntityManager& EntityManager);

	/**
	 * Full-scan (not break-on-first-non-expired) two-pass collection of all expired PendingRespawns entries.
	 * Pass 1: scan entire PendingRespawns array, collect indices and copies of entries with RespawnAtUtc <= NowUtc.
	 * Pass 2: walk collected indices in reverse, bulk-remove from PendingRespawns to keep earlier indices valid.
	 * Then remove the corresponding (ClassIndex, TransformIndex) from ClassRemovals.
	 * Returns copies of the expired entries so callers can re-spawn them.
	 */
	TArray<FArcPlacedEntityPendingRespawn> CollectAndClearExpiredRespawns(int64 NowUtc);

	/**
	 * Re-spawns a single placed instance at (EntryIndex, TransformIndex).
	 * Allocates one entity via BatchCreateEntities, calls SpawnSingleInstance to write fragments,
	 * writes the handle back into SpawnedEntities at the computed flat index, and updates EntityToLocation.
	 */
	void RestoreInstance(int32 EntryIndex, int32 TransformIndex);

	/**
	 * Computes the flat index into SpawnedEntities for (EntryIndex, TransformIndex).
	 * Sums InstanceTransforms.Num() for all entries before EntryIndex, then adds TransformIndex.
	 */
	int32 ComputeFlatIndex(int32 EntryIndex, int32 TransformIndex) const;

	/** World Partition runtime grid name this partition belongs to. */
	UPROPERTY(VisibleAnywhere, Category = "PlacedEntities")
	FName GridName;

	UPROPERTY(Transient)
	TArray<FMassEntityHandle> SpawnedEntities;

	/** Removal overlay — parallel to Entries[]. Named ClassRemovals to match the AIW JSON key so serialized blobs are interoperable during transition. */
	UPROPERTY(Transient)
	TArray<FArcPlacedEntityRemovals> ClassRemovals;

	/** Respawn queue — instances pending re-spawn after their RespawnTimeSeconds elapses. */
	UPROPERTY(Transient)
	TArray<FArcPlacedEntityPendingRespawn> PendingRespawns;

	/** Reverse lookup for MarkInstanceRemoved overload that takes an entity handle; maps entity -> (EntryIndex, TransformIndex). */
	TMap<FMassEntityHandle, TPair<int32, int32>> EntityToLocation;

	/** The single Mass entity that hooks this partition actor into the respawn processor graph. Lifetime tied to BeginPlay/EndPlay. */
	FMassEntityHandle TrackerEntity;

	/** True for the duration of EndPlay's DespawnEntities call so UArcPlacedEntityOwnerObserver
	 *  doesn't treat cell-unload destruction as a pickup-style removal. */
	bool bIsDespawningOnEndPlay = false;

	/** Pins Client-bundle assets (meshes/materials) for the actor's lifetime. Load-only v1: never released on stream-out. */
	TSharedPtr<FStreamableHandle> ClientBundleHandle;
	/** True while the Client-bundle load is in flight and SpawnEntities() hasn't run. */
	bool bPendingBundleSpawn = false;
#if WITH_EDITOR
	/** Editor-world async mesh/material load (non-PIE componentless preview). */
	TSharedPtr<FStreamableHandle> EditorMeshLoadHandle;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid GridGuid;

	// ---------------------------------------------------------------------------
	// Componentless MassEntity editor state (static-mesh entries only)
	// ---------------------------------------------------------------------------

	/** One composite sub-mesh's holder + slot + relative offset, for transform writeback. */
	struct FArcCompositeSubHolderRef
	{
		FMassEntityHandle Holder;
		int32 SubIndex = 0;
		FTransform RelativeTransform = FTransform::Identity;
		/** Skinned sub-holders live in SkinnedHolderRegistry; static ones in HolderRegistry. */
		bool bIsSkinned = false;
	};

	/** Per-instance info for the componentless render path. Static/skinned use Holder (single); composite leaves Holder invalid and fills CompositeHolders (one per sub-mesh). */
	struct FArcComponentlessInstanceInfo
	{
		FMassEntityHandle Source;
		FMassEntityHandle Holder;
		TArray<FArcCompositeSubHolderRef> CompositeHolders;
	};

	/** Holder registry: owns the ISM scene proxy descriptors in the editor world. */
	FArcISMHolderRegistry HolderRegistry;

	/** Maps holder key -> holder entity handle. One bucket per actor (one WP cell). */
	TMap<FArcISMHolderKey, FMassEntityHandle> HolderBucket;

	// Componentless skinned-mesh editor state (mirrors HolderRegistry/HolderBucket for skinned entries).
	// NOTE: reuses the shared InstanceToSourceEntity map + SourceEntityArchetype below — the
	// (EntryIndex, TransformIndex) key is unique per instance regardless of mesh type, and the source
	// archetype {FTransformFragment, FArcTypeElementFragment} is identical for static and skinned.
	FArcSkinnedMeshHolderRegistry SkinnedHolderRegistry;
	TMap<FArcSkinnedISMHolderKey, FMassEntityHandle> SkinnedHolderBucket;

	/** Maps (EntryIndex, TransformIndex) -> (Source, Holder) for transform writeback and teardown. */
	TMap<TPair<int32, int32>, FArcComponentlessInstanceInfo> InstanceToSourceEntity;

	/** Archetype for source entities: {FTransformFragment, FArcTypeElementFragment}. Built once. */
	FMassArchetypeHandle SourceEntityArchetype;

	/** Re-entrancy guard for RebuildEditorRenderState. */
	bool bIsRebuildingEditorRenderState = false;

	/** Selection snapshot carried across a teardown->rebuild cycle so the editor transform gizmo
	 *  survives render-state rebuilds (PostEditChangeProperty, material-compile poll, component re-register).
	 *  Reset+populated in TearDownEditorRenderState before it deselects; consumed+cleared at the tail of
	 *  RebuildEditorRenderState. Keyed (EntryIndex, TransformIndex) to match InstanceToSourceEntity. */
	TArray<TPair<int32, int32>> PendingSelectionRestore;

	// ---------------------------------------------------------------------------
	// Component-based preview state (skinned/Niagara/composite entries)
	// ---------------------------------------------------------------------------

	struct FArcPlacedEntityPreviewGroup
	{
		TArray<TObjectPtr<UNiagaraComponent>> NiagaraComponents;
		FTransform NiagaraComponentTransform = FTransform::Identity;
	};

	/** One group per entry. Single-mesh entries have 1 component; composite entries have N. */
	TArray<FArcPlacedEntityPreviewGroup> EditorPreviewGroups;

	/** Guard against re-entrant calls to RebuildEditorPreviewISMCs. */
	bool bIsRebuildingPreviewISMCs = false;

	/** Reverse index: LinkGuid -> (EntryIndex, TransformIndex). Rebuilt on any entry mutation. */
	TMap<FGuid, TPair<int32, int32>> GuidToInstanceMap;

	FDelegateHandle PreBeginPIEHandle;
	FDelegateHandle EndPIEHandle;

	FTSTicker::FDelegateHandle PendingRenderRebuildTickHandle;

	FDelegateHandle MaterialCompilationFinishedHandle;
	bool bForcePostCompileRebuild = false;
	int32 PostCompileRebuildPollTicks = 0;

	/** TEMP DIAGNOSTIC: subscription to UPackage::PackageMarkedDirtyEvent; dumps a callstack when this
	 *  actor's package is marked dirty. Remove once the perpetual-dirty source is identified. */
	FDelegateHandle PackageDirtyDiagHandle;
#endif
};
