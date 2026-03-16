// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityApplyCooldownTask.h"

#include "StateTreeExecutionContext.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"

EStateTreeRunStatus FArcGameplayAbilityApplyCooldownTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(InstanceData.Ability);
	if (!ArcAbility)
	{
		return EStateTreeRunStatus::Failed;
	}

	ArcAbility->ApplyCooldown(
		ArcAbility->GetCurrentAbilitySpecHandle(),
		ArcAbility->GetCurrentActorInfo(),
		ArcAbility->GetCurrentActivationInfo());

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FArcGameplayAbilityApplyCooldownTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcCore", "ApplyCooldownDesc", "Apply Cooldown");
}
#endif
