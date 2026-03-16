// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityCheckCostCondition.h"

#include "StateTreeExecutionContext.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"

bool FArcGameplayAbilityCheckCostCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(InstanceData.Ability);
	if (!ArcAbility)
	{
		return false;
	}

	return ArcAbility->CheckCost(
		ArcAbility->GetCurrentAbilitySpecHandle(),
		ArcAbility->GetCurrentActorInfo(),
		nullptr);
}

#if WITH_EDITOR
FText FArcGameplayAbilityCheckCostCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcCore", "CheckCostDesc", "Check Cost");
}
#endif
