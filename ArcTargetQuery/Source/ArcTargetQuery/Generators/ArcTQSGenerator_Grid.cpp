// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_Grid.h"
#include "NavigationSystem.h"

void FArcTQSGenerator_Grid::GenerateItemsAroundLocation(
	const FVector& CenterLocation,
	const FArcTQSQueryContext& QueryContext,
	TArray<FArcTQSTargetItem>& OutItems) const
{
	const int32 HalfCount = FMath::FloorToInt(GridHalfExtent / GridSpacing);
	const int32 TotalPoints = (2 * HalfCount + 1) * (2 * HalfCount + 1);

	OutItems.Reserve(OutItems.Num() + TotalPoints);

	UNavigationSystemV1* NavSys = bProjectToNavMesh ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryContext.World.Get()) : nullptr;

	for (int32 X = -HalfCount; X <= HalfCount; ++X)
	{
		for (int32 Y = -HalfCount; Y <= HalfCount; ++Y)
		{
			FVector PointLocation = CenterLocation + FVector(X * GridSpacing, Y * GridSpacing, 0.0f);

			if (NavSys)
			{
				FNavLocation ProjectedLocation;
				if (NavSys->ProjectPointToNavigation(
					PointLocation, ProjectedLocation,
					FVector(0.0f, 0.0f, ProjectionExtent)))
				{
					PointLocation = ProjectedLocation.Location;
				}
				else
				{
					continue; // Skip points that don't project onto navmesh
				}
			}

			FArcTQSTargetItem Item;
			Item.TargetType = EArcTQSTargetType::Location;
			Item.Location = PointLocation;
			OutItems.Add(MoveTemp(Item));
		}
	}
}
