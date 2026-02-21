// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSContextProvider.h"
#include "TargetQuery/Generators/ArcTQSGenerator_PerceivedEntities.h"
#include "ArcTQSContextProvider_PerceivedEntities.generated.h"

/**
 * Context provider that uses perceived entity locations as context points.
 * Reads the querier's perception result fragments (sight, hearing, or both)
 * and outputs each perceived entity's location as a context location.
 *
 * Example: generate a grid around each perceived enemy, then score those
 * positions for flanking maneuvers.
 */
USTRUCT(DisplayName = "Perceived Entities")
struct ARCAI_API FArcTQSContextProvider_PerceivedEntities : public FArcTQSContextProvider
{
	GENERATED_BODY()

	// Which perception sense(s) to read from.
	UPROPERTY(EditAnywhere, Category = "Context")
	EArcTQSPerceptionSense Sense = EArcTQSPerceptionSense::Sight;

	// If true, use LastKnownLocation from perception result instead of
	// resolving the entity's current transform.
	UPROPERTY(EditAnywhere, Category = "Context")
	bool bUseLastKnownLocation = false;

	virtual void GenerateContextLocations(
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const override;
};
