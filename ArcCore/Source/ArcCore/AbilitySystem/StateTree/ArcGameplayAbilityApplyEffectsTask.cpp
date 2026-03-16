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

#if WITH_EDITOR
FText FArcGameplayAbilityApplyEffectsTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData && InstanceData->EffectTag.IsValid())
		{
			return FText::Format(NSLOCTEXT("ArcCore", "ApplyEffectsDesc", "Apply Effects: {0}"), FText::FromString(InstanceData->EffectTag.ToString()));
		}
	}
	return NSLOCTEXT("ArcCore", "ApplyEffectsDescDefault", "Apply All Effects");
}
#endif
