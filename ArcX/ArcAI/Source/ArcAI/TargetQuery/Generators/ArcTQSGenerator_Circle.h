// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "ArcTQSGenerator_Circle.generated.h"

/**
 * Generator that creates concentric rings of Location-type items around each context location.
 *
 * Points are placed along arcs at regular angular intervals. Multiple rings can be generated
 * at increasing distances from the center. The arc can be a full 360 or a partial sweep.
 */
USTRUCT(DisplayName = "Circle Points")
struct ARCAI_API FArcTQSGenerator_Circle : public FArcTQSGenerator
{
	GENERATED_BODY()

	// Arc sweep in degrees (360 = full circle, 180 = semicircle, etc.)
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 1.0, ClampMax = 360.0))
	float ArcDegrees = 360.0f;

	// Angular spacing between points on each ring (in degrees)
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 1.0, ClampMax = 360.0))
	float DegreesPerPoint = 30.0f;

	// Number of concentric rings
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 1))
	int32 NumRings = 3;

	// Distance between concentric rings
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 10.0))
	float DistanceBetweenRings = 300.0f;

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
