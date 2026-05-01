// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_Cone.h"
#include "NavigationSystem.h"

void FArcTQSGenerator_Cone::GenerateItemsAroundLocation(
	const FVector& CenterLocation,
	const FArcTQSQueryContext& QueryContext,
	TArray<FArcTQSTargetItem>& OutItems) const
{
	const int32 PointsPerLine = FMath::Max(1, FMath::FloorToInt(MaxLength / PointSpacing));
	const int32 NumLines = FMath::Max(1, FMath::FloorToInt((HalfAngleDegrees * 2.0f) / DegreesPerLine)) + 1;
	const int32 EstimatedPoints = NumLines * PointsPerLine;

	OutItems.Reserve(OutItems.Num() + EstimatedPoints);

	UNavigationSystemV1* NavSys = bProjectToNavMesh
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryContext.World.Get())
		: nullptr;

	// Use querier forward as cone direction, flattened to XY plane
	FVector Forward = QueryContext.QuerierForward;
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	const float StartAngle = -HalfAngleDegrees;
	const float EndAngle = HalfAngleDegrees;

	for (float AngleDeg = StartAngle; AngleDeg <= EndAngle + KINDA_SMALL_NUMBER; AngleDeg += DegreesPerLine)
	{
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);
		const FVector LineDirection = Forward.RotateAngleAxisRad(AngleRad, FVector::UpVector);

		for (int32 PointIdx = 1; PointIdx <= PointsPerLine; ++PointIdx)
		{
			const float Distance = PointIdx * PointSpacing;
			FVector PointLocation = CenterLocation + LineDirection * Distance;

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
					continue;
				}
			}

			FArcTQSTargetItem Item;
			Item.TargetType = EArcTQSTargetType::Location;
			Item.Location = PointLocation;
			OutItems.Add(MoveTemp(Item));
		}
	}
}
