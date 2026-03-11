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

#if WITH_EDITOR
FText FArcGameplayAbilityCheckTagsCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			const int32 RequiredNum = InstanceData->RequiredTags.Num();
			const int32 BlockedNum = InstanceData->BlockedTags.Num();
			if (RequiredNum > 0 || BlockedNum > 0)
			{
				return FText::Format(NSLOCTEXT("ArcCore", "CheckTagsDesc", "Check Tags (Required: {0}, Blocked: {1})"), FText::AsNumber(RequiredNum), FText::AsNumber(BlockedNum));
			}
		}
	}
	return NSLOCTEXT("ArcCore", "CheckTagsDescDefault", "Check Gameplay Tags");
}
#endif
