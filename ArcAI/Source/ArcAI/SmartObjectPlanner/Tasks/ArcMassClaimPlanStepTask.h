// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "MassSmartObjectRequest.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcMass/ArcMassEntityHandleWrapper.h"

#include "ArcMassClaimPlanStepTask.generated.h"

class USmartObjectSubsystem;
class UMassSignalSubsystem;
struct FMassSmartObjectUserFragment;
namespace UE::MassBehavior
{
	struct FStateTreeDependencyBuilder;
}
namespace UE::Mass::SmartObject
{
	struct FMRUSlotsFragment;
}

/**
 * Unified claim task for the SmartObject Planner execution pipeline.
 *
 * Receives step data from GetNextPlanStepTask and claims either a SmartObject slot
 * or a knowledge advertisement depending on what the step carries.
 *
 * Discriminator: KnowledgeHandle.IsValid()
 *   true  -> claim advertisement via UArcKnowledgeSubsystem
 *   false -> claim SO slot from CandidateSlots via USmartObjectSubsystem
 *
 * Outputs both claim handles — downstream tasks check which is valid.
 * Releases the active claim on ExitState.
 */
USTRUCT()
struct FArcMassClaimPlanStepTaskInstanceData
{
	GENERATED_BODY()

	// --- Inputs (bound from GetNextPlanStepTask outputs) ---

	UPROPERTY(EditAnywhere, Category = Input)
	FArcMassEntityHandleWrapper SmartObjectEntityHandle;

	UPROPERTY(VisibleAnywhere, Category = Input)
	FMassSmartObjectCandidateSlots CandidateSlots;

	UPROPERTY(EditAnywhere, Category = Input)
	FArcKnowledgeHandle KnowledgeHandle;

	// --- Outputs ---

	UPROPERTY(EditAnywhere, Category = Output)
	FSmartObjectClaimHandle SmartObjectClaimHandle;

	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeHandle ClaimedAdvertisementHandle;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bClaimSucceeded = false;
};

USTRUCT(meta = (DisplayName = "Arc Claim Plan Step", Category = "Smart Object Planner"))
struct ARCAI_API FArcMassClaimPlanStepTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassClaimPlanStepTaskInstanceData;

	FArcMassClaimPlanStepTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float InteractionCooldown = 0.f;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

private:
	TStateTreeExternalDataHandle<FMassSmartObjectUserFragment> SmartObjectUserHandle;
	TStateTreeExternalDataHandle<UE::Mass::SmartObject::FMRUSlotsFragment, EStateTreeExternalDataRequirement::Optional> SmartObjectMRUSlotsHandle;
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
};
