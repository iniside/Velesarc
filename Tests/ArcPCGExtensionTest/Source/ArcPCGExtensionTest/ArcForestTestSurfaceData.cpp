// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcForestTestSurfaceData.h"

#include "PCGPoint.h"
#include "Helpers/PCGHelpers.h"

bool UArcForestTestSurfaceData::SamplePoint(
	const FTransform& InTransform,
	const FBox& InBounds,
	FPCGPoint& OutPoint,
	UPCGMetadata* OutMetadata) const
{
	const FVector InPosition = InTransform.GetLocation();

	// XY-only containment: the query Z (execution-bounds top face) is deliberately ignored so a
	// top-face projection still lands on the surface. Off-footprint XY reports "not on surface".
	// FootprintOverride lets a test decouple this check from GetBounds() — see its declaration.
	const FBox Footprint = FootprintOverride.IsValid != 0 ? FootprintOverride : GetBounds();
	if (!PCGHelpers::IsInsideBoundsXY(Footprint, InPosition))
	{
		return false;
	}

	FVector ProjectedLocation = InPosition;
	if (!bHonorQueryZ)
	{
		// An analytic height function shapes the surface so the derived morphology fields (slope /
		// exposure / elevation) carry real spatial structure; unbound keeps the flat legacy behaviour.
		ProjectedLocation.Z = HeightFunction
			? static_cast<double>(HeightFunction(static_cast<float>(InPosition.X), static_cast<float>(InPosition.Y)))
			: static_cast<double>(FlatSurfaceZ);
	}
	// else: report exactly the Z the caller queried at — ProjectedLocation.Z already equals it.

	OutPoint.Transform = FTransform(ProjectedLocation);
	OutPoint.SetLocalBounds(InBounds);
	OutPoint.Density = 1.0f;
	return true;
}
