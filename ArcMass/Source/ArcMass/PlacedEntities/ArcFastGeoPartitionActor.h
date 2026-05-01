// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorPartition/PartitionActor.h"
#include "Elements/SMInstance/SMInstanceManager.h"
#include "ArcPlacedEntityTypes.h"
#include "ArcFastGeoPartitionActor.generated.h"

class UMassEntityConfigAsset;
class UInstancedStaticMeshComponent;
class UFastGeoContainer;

UCLASS()
class ARCMASS_API AArcFastGeoPartitionActor : public APartitionActor, public ISMInstanceManager, public ISMInstanceManagerProvider
{
	GENERATED_BODY()

public:
	AArcFastGeoPartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, Category = "FastGeo")
	TArray<FArcPlacedEntityEntry> Entries;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override;
	virtual FGuid GetGridGuid() const override { return GridGuid; }
	void SetGridGuid(const FGuid& InGuid) { GridGuid = InGuid; }
	void SetGridName(FName InGridName) { GridName = InGridName; }

	virtual void PostRegisterAllComponents() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	void AddInstance(UMassEntityConfigAsset* ConfigAsset, const FTransform& Transform, int32& OutEntryIndex, int32& OutInstanceIndex);
	void RebuildEditorPreviewISMCs();
	UInstancedStaticMeshComponent* GetEditorPreviewISMC(int32 EntryIndex) const;
	void DestroyEditorPreviewISMCs();
	bool ResolveInstanceId(const FSMInstanceId& InstanceId, int32& OutEntryIndex, int32& OutTransformIndex) const;
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

	//~ ISMInstanceManagerProvider interface
	virtual ISMInstanceManager* GetSMInstanceManager(const FSMInstanceId& InstanceId) override;

#if WITH_EDITOR
	void OnPreBeginPIE(const bool bIsSimulating);
	void OnEndPIE(const bool bIsSimulating);
#endif

	void CreateFastGeoContainer();
	void DestroyFastGeoContainer();

	UPROPERTY(VisibleAnywhere, Category = "FastGeo")
	FName GridName;

	UPROPERTY(Transient)
	TObjectPtr<UFastGeoContainer> FastGeoContainerPtr;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid GridGuid;

	TArray<TObjectPtr<UInstancedStaticMeshComponent>> EditorPreviewISMCs;

	FDelegateHandle PreBeginPIEHandle;
	FDelegateHandle EndPIEHandle;
#endif
};
