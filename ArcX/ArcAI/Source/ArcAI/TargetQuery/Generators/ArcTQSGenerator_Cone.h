// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "ArcTQSGenerator_Cone.generated.h"

/**
 * Generator that creates a cone of Location-type items radiating from each context location
 * in the querier's forward direction.
 *
 * Lines radiate outward from the center, spaced by DegreesPerLine, each filled with points
 * at PointSpacing intervals up to MaxLength.
 */
USTRUCT(DisplayName = "Cone Points")
struct ARCAI_API FArcTQSGenerator_Cone : public FArcTQSGenerator
{
	GENERATED_BODY()

	// Total cone angle in degrees (half-angle on each side of forward)
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 1.0, ClampMax = 360.0))
	float HalfAngleDegrees = 45.0f;

	// Maximum length of the cone from center
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 10.0))
	float MaxLength = 2000.0f;

	// Distance between points along each line
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 10.0))
	float PointSpacing = 200.0f;

	// Angular distance in degrees between radial lines
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 1.0, ClampMax = 360.0))
	float DegreesPerLine = 15.0f;

	// Whether to project points onto navigation mesh
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bProjectToNavMesh = false;

	// Vertical extent for navmesh projection
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "bProjectToNavMesh"))
	float ProjectionExtent = 500.0f;

	virtual void GenerateItemsAroundLocation(
		const FVector& CenterLocation,
		const FArcTQSQueryContext& QueryContext,
		TArray<FArcTQSTargetItem>& OutItems) const override;
};
