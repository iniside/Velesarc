// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TargetQuery/ArcTQSContextProvider.h"
#include "ArcTQSContextProvider_Areas.generated.h"

/**
 * Context provider that uses registered area locations as context points.
 * Generators will run around each matching area location.
 */
USTRUCT(DisplayName = "Areas")
struct ARCAREA_API FArcTQSContextProvider_Areas : public FArcTQSContextProvider
{
	GENERATED_BODY()

	/** Filter areas by their gameplay tags. */
	UPROPERTY(EditAnywhere, Category = "Context")
	FGameplayTagQuery AreaTagQuery;

	/** Maximum distance from querier. 0 = unlimited. */
	UPROPERTY(EditAnywhere, Category = "Context", meta = (ClampMin = 0.0))
	float MaxDistance = 0.0f;

	virtual void GenerateContextLocations(
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const override;
};
