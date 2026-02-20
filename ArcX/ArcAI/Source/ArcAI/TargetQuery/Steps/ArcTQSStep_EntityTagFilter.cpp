// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_EntityTagFilter.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"
#include "MassEntityManager.h"

float FArcTQSStep_EntityTagFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	if (Item.TargetType != EArcTQSTargetType::MassEntity || !QueryContext.EntityManager)
	{
		// Non-entity targets pass by default (filter doesn't apply)
		return 1.0f;
	}

	if (!QueryContext.EntityManager->IsEntityValid(Item.EntityHandle))
	{
		return 0.0f;
	}

	const FArcMassGameplayTagContainerFragment* TagFragment =
		QueryContext.EntityManager->GetFragmentDataPtr<FArcMassGameplayTagContainerFragment>(Item.EntityHandle);

	if (!TagFragment)
	{
		// No tag fragment — fail if we require tags, pass otherwise
		return RequiredTags.IsEmpty() ? 1.0f : 0.0f;
	}

	// Check required tags
	if (!RequiredTags.IsEmpty() && !TagFragment->Tags.HasAll(RequiredTags))
	{
		return 0.0f;
	}

	// Check excluded tags
	if (!ExcludedTags.IsEmpty() && TagFragment->Tags.HasAny(ExcludedTags))
	{
		return 0.0f;
	}

	return 1.0f;
}
