// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/Sources/ArcExtraCaptureSource_TargetEntity.h"

#include "Effects/ArcEffectContext.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/EntityManager.h"

namespace ArcExtraCaptureSource_TargetEntity_Internal
{
	static bool ResolveEntityAttribute(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcAttributeRef& Attribute,
		float& OutValue)
	{
		if (!Attribute.IsValid())
		{
			return false;
		}

		if (!EntityManager.IsEntityValid(Entity))
		{
			return false;
		}

		const FArcAttribute* AttributePtr = ArcAttributes::ResolveAttribute(EntityManager, Entity, Attribute);
		if (!AttributePtr)
		{
			return false;
		}

		OutValue = AttributePtr->GetCurrentValue();
		return true;
	}
}

bool FArcExtraCaptureSource_TargetEntity::ResolveValue(
	const FArcEffectContext& Context,
	FMassEntityManager& EntityManager,
	const FArcEvaluateParameters& /*EvalParams*/,
	float& OutValue) const
{
	return ArcExtraCaptureSource_TargetEntity_Internal::ResolveEntityAttribute(
		EntityManager, Context.TargetEntity, Attribute, OutValue);
}
