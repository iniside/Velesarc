// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_EntityTag.h"
#include "Mass/EntityManager.h"
#include "Mass/ArchetypeTypes.h"
#include "Fragments/ArcOwnedTagsFragment.h"

FArcUtilityConsiderationResult FArcUtilityConsideration_EntityGameplayTags::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Target.EntityHandle.IsSet())
	{
		return FArcUtilityConsiderationResult::Accept(0.0f);
	}

	if (!Context.EntityManager->IsEntityValid(Target.EntityHandle))
	{
		return FArcUtilityConsiderationResult::Accept(0.0f);
	}

	const FArcOwnedTagsFragment* TagFragment =
		Context.EntityManager->GetFragmentDataPtr<FArcOwnedTagsFragment>(Target.EntityHandle);

	if (!TagFragment)
	{
		return FArcUtilityConsiderationResult::Accept(0.0f);
	}

	return FArcUtilityConsiderationResult::Accept(TagQuery.Matches(TagFragment->GetTags().GetTagContainer()) ? 1.0f : 0.0f);
}

FArcUtilityConsiderationResult FArcUtilityConsideration_EntityMassTag::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Target.EntityHandle.IsSet() || !MassTagType)
	{
		return FArcUtilityConsiderationResult::Accept(0.0f);
	}

	if (!Context.EntityManager->IsEntityValid(Target.EntityHandle))
	{
		return FArcUtilityConsiderationResult::Accept(0.0f);
	}

	const FMassArchetypeHandle Archetype = Context.EntityManager->GetArchetypeForEntity(Target.EntityHandle);
	if (!Archetype.IsValid())
	{
		return FArcUtilityConsiderationResult::Accept(0.0f);
	}

	const FMassElementBitSet& Composition = Archetype.GetCompositionBitSetChecked();
	return FArcUtilityConsiderationResult::Accept(Composition.Contains(MassTagType) ? 1.0f : 0.0f);
}
