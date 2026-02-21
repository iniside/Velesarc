// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSContextProvider_EntitiesInRange.h"
#include "TargetQuery/Generators/ArcTQSGeneratorSpatialHashHelper.h"
#include "Engine/World.h"

void FArcTQSContextProvider_EntitiesInRange::GenerateContextLocations(
	const FArcTQSQueryContext& QueryContext,
	TArray<FVector>& OutLocations) const
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

	const FVector Center = QueryContext.QuerierLocation;

	// Compute index keys using the same helper as the generators
	const TArray<uint32> QueryKeys = ArcTQSGeneratorSpatialHashHelper::ComputeQueryKeys(
		IndexTags, IndexFragments, IndexMassTags, CombinationIndices, bCombineAllIndices);

	TArray<FArcMassEntityInfo> Entities;
	TSet<FMassEntityHandle> SeenEntities;

	if (QueryKeys.IsEmpty())
	{
		// No index filtering — query the main grid
		SpatialHash->QuerySphere(Center, Radius, Entities);
		for (const FArcMassEntityInfo& Info : Entities)
		{
			if (Info.Entity == QueryContext.QuerierEntity)
			{
				continue;
			}

			bool bAlreadyInSet = false;
			SeenEntities.Add(Info.Entity, &bAlreadyInSet);
			if (!bAlreadyInSet)
			{
				OutLocations.Add(Info.Location);
			}
		}
	}
	else
	{
		// Query each indexed grid and deduplicate
		for (const uint32 Key : QueryKeys)
		{
			Entities.Reset();
			SpatialHash->QuerySphere(Key, Center, Radius, Entities);

			for (const FArcMassEntityInfo& Info : Entities)
			{
				if (Info.Entity == QueryContext.QuerierEntity)
				{
					continue;
				}

				bool bAlreadyInSet = false;
				SeenEntities.Add(Info.Entity, &bAlreadyInSet);
				if (!bAlreadyInSet)
				{
					OutLocations.Add(Info.Location);
				}
			}
		}
	}
}
