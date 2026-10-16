// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/Sources/ArcExtraCaptureSource_SourceEntity.h"

#include "Effects/ArcEffectContext.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/EntityManager.h"

namespace ArcExtraCaptureSource_SourceEntity_Internal
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

bool FArcExtraCaptureSource_SourceEntity::ResolveValue(
	const FArcEffectContext& Context,
	FMassEntityManager& EntityManager,
	const FArcEvaluateParameters& /*EvalParams*/,
	float& OutValue) const
{
	return ArcExtraCaptureSource_SourceEntity_Internal::ResolveEntityAttribute(
		EntityManager, Context.SourceEntity, Attribute, OutValue);
}
