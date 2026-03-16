// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilitySendTargetingResultTask.h"

#include "StateTreeExecutionContext.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/ArcItemGameplayAbility.h"
#include "AbilitySystem/Targeting/ArcTargetingObject.h"
#include "Items/ArcItemDefinition.h"

EStateTreeRunStatus FArcGameplayAbilitySendTargetingResultTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UArcItemGameplayAbility* ArcAbility = Cast<UArcItemGameplayAbility>(InstanceData.Ability);
	if (!ArcAbility)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Resolve targeting object
	UArcTargetingObject* TargetingObj = InstanceData.TargetingObject;
	if (!TargetingObj)
	{
		const UArcItemDefinition* ItemDef = ArcAbility->NativeGetSourceItemData();
		if (ItemDef)
		{
			if (const auto* Fragment = ItemDef->FindFragment<FArcItemFragment_TargetingObjectPreset>())
			{
				TargetingObj = Fragment->TargetingObject;
			}
		}
	}

	ArcAbility->SendTargetingResult(InstanceData.TargetData, TargetingObj);

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FArcGameplayAbilitySendTargetingResultTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData && InstanceData->TargetingObject)
		{
			return FText::Format(NSLOCTEXT("ArcCore", "SendTargetingResultDesc", "Send Targeting Result: {0}"), FText::FromString(GetNameSafe(InstanceData->TargetingObject)));
		}
	}
	return NSLOCTEXT("ArcCore", "SendTargetingResultDescDefault", "Send Targeting Result");
}
#endif
