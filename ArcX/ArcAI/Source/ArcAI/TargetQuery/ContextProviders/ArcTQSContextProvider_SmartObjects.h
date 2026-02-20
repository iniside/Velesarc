// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSContextProvider.h"
#include "GameplayTagContainer.h"
#include "ArcTQSContextProvider_SmartObjects.generated.h"

/**
 * Context provider that finds smart object slot locations within a radius of the querier.
 * The resulting locations become the context points that generators run around.
 *
 * Example use case: generate a grid around each smart object near the querier,
 * then score those grid points by distance/visibility/etc.
 */
USTRUCT(DisplayName = "Smart Objects")
struct ARCAI_API FArcTQSContextProvider_SmartObjects : public FArcTQSContextProvider
{
	GENERATED_BODY()

	// Search radius around querier location
	UPROPERTY(EditAnywhere, Category = "Context", meta = (ClampMin = 0.0))
	float SearchRadius = 5000.0f;

	// Vertical extent of the search box (half-extent)
	UPROPERTY(EditAnywhere, Category = "Context", meta = (ClampMin = 0.0))
	float VerticalExtent = 500.0f;

	// Optional: only include smart objects matching these activity tags
	UPROPERTY(EditAnywhere, Category = "Context")
	FGameplayTagQuery ActivityRequirements;

	// Whether to include already claimed slots
	UPROPERTY(EditAnywhere, Category = "Context")
	bool bIncludeClaimedSlots = false;

	virtual void GenerateContextLocations(
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const override;
};
