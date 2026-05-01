// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorPartition/PartitionActor.h"
#include "Elements/SMInstance/SMInstanceManager.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceManager.h"
#include "MassEntityTypes.h"
#include "ArcPlacedEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "ArcPlacedEntityPartitionActor.generated.h"

class UMassEntityConfigAsset;
class UInstancedStaticMeshComponent;
class UNiagaraComponent;
class UArcPlacedEntityCollisionDrawComponent;
class UInstancedSkinnedMeshComponent;

/**
 * Partition actor that stores Mass entity config instances placed in the world.
 * In-editor, creates ISM components for visual preview and per-instance selection/manipulation.
 */
UCLASS()
class ARCMASS_API AArcPlacedEntityPartitionActor : public APartitionActor, public ISMInstanceManager, public ISMInstanceManagerProvider, public ISkMInstanceManager, public ISkMInstanceManagerProvider
{
	GENERATED_BODY()

public:
	AArcPlacedEntityPartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, Category = "PlacedEntities")
	TArray<FArcPlacedEntityEntry> Entries;

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
	virtual void PostLoad() override;
	//~ End AActor Interface

	/**
	 * Add a new instance of the given config at the given transform. Creates a new entry if needed.
	 * @param OutEntryIndex   Index into Entries[] for the matched/created entry, or INDEX_NONE on failure.
	 * @param OutInstanceIndex Index of the new transform within that entry's InstanceTransforms, or INDEX_NONE on failure.
	 */
	void AddInstance(UMassEntityConfigAsset* ConfigAsset, const FTransform& Transform, int32& OutEntryIndex, int32& OutInstanceIndex);

	/** Rebuild all editor preview ISM components from Entries data. */
	void RebuildEditorPreviewISMCs();

	/** Returns the editor preview ISMC for the given entry index, or nullptr if out of range / not yet created. */
	UInstancedStaticMeshComponent* GetEditorPreviewISMC(int32 EntryIndex) const;

	/** Destroy all editor preview ISM components. */
	void DestroyEditorPreviewISMCs();

	/** Resolve an FSMInstanceId to entry index and transform index. Returns false if not found. */
	bool ResolveInstanceId(const FSMInstanceId& InstanceId, int32& OutEntryIndex, int32& OutTransformIndex) const;

	/** Look up which entry/transform owns a given LinkGuid. Returns false if not found. */
	bool FindInstanceByGuid(const FGuid& Guid, int32& OutEntryIndex, int32& OutTransformIndex) const;

	/** Resolve an FSkMInstanceId to entry index and transform index. Returns false if not found. */
	bool ResolveSkMInstanceId(const FSkMInstanceId& InstanceId, int32& OutEntryIndex, int32& OutTransformIndex) const;

	/** Returns the first editor preview skinned mesh component for the given entry index, or nullptr. */
	UInstancedSkinnedMeshComponent* GetEditorPreviewSkMC(int32 EntryIndex) const;
#endif

	FName GetGridName() const { return GridName; }

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

	/** Rebuild GuidToInstanceMap from all entries' PerInstanceOverrides. */
	void RebuildGuidIndex();
#endif

	void SpawnEntities();
	void DespawnEntities();

	/** World Partition runtime grid name this partition belongs to. */
	UPROPERTY(VisibleAnywhere, Category = "PlacedEntities")
	FName GridName;

	UPROPERTY(Transient)
	TArray<FMassEntityHandle> SpawnedEntities;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid GridGuid;

	struct FArcPlacedEntityPreviewGroup
	{
		TArray<TObjectPtr<UInstancedStaticMeshComponent>> ISMComponents;
		/** Per-component relative transform (mesh entry's RelativeTransform or ComponentTransform). */
		TArray<FTransform> ComponentRelativeTransforms;

		TArray<TObjectPtr<UNiagaraComponent>> NiagaraComponents;
		FTransform NiagaraComponentTransform = FTransform::Identity;

		TArray<TObjectPtr<UInstancedSkinnedMeshComponent>> SkinnedMeshComponents;
		TArray<FTransform> SkinnedMeshRelativeTransforms;
	};

	/** One group per entry. Single-mesh entries have 1 component; composite entries have N. */
	TArray<FArcPlacedEntityPreviewGroup> EditorPreviewGroups;

	/** Guard against re-entrant calls to RebuildEditorPreviewISMCs. */
	bool bIsRebuildingPreviewISMCs = false;

	/** Reverse index: LinkGuid -> (EntryIndex, TransformIndex). Rebuilt on any entry mutation. */
	TMap<FGuid, TPair<int32, int32>> GuidToInstanceMap;

	UPROPERTY(Transient)
	TObjectPtr<UArcPlacedEntityCollisionDrawComponent> CollisionDrawComponent;

	FDelegateHandle PreBeginPIEHandle;
	FDelegateHandle EndPIEHandle;
#endif
};
