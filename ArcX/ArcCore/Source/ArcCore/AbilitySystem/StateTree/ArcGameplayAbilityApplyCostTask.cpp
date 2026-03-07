// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityApplyCostTask.h"

#include "StateTreeExecutionContext.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"

EStateTreeRunStatus FArcGameplayAbilityApplyCostTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(InstanceData.Ability);
	if (!ArcAbility)
	{
		return EStateTreeRunStatus::Failed;
	}

	ArcAbility->ApplyCost(
		ArcAbility->GetCurrentAbilitySpecHandle(),
		ArcAbility->GetCurrentActorInfo(),
		ArcAbility->GetCurrentActivationInfo());

	return EStateTreeRunStatus::Succeeded;
}
