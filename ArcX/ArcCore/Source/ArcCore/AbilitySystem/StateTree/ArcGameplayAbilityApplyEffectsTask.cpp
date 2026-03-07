// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityApplyEffectsTask.h"

#include "StateTreeExecutionContext.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/ArcItemGameplayAbility.h"

EStateTreeRunStatus FArcGameplayAbilityApplyEffectsTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UArcItemGameplayAbility* ArcAbility = Cast<UArcItemGameplayAbility>(InstanceData.Ability);
	if (!ArcAbility)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.EffectTag.IsValid())
	{
		InstanceData.AppliedEffects = ArcAbility->ApplyEffectsFromItem(InstanceData.EffectTag, InstanceData.TargetData);
	}
	else
	{
		InstanceData.AppliedEffects = ArcAbility->ApplyAllEffectsFromItem(InstanceData.TargetData);
	}

	return EStateTreeRunStatus::Succeeded;
}
