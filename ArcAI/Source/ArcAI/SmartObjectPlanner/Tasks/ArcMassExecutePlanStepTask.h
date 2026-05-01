// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcGameplayInteractionContext.h"
#include "ArcAdvertisementExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcMass/ArcMassEntityHandleWrapper.h"

#include "ArcMassExecutePlanStepTask.generated.h"

class UGameplayBehavior;
class USmartObjectSubsystem;
class UMassSignalSubsystem;
struct FMassSmartObjectUserFragment;
struct FMassMoveTargetFragment;
struct FTransformFragment;

/**
 * Unified execution task for the SmartObject Planner pipeline.
 *
 * Given the outputs of ClaimPlanStepTask, executes whichever interaction
 * was claimed — SmartObject behaviors or knowledge advertisement instruction.
 *
 * Discriminator: AdvertisementHandle.IsValid()
 *   true  -> knowledge path: look up entry, extract FArcAdvertisementInstruction_StateTree,
 *            run via FArcAdvertisementExecutionContext (Activate/Tick/Deactivate)
 *   false -> SO path: run GameplayInteraction / GameplayBehavior / MassBehavior
 *            chain on the claimed slot (same as FArcMassUseSmartObjectTask)
 *
 * Both paths return Running until complete, then Succeeded.
 * ExitState cleans up the active path (deactivate context or release SO).
 */
USTRUCT()
struct FArcMassExecutePlanStepTaskInstanceData
{
	GENERATED_BODY()

	// --- Inputs (bound from ClaimPlanStepTask outputs) ---

	UPROPERTY(EditAnywhere, Category = Input)
	FSmartObjectClaimHandle SmartObjectClaimHandle;

	UPROPERTY(EditAnywhere, Category = Input)
	FArcMassEntityHandleWrapper SmartObjectEntityHandle;

	UPROPERTY(EditAnywhere, Category = Input)
	FArcKnowledgeHandle AdvertisementHandle;

	// --- Internal state ---

	UPROPERTY()
	FArcAdvertisementExecutionContext AdvertisementExecutionContext;

	UPROPERTY()
	FArcGameplayInteractionContext GameplayInteractionContext;

	UPROPERTY()
	TObjectPtr<UGameplayBehavior> GameplayBehavior;
};

USTRUCT(meta = (DisplayName = "Arc Execute Plan Step", Category = "Smart Object Planner"))
struct ARCAI_API FArcMassExecutePlanStepTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassExecutePlanStepTaskInstanceData;

	FArcMassExecutePlanStepTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	UPROPERTY(EditAnywhere, Category = "SmartObject")
	bool bAlwaysRunMassBehavior = true;

	UPROPERTY(EditAnywhere, Category = "Knowledge")
	bool bCompleteOnSuccess = true;

	UPROPERTY(EditAnywhere, Category = "Knowledge")
	bool bCancelOnInterrupt = true;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

private:
	TStateTreeExternalDataHandle<FMassSmartObjectUserFragment> SmartObjectUserHandle;
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};
