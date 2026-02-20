// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Attribute.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

float FArcTQSStep_Attribute::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	if (!Attribute.IsValid())
	{
		return bFilterItemsWithoutAttribute ? 0.0f : DefaultScore;
	}

	AActor* TargetActor = const_cast<FArcTQSTargetItem&>(Item).GetActor(QueryContext.EntityManager);
	if (!TargetActor)
	{
		return bFilterItemsWithoutAttribute ? 0.0f : DefaultScore;
	}

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(TargetActor);
	if (!ASI)
	{
		return bFilterItemsWithoutAttribute ? 0.0f : DefaultScore;
	}

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC)
	{
		return bFilterItemsWithoutAttribute ? 0.0f : DefaultScore;
	}

	const float AttributeValue = ASC->GetNumericAttribute(Attribute);

	float MaxValue;
	if (bUseMaxAttribute && MaxAttribute.IsValid())
	{
		MaxValue = ASC->GetNumericAttribute(MaxAttribute);
		if (MaxValue <= 0.0f)
		{
			return bFilterItemsWithoutAttribute ? 0.0f : DefaultScore;
		}
	}
	else
	{
		MaxValue = ManualMaxValue;
	}

	const float NormalizedValue = FMath::Clamp(AttributeValue / MaxValue, 0.0f, 1.0f);
	return ResponseCurve.Evaluate(NormalizedValue);
}
