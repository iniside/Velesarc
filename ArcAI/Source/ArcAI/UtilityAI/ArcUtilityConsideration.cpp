// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/ArcUtilityConsideration.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "MassEntityManager.h"

float FArcUtilityConsideration_Distance::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	const FVector TargetLoc = Target.GetLocation(Context.EntityManager);
	const float Dist = FVector::Dist(Context.QuerierLocation, TargetLoc);
	return FMath::Clamp(1.0f - (Dist / MaxDistance), 0.0f, 1.0f);
}

float FArcUtilityConsideration_Constant::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	return Value;
}

float FArcUtilityConsideration_GameplayTag::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	AActor* TargetActor = Target.GetActor(Context.EntityManager);
	if (!TargetActor)
	{
		return 0.0f;
	}

	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
	{
		FGameplayTagContainer Tags;
		ASC->GetOwnedGameplayTags(Tags);
		return TagQuery.Matches(Tags) ? 1.0f : 0.0f;
	}

	return 0.0f;
}

float FArcUtilityConsideration_Attribute::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	AActor* TargetActor = Target.GetActor(Context.EntityManager);
	if (!TargetActor)
	{
		return 0.0f;
	}

	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
	{
		if (ASC->HasAttributeSetForAttribute(Attribute))
		{
			const float Val = ASC->GetNumericAttribute(Attribute);
			const float Range = MaxValue - MinValue;
			if (Range > KINDA_SMALL_NUMBER)
			{
				return FMath::Clamp((Val - MinValue) / Range, 0.0f, 1.0f);
			}
		}
	}

	return 0.0f;
}
