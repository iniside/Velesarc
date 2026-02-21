// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanGameplayTagCondition.h"

#include "GameplayTagAssetInterface.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "Engine/World.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

bool FArcSmartObjectPlanGameplayTagCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FMassEntityManager& EntityManager) const
{
	const UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return false;
	}

	const UMassActorSubsystem* ActorSubsystem = World->GetSubsystem<UMassActorSubsystem>();
	if (!ActorSubsystem)
	{
		return false;
	}

	const AActor* Actor = ActorSubsystem->GetActorFromHandle(Entity.EntityHandle);
	if (!Actor)
	{
		return RequireTags.IsEmpty();
	}

	const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Actor);
	if (!TagInterface)
	{
		return RequireTags.IsEmpty();
	}

	FGameplayTagContainer OwnedTags;
	TagInterface->GetOwnedGameplayTags(OwnedTags);

	if (!RequireTags.IsEmpty() && !OwnedTags.HasAll(RequireTags))
	{
		return false;
	}

	if (!IgnoreTags.IsEmpty() && OwnedTags.HasAny(IgnoreTags))
	{
		return false;
	}

	return true;
}
