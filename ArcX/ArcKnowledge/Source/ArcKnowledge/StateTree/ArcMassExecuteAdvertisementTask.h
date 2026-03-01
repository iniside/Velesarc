// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcAdvertisementExecutionContext.h"
#include "MassEntityFragments.h"
#include "ArcMassExecuteAdvertisementTask.generated.h"

class UMassSignalSubsystem;

USTRUCT()
struct FArcMassExecuteAdvertisementTaskInstanceData
{
	GENERATED_BODY()

	/** Handle to the claimed advertisement to execute. */
	UPROPERTY(EditAnywhere, Category = Input)
	FArcKnowledgeHandle AdvertisementHandle;

	/** The execution context that runs the advertisement's StateTree. */
	UPROPERTY()
	FArcAdvertisementExecutionContext ExecutionContext;
};

/**
 * StateTree task that executes a claimed advertisement's behavioral instruction.
 * Extracts the FArcAdvertisementInstruction_StateTree from the advertisement entry,
 * creates an execution context, and runs the StateTree until completion.
 *
 * The advertisement must be claimed before executing this task.
 */
USTRUCT(meta = (DisplayName = "Arc Execute Advertisement", Category = "ArcKnowledge"))
struct ARCKNOWLEDGE_API FArcMassExecuteAdvertisementTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassExecuteAdvertisementTaskInstanceData;

	FArcMassExecuteAdvertisementTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Whether to complete the advertisement when the behavior finishes successfully. */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bCompleteOnSuccess = true;

	/** Whether to cancel the claim when the behavior is interrupted. */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bCancelOnInterrupt = true;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif

private:
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> SignalSubsystemHandle;
};
