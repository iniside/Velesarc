// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGEC_TargetTagRequirementsFromItem.h"

#include "AbilitySystemComponent.h"
#include "ArcGameplayEffectContext.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/ArcActiveGameplayEffectForRPC.h"
#include "Items/ArcItemDefinition.h"

bool UArcGEC_TargetTagRequirementsFromItem::CanGameplayEffectApply(const FActiveGameplayEffectsContainer& ActiveGEContainer
																   , const FGameplayEffectSpec& GESpec) const
{
	FGameplayTagContainer Tags;
	ActiveGEContainer.Owner->GetOwnedGameplayTags(Tags);

	const FGameplayEffectContextHandle& CtxHandle = GESpec.GetEffectContext();
	const FArcGameplayEffectContext* Ctx = static_cast<const FArcGameplayEffectContext*>(CtxHandle.Get());

	if (!Ctx)
	{
		return true;                
	}

	const UArcItemDefinition* ItemDef = Ctx->GetSourceItem();
	if (!ItemDef)
	{
		return true;
	}
	const FArcItemFragment_GameplayEffectTargetTagRequirements* ReqTagsFragment = ItemDef->FindFragment<FArcItemFragment_GameplayEffectTargetTagRequirements>();
	if (!ReqTagsFragment)
	{
		return true;
	}

	if (ReqTagsFragment->ApplicationTagRequirements.RequirementsMet(Tags) == false)
	{
		return false;
	}

	if (!ReqTagsFragment->RemovalTagRequirements.IsEmpty() && ReqTagsFragment->RemovalTagRequirements.RequirementsMet(Tags))
	{
		return false;
	}
	
	return true;
}