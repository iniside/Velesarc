// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_SpatialHash.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"
#include "Engine/World.h"

void FArcTQSGenerator_SpatialHash::GenerateItems(
	const FArcTQSQueryContext& QueryContext,
	TArray<FArcTQSTargetItem>& OutItems) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return;
	}

	UArcMassSpatialHashSubsystem* SpatialHash = World->GetSubsystem<UArcMassSpatialHashSubsystem>();
	if (!SpatialHash)
	{
		return;
	}

	// Deduplicate entities across multiple context location queries
	TSet<FMassEntityHandle> SeenEntities;

	for (int32 ContextIdx = 0; ContextIdx < QueryContext.ContextLocations.Num(); ++ContextIdx)
	{
		TArray<FArcMassEntityInfo> Entities = SpatialHash->QueryEntitiesInRadius(QueryContext.ContextLocations[ContextIdx], Radius);

		OutItems.Reserve(OutItems.Num() + Entities.Num());
		for (const FArcMassEntityInfo& Info : Entities)
		{
			// Skip querier
			if (Info.Entity == QueryContext.QuerierEntity)
			{
				continue;
			}

			// Skip already-added entities (from overlapping context radii)
			// First context to find the entity "owns" it
			bool bAlreadyInSet = false;
			SeenEntities.Add(Info.Entity, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				continue;
			}

			FArcTQSTargetItem Item;
			Item.TargetType = EArcTQSTargetType::MassEntity;
			Item.EntityHandle = Info.Entity;
			Item.Location = Info.Location;
			Item.ContextIndex = ContextIdx;
			OutItems.Add(MoveTemp(Item));
		}
	}
}
