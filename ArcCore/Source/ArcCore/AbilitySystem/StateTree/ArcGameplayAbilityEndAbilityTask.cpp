// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityEndAbilityTask.h"

#include "AbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FArcGameplayAbilityEndAbilityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Ability)
	{
		return EStateTreeRunStatus::Failed;
	}



	InstanceData.AbilitySystemComponent->CallServerEndAbility(InstanceData.Ability->GetCurrentAbilitySpecHandle(),
		InstanceData.Ability->GetCurrentActivationInfo(), InstanceData.AbilitySystemComponent->ScopedPredictionKey);

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FArcGameplayAbilityEndAbilityTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData && InstanceData->bWasCancelled)
		{
			return NSLOCTEXT("ArcCore", "EndAbilityCancelledDesc", "Cancel Ability");
		}
	}
	return NSLOCTEXT("ArcCore", "EndAbilityDesc", "End Ability");
}
#endif
