// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSetBoolTask.h"

FArcMassSetBoolTask::FArcMassSetBoolTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassSetBoolTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	
	if (!bSetOnExit)
	{
		bool* bValue = InstanceData.BoolToSet.GetMutablePtr<bool>(Context);
		if (bValue)
		{
			*bValue = InstanceData.bNewValue;
		}
	}
	
	
	return EStateTreeRunStatus::Running;
}

void FArcMassSetBoolTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	if (bSetOnExit)
	{
		bool* bValue = InstanceData.BoolToSet.GetMutablePtr<bool>(Context);
		if (bValue)
		{
			*bValue = InstanceData.bNewValue;
		}
	}
}

#if WITH_EDITOR
FText FArcMassSetBoolTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "SetBoolDesc", "Set Bool to {0}{1}"), InstanceData->bNewValue ? FText::FromString(TEXT("True")) : FText::FromString(TEXT("False")), bSetOnExit ? FText::FromString(TEXT(" (on exit)")) : FText::GetEmpty());
		}
	}
	return NSLOCTEXT("ArcAI", "SetBoolDescDefault", "Set Bool");
}
#endif
