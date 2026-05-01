// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAggregator.h"
#include "Effects/ArcEffectTypes.h"
#include "GameplayTagContainer.h"

struct FMassEntityManager;
struct FArcAttribute;
struct FArcAttributeRef;
struct FArcDirectModifier;
struct FArcAggregatorFragment;

namespace ArcAttributes
{
	ARCMASSABILITIES_API FArcAttribute* ResolveAttribute(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
	                                                     const FArcAttributeRef& Attribute);

	ARCMASSABILITIES_API FArcAttribute* ResolveAttribute(const FArcAttributeRef& Attribute, uint8* FragmentMemory);

	ARCMASSABILITIES_API void SetValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
	                                   const FArcAttributeRef& Attribute, float NewValue);

	ARCMASSABILITIES_API void QueueSetValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
	                                        const FArcAttributeRef& Attribute, float NewValue);

	ARCMASSABILITIES_API float GetValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
	                                    const FArcAttributeRef& Attribute);

	ARCMASSABILITIES_API float EvaluateAttributeWithTags(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcAttributeRef& Attribute,
		float BaseValue,
		const FGameplayTagContainer& SourceTags,
		const FGameplayTagContainer& TargetTags);

	ARCMASSABILITIES_API float EvaluateAttributeWithTagsUnchecked(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcAttributeRef& Attribute,
		float BaseValue,
		const FGameplayTagContainer& SourceTags,
		const FGameplayTagContainer& TargetTags);
}

namespace ArcModifiers
{
	void ApplyInstant(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                  const FArcAttributeRef& Attribute, EArcModifierOp Op,
	                  float Magnitude);

	ARCMASSABILITIES_API void QueueInstant(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                       const FArcAttributeRef& Attribute, EArcModifierOp Op,
	                                       float Magnitude);

	ARCMASSABILITIES_API void QueueExecute(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                       const FArcAttributeRef& Attribute, EArcModifierOp Op,
	                                       float Magnitude);

	ARCMASSABILITIES_API FArcModifierHandle ApplyInfinite(FMassEntityManager& EntityManager,
	                                 FMassEntityHandle Target, FMassEntityHandle Source,
	                                 const FArcDirectModifier& Modifier);

	ARCMASSABILITIES_API void RemoveModifier(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                    FArcModifierHandle Handle);
}
