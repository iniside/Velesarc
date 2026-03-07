// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityCheckTagsCondition.h"

#include "AbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"

bool FArcGameplayAbilityCheckTagsCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AbilitySystemComponent)
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	InstanceData.AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);

	if (InstanceData.RequiredTags.Num() > 0 && !OwnedTags.HasAll(InstanceData.RequiredTags))
	{
		return false;
	}

	if (InstanceData.BlockedTags.Num() > 0 && OwnedTags.HasAny(InstanceData.BlockedTags))
	{
		return false;
	}

	return true;
}
