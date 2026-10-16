// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRPGMechanics/Resources/ArcRPGResourceRate_AttributeModifier.h"

#include "Attributes/Handlers/ArcRPGCoreDerivation.h"

float FArcRPGResourceRate_AttributeModifier::GetRate(const FArcResourceContext& Context) const
{
	float AttributeValue = 0.f;
	if (!Attribute.IsValid() || !Context.ReadAttribute(Attribute, AttributeValue))
	{
		return 0.f;
	}

	const float Modifier = ArcRPGCoreDerivation::CalculateModifier(AttributeValue);
	return FMath::Max(0.f, BasePerSecond * (1.f + ModifierScale * Modifier));
}
