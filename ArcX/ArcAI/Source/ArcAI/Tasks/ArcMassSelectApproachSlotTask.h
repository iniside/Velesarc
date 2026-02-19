// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "GameplayTagContainer.h"
#include "MassActorSubsystem.h"
#include "Components/SceneComponent.h"
#include "UObject/Object.h"
#include "ArcMassSelectApproachSlotTask.generated.h"

USTRUCT(BlueprintType)
struct FArcDynamicSlotInfo
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, meta = (Categories = "AI, DynamicSlots"))
	FGameplayTag SlotTag;
	
	UPROPERTY(EditAnywhere, meta = (MakeEditWidget = true))
	FTransform SlotLocation;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UObject> Owner;
	
	UPROPERTY()
	FMassEntityHandle OwnerHandle;
};

UCLASS(BlueprintType, editinlinenew, meta = (BlueprintSpawnableComponent))
class UArcDynamicSlotComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Slots", meta = (MakeEditWidget = true))
	TArray<FArcDynamicSlotInfo> Slots;
	
	UPROPERTY(EditAnywhere, meta = (MakeEditWidget = true))
	TArray<FTransform> SlotLocation;
	
	TArray<int32> TakenSlots;
	void ClaimSlot(const FGameplayTag& SlotTag, UObject* Owner, FVector& OutLocation, int32 &OutSlotIndex);

	void FreeSlot(int32 SlotIndex);
};

USTRUCT()
struct FArcMassSelectApproachSlotTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> Target;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag SlotTag;
	
	UPROPERTY(EditAnywhere, Category = Output)
	FVector WorldLocation;
	
	UPROPERTY(EditAnywhere, Category = Output)
	bool bSuccess = false;
	
	UPROPERTY(VisibleAnywhere)
	int32 SlotIndex = INDEX_NONE;
};

USTRUCT(DisplayName="Arc Mass Select Approach Slot Task", meta = (Category = "Arc|Action"))
struct FArcMassSelectApproachSlotTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassSelectApproachSlotTaskInstanceData;

	FArcMassSelectApproachSlotTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};
