// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSContextProvider_Areas.h"
#include "ArcAreaSubsystem.h"
#include "Engine/World.h"

void FArcTQSContextProvider_Areas::GenerateContextLocations(
	const FArcTQSQueryContext& QueryContext,
	TArray<FVector>& OutLocations) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return;
	}

	const UArcAreaSubsystem* Subsystem = World->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	const float MaxDistSq = MaxDistance > 0.0f ? MaxDistance * MaxDistance : 0.0f;

	const TMap<FArcAreaHandle, FArcAreaData>& AllAreas = Subsystem->GetAllAreas();
	OutLocations.Reserve(OutLocations.Num() + AllAreas.Num());

	for (const auto& [Handle, AreaData] : AllAreas)
	{
		if (!AreaTagQuery.IsEmpty() && !AreaTagQuery.Matches(AreaData.AreaTags))
		{
			continue;
		}

		if (MaxDistSq > 0.0f)
		{
			const float DistSq = FVector::DistSquared(QueryContext.QuerierLocation, AreaData.Location);
			if (DistSq > MaxDistSq)
			{
				continue;
			}
		}

		OutLocations.Add(AreaData.Location);
	}
}
