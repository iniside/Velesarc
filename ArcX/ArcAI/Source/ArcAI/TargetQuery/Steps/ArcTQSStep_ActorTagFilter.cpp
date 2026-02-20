// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_ActorTagFilter.h"
#include "GameplayTagAssetInterface.h"

float FArcTQSStep_ActorTagFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const AActor* ResolvedActor = const_cast<FArcTQSTargetItem&>(Item).GetActor(QueryContext.EntityManager);
	if (!ResolvedActor)
	{
		return bFilterItemsWithoutActor ? 0.0f : 1.0f;
	}

	const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(ResolvedActor);
	if (!TagInterface)
	{
		return bFilterItemsWithoutActor ? 0.0f : 1.0f;
	}

	FGameplayTagContainer OwnedTags;
	TagInterface->GetOwnedGameplayTags(OwnedTags);

	// Check required tags
	if (!RequiredTags.IsEmpty() && !OwnedTags.HasAll(RequiredTags))
	{
		return 0.0f;
	}

	// Check excluded tags
	if (!ExcludedTags.IsEmpty() && OwnedTags.HasAny(ExcludedTags))
	{
		return 0.0f;
	}

	return 1.0f;
}
