// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSContextProvider.h"
#include "ArcTQSContextProvider_ActorsInRange.generated.h"

/**
 * Context provider that finds actors within range using a sphere overlap query.
 * Each actor's location becomes a context point that generators run around.
 *
 * Filters actors by optional class and collision channel.
 *
 * Example: find all patrol point actors near the querier, then generate
 * entities around each one and score them.
 */
USTRUCT(DisplayName = "Actors In Range")
struct ARCAI_API FArcTQSContextProvider_ActorsInRange : public FArcTQSContextProvider
{
	GENERATED_BODY()

	// Search radius around querier location
	UPROPERTY(EditAnywhere, Category = "Context", meta = (ClampMin = 0.0))
	float Radius = 5000.0f;

	// Collision channel used for the overlap query
	UPROPERTY(EditAnywhere, Category = "Context")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Pawn;

	// Optional: only include actors of this class (and subclasses). Null = any class.
	UPROPERTY(EditAnywhere, Category = "Context")
	TSubclassOf<AActor> ActorClassFilter;

	// Whether to exclude the querier actor from results
	UPROPERTY(EditAnywhere, Category = "Context")
	bool bExcludeQuerier = true;

	virtual void GenerateContextLocations(
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const override;
};
