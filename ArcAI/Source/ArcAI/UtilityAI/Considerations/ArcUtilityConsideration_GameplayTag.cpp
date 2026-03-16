// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_GameplayTag.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "MassEntityManager.h"

static UAbilitySystemComponent* GetTargetASC(const FArcUtilityTarget& Target, const FArcUtilityContext& Context)
{
	AActor* TargetActor = Target.GetActor(Context.EntityManager);
	if (!TargetActor)
	{
		return nullptr;
	}
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
}

float FArcUtilityConsideration_GameplayTagMatch::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Tag.IsValid())
	{
		return 0.0f;
	}

	UAbilitySystemComponent* ASC = GetTargetASC(Target, Context);
	if (!ASC)
	{
		return 0.0f;
	}

	FGameplayTagContainer OwnedTags;
	ASC->GetOwnedGameplayTags(OwnedTags);

	if (bExactMatch)
	{
		return OwnedTags.HasTagExact(Tag) ? 1.0f : 0.0f;
	}

	return OwnedTags.HasTag(Tag) ? 1.0f : 0.0f;
}

float FArcUtilityConsideration_GameplayTagContainer::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (Tags.IsEmpty())
	{
		return 0.0f;
	}

	UAbilitySystemComponent* ASC = GetTargetASC(Target, Context);
	if (!ASC)
	{
		return 0.0f;
	}

	FGameplayTagContainer OwnedTags;
	ASC->GetOwnedGameplayTags(OwnedTags);

	bool bResult = false;
	if (MatchType == EGameplayContainerMatchType::Any)
	{
		bResult = bExactMatch ? OwnedTags.HasAnyExact(Tags) : OwnedTags.HasAny(Tags);
	}
	else // All
	{
		bResult = bExactMatch ? OwnedTags.HasAllExact(Tags) : OwnedTags.HasAll(Tags);
	}

	return bResult ? 1.0f : 0.0f;
}
