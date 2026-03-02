// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_EntityTag.h"
#include "MassEntityManager.h"
#include "MassArchetypeTypes.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"

float FArcUtilityConsideration_EntityGameplayTags::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Target.EntityHandle.IsSet())
	{
		return 0.0f;
	}

	if (!Context.EntityManager->IsEntityValid(Target.EntityHandle))
	{
		return 0.0f;
	}

	const FArcMassGameplayTagContainerFragment* TagFragment =
		Context.EntityManager->GetFragmentDataPtr<FArcMassGameplayTagContainerFragment>(Target.EntityHandle);

	if (!TagFragment)
	{
		return 0.0f;
	}

	return TagQuery.Matches(TagFragment->Tags) ? 1.0f : 0.0f;
}

float FArcUtilityConsideration_EntityMassTag::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Target.EntityHandle.IsSet() || !MassTagType)
	{
		return 0.0f;
	}

	if (!Context.EntityManager->IsEntityValid(Target.EntityHandle))
	{
		return 0.0f;
	}

	const FMassArchetypeHandle Archetype = Context.EntityManager->GetArchetypeForEntity(Target.EntityHandle);
	if (!Archetype.IsValid())
	{
		return 0.0f;
	}

	const FMassElementBitSet& Composition = Archetype.GetCompositionBitSetChecked();
	return Composition.Contains(MassTagType) ? 1.0f : 0.0f;
}
