// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Elements/SMInstance/SMInstanceManager.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceManager.h"
#include "MassEntityTypes.h"
#include "ArcPlacedEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "ArcAlwaysLoadedEntityActor.generated.h"

class UMassEntityConfigAsset;
class UInstancedStaticMeshComponent;
class UNiagaraComponent;
class UArcPlacedEntityCollisionDrawComponent;
class UInstancedSkinnedMeshComponent;

/**
 * Always-loaded actor that spawns Mass entities unconditionally on BeginPlay.
 * Singleton per level — all entity configs with UArcAlwaysLoadedEntityTrait route here.
 * Provides the same ISM/SkM editor preview and per-instance manipulation as the partition actor.
 */
UCLASS()
class ARCMASS_API AArcAlwaysLoadedEntityActor : public AActor, public ISMInstanceManager, public ISMInstanceManagerProvider, public ISkMInstanceManager, public ISkMInstanceManagerProvider
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
    virtual void PostLoad() override;

    void AddInstance(UMassEntityConfigAsset* ConfigAsset, const FTransform& Transform, int32& OutEntryIndex, int32& OutInstanceIndex);

    void RebuildEditorPreviewISMCs();
    UInstancedStaticMeshComponent* GetEditorPreviewISMC(int32 EntryIndex) const;
    void DestroyEditorPreviewISMCs();

    bool ResolveInstanceId(const FSMInstanceId& InstanceId, int32& OutEntryIndex, int32& OutTransformIndex) const;
    bool FindInstanceByGuid(const FGuid& Guid, int32& OutEntryIndex, int32& OutTransformIndex) const;

    bool ResolveSkMInstanceId(const FSkMInstanceId& InstanceId, int32& OutEntryIndex, int32& OutTransformIndex) const;
    UInstancedSkinnedMeshComponent* GetEditorPreviewSkMC(int32 EntryIndex) const;
#endif

    const TArray<FArcPlacedEntityEntry>& GetEntries() const { return Entries; }

    virtual void BeginDestroy() override;

private:
    //~ ISMInstanceManager interface
    virtual FText GetSMInstanceDisplayName(const FSMInstanceId& InstanceId) const override;
    virtual FText GetSMInstanceTooltip(const FSMInstanceId& InstanceId) const override;
    virtual void ForEachSMInstanceInSelectionGroup(const FSMInstanceId& InstanceId, TFunctionRef<bool(FSMInstanceId)> Callback) override;
    virtual bool CanEditSMInstance(const FSMInstanceId& InstanceId) const override;
    virtual bool CanMoveSMInstance(const FSMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const override;
    virtual bool GetSMInstanceTransform(const FSMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace = false) const override;
    virtual bool SetSMInstanceTransform(const FSMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false) override;
    virtual void NotifySMInstanceMovementStarted(const FSMInstanceId& InstanceId) override;
    virtual void NotifySMInstanceMovementOngoing(const FSMInstanceId& InstanceId) override;
    virtual void NotifySMInstanceMovementEnded(const FSMInstanceId& InstanceId) override;
    virtual void NotifySMInstanceSelectionChanged(const FSMInstanceId& InstanceId, const bool bIsSelected) override;
    virtual bool DeleteSMInstances(TArrayView<const FSMInstanceId> InstanceIds) override;
    virtual bool DuplicateSMInstances(TArrayView<const FSMInstanceId> InstanceIds, TArray<FSMInstanceId>& OutNewInstanceIds) override;
    virtual TSubclassOf<USMInstanceProxyEditingObject> GetSMInstanceEditingProxyClass() const override;

    //~ ISMInstanceManagerProvider interface
    virtual ISMInstanceManager* GetSMInstanceManager(const FSMInstanceId& InstanceId) override;

    //~ ISkMInstanceManager interface
    virtual FText GetSkMInstanceDisplayName(const FSkMInstanceId& InstanceId) const override;
    virtual FText GetSkMInstanceTooltip(const FSkMInstanceId& InstanceId) const override;
    virtual void ForEachSkMInstanceInSelectionGroup(const FSkMInstanceId& InstanceId, TFunctionRef<bool(FSkMInstanceId)> Callback) override;
    virtual bool CanEditSkMInstance(const FSkMInstanceId& InstanceId) const override;
    virtual bool CanMoveSkMInstance(const FSkMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const override;
    virtual bool GetSkMInstanceTransform(const FSkMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace = false) const override;
    virtual bool SetSkMInstanceTransform(const FSkMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false) override;
    virtual void NotifySkMInstanceMovementStarted(const FSkMInstanceId& InstanceId) override;
    virtual void NotifySkMInstanceMovementOngoing(const FSkMInstanceId& InstanceId) override;
    virtual void NotifySkMInstanceMovementEnded(const FSkMInstanceId& InstanceId) override;
    virtual void NotifySkMInstanceSelectionChanged(const FSkMInstanceId& InstanceId, const bool bIsSelected) override;
    virtual bool DeleteSkMInstances(TArrayView<const FSkMInstanceId> InstanceIds) override;
    virtual bool DuplicateSkMInstances(TArrayView<const FSkMInstanceId> InstanceIds, TArray<FSkMInstanceId>& OutNewInstanceIds) override;

    //~ ISkMInstanceManagerProvider interface
    virtual ISkMInstanceManager* GetSkMInstanceManager(const FSkMInstanceId& InstanceId) override;

#if WITH_EDITOR
    void OnPreBeginPIE(const bool bIsSimulating);
    void OnEndPIE(const bool bIsSimulating);
    void AutoAssignLinkGuidIfNeeded(FArcPlacedEntityEntry& Entry, int32 InstanceIndex);
    void RebuildGuidIndex();
#endif

    void SpawnEntities();
    void DespawnEntities();

    UPROPERTY(Transient)
    TArray<FMassEntityHandle> SpawnedEntities;

#if WITH_EDITORONLY_DATA
    struct FArcPlacedEntityPreviewGroup
    {
        TArray<TObjectPtr<UInstancedStaticMeshComponent>> ISMComponents;
        TArray<FTransform> ComponentRelativeTransforms;

        TArray<TObjectPtr<UNiagaraComponent>> NiagaraComponents;
        FTransform NiagaraComponentTransform = FTransform::Identity;

        TArray<TObjectPtr<UInstancedSkinnedMeshComponent>> SkinnedMeshComponents;
        TArray<FTransform> SkinnedMeshRelativeTransforms;
    };

    TArray<FArcPlacedEntityPreviewGroup> EditorPreviewGroups;
    bool bIsRebuildingPreviewISMCs = false;
    TMap<FGuid, TPair<int32, int32>> GuidToInstanceMap;

    UPROPERTY(Transient)
    TObjectPtr<UArcPlacedEntityCollisionDrawComponent> CollisionDrawComponent;

    FDelegateHandle PreBeginPIEHandle;
    FDelegateHandle EndPIEHandle;
#endif
};
