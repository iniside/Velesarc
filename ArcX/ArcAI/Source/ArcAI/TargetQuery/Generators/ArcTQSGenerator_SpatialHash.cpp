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

	for (const FVector& CenterLocation : QueryContext.ContextLocations)
	{
		TArray<FArcMassEntityInfo> Entities = SpatialHash->QueryEntitiesInRadius(CenterLocation, Radius);

		OutItems.Reserve(OutItems.Num() + Entities.Num());
		for (const FArcMassEntityInfo& Info : Entities)
		{
			// Skip querier
			if (Info.Entity == QueryContext.QuerierEntity)
			{
				continue;
			}

			// Skip already-added entities (from overlapping context radii)
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
			OutItems.Add(MoveTemp(Item));
		}
	}
}
