// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSContextProvider.generated.h"

/**
 * Base struct for TQS context providers. Context providers produce the array of
 * ContextLocations that generators run around.
 *
 * Runs once before the generator phase. The output locations are appended to
 * any explicit ContextLocations already present in the query context.
 *
 * Examples:
 * - Smart objects within radius → generate grid around each smart object
 * - Patrol waypoints → gather entities near each waypoint
 * - Known enemy positions → score locations relative to threats
 */
USTRUCT(BlueprintType)
struct ARCAI_API FArcTQSContextProvider
{
	GENERATED_BODY()

	virtual ~FArcTQSContextProvider() = default;

	/**
	 * Produce context locations. Called once before the generator runs.
	 * Append locations to OutLocations — they will be merged with any
	 * explicit ContextLocations already set on the query context.
	 *
	 * @param QueryContext - provides querier entity, location, world, entity manager
	 * @param OutLocations - array to append generated context locations to
	 */
	virtual void GenerateContextLocations(
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const {}
};
