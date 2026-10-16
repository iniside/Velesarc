// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_Circle.h"
#include "NavigationSystem.h"

void FArcTQSGenerator_Circle::GenerateItems(
	const FArcTQSQueryContext& QueryContext,
	TArray<FArcTQSTargetItem>& OutItems) const
{
	const int32 PointsPerRing = FMath::Max(1, FMath::FloorToInt(ArcDegrees / DegreesPerPoint)) + 1;
	const int32 EstimatedPoints = NumRings * PointsPerRing;

	OutItems.Reserve(OutItems.Num() + EstimatedPoints);

	UNavigationSystemV1* NavSys = bProjectToNavMesh && !EnumHasAnyFlags(QueryContext.UnavailableRequirements, EArcTQSWorldRequirements::Navigation)
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryContext.World.Get())
		: nullptr;

	// Use Forward as the arc center direction (0 degrees), flattened to XY
	FVector ArcForward = Forward;
	ArcForward.Z = 0.0f;
	if (!ArcForward.Normalize())
	{
		ArcForward = FVector::ForwardVector;
	}

	const float HalfArc = ArcDegrees * 0.5f;
	const float StartAngle = -HalfArc;
	// For a full 360, don't duplicate the last point on top of the first
	const bool bFullCircle = ArcDegrees >= 360.0f - KINDA_SMALL_NUMBER;
	const float EndAngle = bFullCircle ? (HalfArc - DegreesPerPoint + KINDA_SMALL_NUMBER) : HalfArc;

	for (int32 RingIdx = 0; RingIdx < NumRings; ++RingIdx)
	{
		const float Radius = (RingIdx + 1) * DistanceBetweenRings;

		for (float AngleDeg = StartAngle; AngleDeg <= EndAngle + KINDA_SMALL_NUMBER; AngleDeg += DegreesPerPoint)
		{
			const float AngleRad = FMath::DegreesToRadians(AngleDeg);
			const FVector Direction = ArcForward.RotateAngleAxisRad(AngleRad, FVector::UpVector);
			FVector PointLocation = Center + Direction * Radius;

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
