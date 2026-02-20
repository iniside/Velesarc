// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "ArcTQSGenerator_Grid.generated.h"

/**
 * Generator that creates a grid of Location-type items around each context location.
 * Useful for finding positions (cover spots, patrol points, etc.)
 */
USTRUCT(DisplayName = "Grid Points")
struct ARCAI_API FArcTQSGenerator_Grid : public FArcTQSGenerator
{
	GENERATED_BODY()

	// Half-extent of the grid from center location
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 0.0))
	float GridHalfExtent = 1000.0f;

	// Spacing between grid points
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 10.0))
	float GridSpacing = 200.0f;

	// Whether to project points onto navigation mesh
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bProjectToNavMesh = false;

	// Vertical offset for projected points
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "bProjectToNavMesh"))
	float ProjectionExtent = 500.0f;

	virtual void GenerateItemsAroundLocation(
		const FVector& CenterLocation,
		const FArcTQSQueryContext& QueryContext,
		TArray<FArcTQSTargetItem>& OutItems) const override;
};
