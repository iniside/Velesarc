// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_SpatialHashSphere.h"
#include "ArcTQSGeneratorSpatialHashHelper.h"
#include "Engine/World.h"

void FArcTQSGenerator_SpatialHashSphere::GenerateItems(
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

	const TArray<uint32> QueryKeys = ArcTQSGeneratorSpatialHashHelper::ComputeQueryKeys(
		IndexTags, IndexFragments, IndexMassTags, CombinationIndices, bCombineAllIndices);

	TSet<FMassEntityHandle> SeenEntities;
	TArray<FArcMassEntityInfo> Entities;

	for (int32 ContextIdx = 0; ContextIdx < QueryContext.ContextLocations.Num(); ++ContextIdx)
	{
		const FVector& Center = QueryContext.ContextLocations[ContextIdx];

		if (QueryKeys.IsEmpty())
		{
			// No index keys — query main grid
			Entities.Reset();
			SpatialHash->QuerySphere(Center, Radius, Entities);
			ArcTQSGeneratorSpatialHashHelper::AddEntities(Entities, QueryContext.QuerierEntity, ContextIdx, SeenEntities, OutItems);
		}
		else
		{
			for (const uint32 Key : QueryKeys)
			{
				Entities.Reset();
				SpatialHash->QuerySphere(Key, Center, Radius, Entities);
				ArcTQSGeneratorSpatialHashHelper::AddEntities(Entities, QueryContext.QuerierEntity, ContextIdx, SeenEntities, OutItems);
			}
		}
	}
}
