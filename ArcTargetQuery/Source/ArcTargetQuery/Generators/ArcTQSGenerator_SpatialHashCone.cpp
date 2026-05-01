// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_SpatialHashCone.h"
#include "ArcTQSGeneratorSpatialHashHelper.h"
#include "Engine/World.h"

void FArcTQSGenerator_SpatialHashCone::GenerateItems(
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

	const float HalfAngleRadians = FMath::DegreesToRadians(HalfAngleDegrees);
	const FVector& Direction = QueryContext.QuerierForward;

	TSet<FMassEntityHandle> SeenEntities;
	TArray<FArcMassEntityInfo> Entities;

	for (int32 ContextIdx = 0; ContextIdx < QueryContext.ContextLocations.Num(); ++ContextIdx)
	{
		const FVector& Origin = QueryContext.ContextLocations[ContextIdx];

		if (QueryKeys.IsEmpty())
		{
			Entities.Reset();
			SpatialHash->QueryCone(Origin, Direction, Length, HalfAngleRadians, Entities);
			ArcTQSGeneratorSpatialHashHelper::AddEntities(Entities, QueryContext.QuerierEntity, ContextIdx, SeenEntities, OutItems);
		}
		else
		{
			for (const uint32 Key : QueryKeys)
			{
				Entities.Reset();
				SpatialHash->QueryCone(Key, Origin, Direction, Length, HalfAngleRadians, Entities);
				ArcTQSGeneratorSpatialHashHelper::AddEntities(Entities, QueryContext.QuerierEntity, ContextIdx, SeenEntities, OutItems);
			}
		}
	}
}
