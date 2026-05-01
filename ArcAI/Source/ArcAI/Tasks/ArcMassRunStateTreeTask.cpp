// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassRunStateTreeTask.h"

#include "MassStateTreeExecutionContext.h"
#include "StateTree.h"

FArcMassRunStateTreeTask::FArcMassRunStateTreeTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = false;
}

const UStateTree* FArcMassRunStateTreeTask::GetEffectiveTree(const FInstanceDataType& InstanceData) const
{
	return InstanceData.StateTreeOverride != nullptr
		? InstanceData.StateTreeOverride.Get()
		: StateTreeReference.GetStateTree();
}

EStateTreeRunStatus FArcMassRunStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.bStarted = false;

	const UStateTree* EffectiveTree = GetEffectiveTree(InstanceData);
	if (EffectiveTree == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	FMassStateTreeExecutionContext NestedCtx(
		*MassCtx.GetOwner(),
		*EffectiveTree,
		InstanceData.StateTreeInstanceData,
		MassCtx.GetMassEntityExecutionContext());

	NestedCtx.SetEntity(MassCtx.GetEntity());

	const EStateTreeRunStatus Status = NestedCtx.Start(&StateTreeReference.GetParameters());
	InstanceData.bStarted = (Status == EStateTreeRunStatus::Running);
	return Status;
}

EStateTreeRunStatus FArcMassRunStateTreeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.bStarted)
	{
		return EStateTreeRunStatus::Failed;
	}

	const UStateTree* EffectiveTree = GetEffectiveTree(InstanceData);
	if (EffectiveTree == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	FMassStateTreeExecutionContext NestedCtx(
		*MassCtx.GetOwner(),
		*EffectiveTree,
		InstanceData.StateTreeInstanceData,
		MassCtx.GetMassEntityExecutionContext());

	NestedCtx.SetEntity(MassCtx.GetEntity());

	return NestedCtx.Tick(DeltaTime);
}

void FArcMassRunStateTreeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.bStarted)
	{
		return;
	}

	const UStateTree* EffectiveTree = GetEffectiveTree(InstanceData);
	if (EffectiveTree == nullptr)
	{
		return;
	}

	FMassStateTreeExecutionContext NestedCtx(
		*MassCtx.GetOwner(),
		*EffectiveTree,
		InstanceData.StateTreeInstanceData,
		MassCtx.GetMassEntityExecutionContext());

	NestedCtx.SetEntity(MassCtx.GetEntity());

	NestedCtx.Stop();
	InstanceData.bStarted = false;
}

#if WITH_EDITOR
FText FArcMassRunStateTreeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const UStateTree* StateTree = StateTreeReference.GetStateTree();
	if (StateTree != nullptr)
	{
		return FText::Format(
			NSLOCTEXT("ArcAI", "RunStateTreeDescNamed", "Run State Tree: {0}"),
			FText::FromString(StateTree->GetName()));
	}
	return NSLOCTEXT("ArcAI", "RunStateTreeDescOverride", "Run State Tree: Override");
}
#endif
