// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorPartition/PartitionActor.h"
#include "Elements/SMInstance/SMInstanceManager.h"
#include "MassEntityTypes.h"
#include "ArcPlacedEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "ArcPlacedEntityPartitionActor.generated.h"

class UMassEntityConfigAsset;
class UInstancedStaticMeshComponent;
class UNiagaraComponent;

/**
 * Partition actor that stores Mass entity config instances placed in the world.
 * In-editor, creates ISM components for visual preview and per-instance selection/manipulation.
 */
UCLASS()
class ARCMASS_API AArcPlacedEntityPartitionActor : public APartitionActor, public ISMInstanceManager, public ISMInstanceManagerProvider
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
	//~ End APartitionActor Interface

	//~ Begin AActor Interface
	virtual void PostRegisterAllComponents() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
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
#endif

	virtual void BeginDestroy() override;

private:
	//~ ISMInstanceManager interface
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

	//~ ISMInstanceManagerProvider interface
	virtual ISMInstanceManager* GetSMInstanceManager(const FSMInstanceId& InstanceId) override;

#if WITH_EDITOR
	/** Resolve an FSMInstanceId to entry index and transform index. Returns false if not found. */
	bool ResolveInstanceId(const FSMInstanceId& InstanceId, int32& OutEntryIndex, int32& OutTransformIndex) const;

	void OnPreBeginPIE(const bool bIsSimulating);
	void OnEndPIE(const bool bIsSimulating);
#endif

	void SpawnEntities();
	void DespawnEntities();

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
	};

	/** One group per entry. Single-mesh entries have 1 component; composite entries have N. */
	TArray<FArcPlacedEntityPreviewGroup> EditorPreviewGroups;

	/** Guard against re-entrant calls to RebuildEditorPreviewISMCs. */
	bool bIsRebuildingPreviewISMCs = false;

	FDelegateHandle PreBeginPIEHandle;
	FDelegateHandle EndPIEHandle;
#endif
};
