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
