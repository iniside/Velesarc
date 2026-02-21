// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPerception.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Sense_Sight, "AI.Perception.Sense.Sight");
UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Sense_Hearing, "AI.Perception.Sense.Hearing");

UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Stimuli_Sight, "AI.Perception.Stimuli.Sight");
UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Stimuli_Hearing, "AI.Perception.Stimuli.Hearing");

//----------------------------------------------------------------------
// Shared Perception Utilities
//----------------------------------------------------------------------

bool ArcPerception::PassesFilters(
	const FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const FArcPerceptionSenseConfigFragment& Config)
{
	if (!Config.RequiredTags.IsEmpty() || !Config.IgnoredTags.IsEmpty())
	{
		const FArcMassGameplayTagContainerFragment* TagFragment = EntityManager.GetFragmentDataPtr<FArcMassGameplayTagContainerFragment>(Entity);

		if (!TagFragment)
		{
			return Config.RequiredTags.IsEmpty();
		}

		const FGameplayTagContainer& EntityTags = TagFragment->Tags;

		if (!Config.IgnoredTags.IsEmpty() && EntityTags.HasAny(Config.IgnoredTags))
		{
			return false;
		}

		if (!Config.RequiredTags.IsEmpty() && !EntityTags.HasAll(Config.RequiredTags))
		{
			return false;
		}
	}

	return true;
}
