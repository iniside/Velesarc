// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "StateTreeInstanceData.h"
#include "StateTreeReference.h"

#include "ArcMassRunStateTreeTask.generated.h"

class UStateTree;
struct FMassStateTreeExecutionContext;

USTRUCT()
struct FArcMassRunStateTreeTaskInstanceData
{
	GENERATED_BODY()

	/** Optional runtime override. When bound, replaces the tree in the task's StateTreeReference.
	 *  Parameters are always taken from StateTreeReference regardless of which tree is used. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UStateTree> StateTreeOverride;

	/** Internal runtime state for the nested tree. Lifetime tied to the task. */
	UPROPERTY()
	FStateTreeInstanceData StateTreeInstanceData;

	/** Whether the nested tree was successfully started. Guards Tick and ExitState. */
	bool bStarted = false;
};

/**
 * Runs a nested state tree in-place during the parent tree's tick.
 * All nodes in the nested tree have full access to FMassStateTreeExecutionContext,
 * meaning they can read/write entity fragments, access subsystems, and use any
 * Mass-aware evaluator, task, or condition.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Run State Tree", Category = "Mass", ToolTip = "Runs a nested Mass state tree in-place. Supports optional runtime tree override."))
struct FArcMassRunStateTreeTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassRunStateTreeTaskInstanceData;

public:
	FArcMassRunStateTreeTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** The nested state tree asset and parameter overrides. Constrained to UMassStateTreeSchema. */
	UPROPERTY(EditAnywhere, meta = (Schema = "/Script/MassAIBehavior.MassStateTreeSchema"))
	FStateTreeReference StateTreeReference;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

private:
	const UStateTree* GetEffectiveTree(const FInstanceDataType& InstanceData) const;
};
