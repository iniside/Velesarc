// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassSmartObjectTypes.h"
#include "SmartObjectRuntime.h"
#include "Tasks/ArcMassStateTreeRunEnvQueryTask.h"
#include "UObject/Object.h"
#include "ArcMassClaimSmartObjectTask.generated.h"

/**
 * 
 */
struct FStateTreeExecutionContext;
struct FMassSmartObjectUserFragment;
class USmartObjectSubsystem;
class UMassSignalSubsystem;
struct FTransformFragment;
struct FMassMoveTargetFragment;
namespace UE::MassBehavior
{
	struct FStateTreeDependencyBuilder;
};
namespace UE::Mass::SmartObject
{
	struct FMRUSlotsFragment;
}

/**
 * Tasks to claim a smart object from search results and release it when done.
 */
USTRUCT()
struct FArcMassClaimSmartObjectTaskInstanceData
{
	GENERATED_BODY()

	/** Result of the candidates search request (Input) */
	//UPROPERTY(VisibleAnywhere, Category = Input, meta = (RefType = "/Script/MassSmartObjects.MassSmartObjectCandidateSlots"))
	UPROPERTY(VisibleAnywhere, Category = Input)
	FArcMassSmartObjectItem SmartObjectItem;

	UPROPERTY(VisibleAnywhere, Category = Output)
	FSmartObjectClaimHandle ClaimedSlot;

	UPROPERTY(VisibleAnywhere, Category = Output)
	FTransform ClaimedSlotTransform;
};

USTRUCT(meta = (DisplayName = "Arc Claim SmartObject", Category = "Arc|SmartObject"))
struct FArcMassClaimSmartObjectTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassClaimSmartObjectTaskInstanceData;

	FArcMassClaimSmartObjectTask();

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	TStateTreeExternalDataHandle<FMassSmartObjectUserFragment> SmartObjectUserHandle;
	TStateTreeExternalDataHandle<UE::Mass::SmartObject::FMRUSlotsFragment, EStateTreeExternalDataRequirement::Optional> SmartObjectMRUSlotsHandle;
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;

	/** Delay in seconds before trying to use another smart object */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float InteractionCooldown = 0.f;
};