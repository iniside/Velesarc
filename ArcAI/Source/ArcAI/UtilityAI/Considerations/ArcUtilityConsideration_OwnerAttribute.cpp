// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_OwnerAttribute.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

float FArcUtilityConsideration_OwnerAttribute::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	AActor* OwnerActor = Context.QuerierActor.Get();
	if (!OwnerActor)
	{
		return 0.0f;
	}

	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor))
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
