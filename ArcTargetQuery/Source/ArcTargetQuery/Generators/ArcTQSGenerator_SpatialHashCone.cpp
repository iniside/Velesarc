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
	const FVector& Direction = Forward;

	TSet<FMassEntityHandle> SeenEntities;
	TArray<FArcMassEntityInfo> Entities;

	if (QueryKeys.IsEmpty())
	{
		Entities.Reset();
		SpatialHash->QueryCone(Origin, Direction, Length, HalfAngleRadians, Entities);
		ArcTQSGeneratorSpatialHashHelper::AddEntities(Entities, ExcludedEntity, SeenEntities, OutItems);
	}
	else
	{
		for (const uint32 Key : QueryKeys)
		{
			Entities.Reset();
			SpatialHash->QueryCone(Key, Origin, Direction, Length, HalfAngleRadians, Entities);
			ArcTQSGeneratorSpatialHashHelper::AddEntities(Entities, ExcludedEntity, SeenEntities, OutItems);
		}
	}
}
