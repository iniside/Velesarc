// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityManager.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityOverrideHost.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/ArchetypeTypes.h"
#include "ArcPlacedEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Mesh/ArcISMHolderRegistry.h"
#include "ArcMass/SkinnedMeshVisualization/ArcSkinnedMeshHolderRegistry.h"
#include "ArcAlwaysLoadedEntityActor.generated.h"

class UMassEntityConfigAsset;
class UInstancedStaticMeshComponent;
class UNiagaraComponent;
class UMaterialInterface;
struct FStreamableHandle;

/**
 * Always-loaded actor that spawns Mass entities unconditionally on BeginPlay.
 * Singleton per level — all entity configs with UArcAlwaysLoadedEntityTrait route here.
 */
UCLASS()
class ARCMASS_API AArcAlwaysLoadedEntityActor : public AActor, public IArcMassEntityManager, public IArcMassEntityManagerProvider, public IArcPlacedEntityOverrideHost
{
    GENERATED_BODY()

public:
    AArcAlwaysLoadedEntityActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UPROPERTY(EditAnywhere, Category = "PlacedEntities")
    TArray<FArcPlacedEntityEntry> Entries;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
    virtual void PostRegisterAllComponents() override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditMove(bool bFinished) override;
    virtual void PostLoad() override;

    void AddInstance(UMassEntityConfigAsset* ConfigAsset, const FTransform& Transform, int32& OutEntryIndex, int32& OutInstanceIndex);

    void RebuildEditorPreviewISMCs();
    void DestroyEditorPreviewISMCs();

    bool FindInstanceByGuid(const FGuid& Guid, int32& OutEntryIndex, int32& OutTransformIndex) const;

    /** Returns the mesh-accurate world-space bounds for a MassEntity element instance. Used by GetBounds in the world interface. */
    bool GetMassEntityInstanceMeshBounds(const FArcMassEntityElementId& InId, FBoxSphereBounds& OutBounds) const;

    FTransform GetEntryComponentTransform(const FArcPlacedEntityEntry& Entry) const;
#endif

    const TArray<FArcPlacedEntityEntry>& GetEntries() const { return Entries; }

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
    void AutoAssignLinkGuidIfNeeded(FArcPlacedEntityEntry& Entry, int32 InstanceIndex);
    void RebuildGuidIndex();

    /** True iff the entry should use the componentless MassEntity editor path (static mesh only, no composite/skinned/Niagara). */
    bool IsComponentlessEntry(const FArcPlacedEntityEntry& Entry) const;

    /** True iff the entry should use the skinned componentless MassEntity editor path (skinned mesh only, no composite/Niagara, not already a static componentless entry). */
    bool IsSkinnedComponentlessEntry(const FArcPlacedEntityEntry& Entry) const;

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

    /**
     * Rebuild both editor paths atomically.
     * Order: TearDownEditorRenderState -> DestroyEditorPreviewISMCs -> RebuildEditorRenderState -> RebuildEditorPreviewISMCs.
     */
    void RebuildAllEditorState();

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

    UPROPERTY(Transient)
    TArray<FMassEntityHandle> SpawnedEntities;

    /** Pins Client-bundle assets (meshes/materials) for the actor's lifetime. Load-only v1: never released on stream-out. */
    TSharedPtr<FStreamableHandle> ClientBundleHandle;
    /** True while the Client-bundle load is in flight and SpawnEntities() hasn't run. */
    bool bPendingBundleSpawn = false;
#if WITH_EDITOR
    /** Editor-world async mesh/material load (non-PIE componentless preview). */
    TSharedPtr<FStreamableHandle> EditorMeshLoadHandle;
#endif

#if WITH_EDITORONLY_DATA
    struct FArcPlacedEntityPreviewGroup
    {
        TArray<TObjectPtr<UNiagaraComponent>> NiagaraComponents;
        FTransform NiagaraComponentTransform = FTransform::Identity;
    };

    TArray<FArcPlacedEntityPreviewGroup> EditorPreviewGroups;
    bool bIsRebuildingPreviewISMCs = false;
    TMap<FGuid, TPair<int32, int32>> GuidToInstanceMap;

    FDelegateHandle PreBeginPIEHandle;
    FDelegateHandle EndPIEHandle;

    FTSTicker::FDelegateHandle PendingRenderRebuildTickHandle;

    FDelegateHandle MaterialCompilationFinishedHandle;
    bool bForcePostCompileRebuild = false;
    int32 PostCompileRebuildPollTicks = 0;

    // ---------------------------------------------------------------------------
    // Componentless MassEntity editor state (static-mesh entries only)
    // ---------------------------------------------------------------------------

    /** Per-instance info for the componentless render path: source entity + holder entity. */
    struct FArcComponentlessInstanceInfo { FMassEntityHandle Source; FMassEntityHandle Holder; };

    /** Holder registry: owns the ISM scene proxy descriptors in the editor world. */
    FArcISMHolderRegistry HolderRegistry;

    /** Maps holder key -> holder entity handle. One bucket (singleton actor). */
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
#endif
};
