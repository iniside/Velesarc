// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSContextProvider.h"
#include "GameplayTagContainer.h"
#include "ArcTQSContextProvider_Settlements.generated.h"

/**
 * TQS Context Provider that produces settlement locations as context points.
 * Generators will then run around each settlement center.
 *
 * Useful for creating TQS queries that evaluate locations relative to
 * multiple settlements (e.g., "find the best patrol point near any settlement").
 */
USTRUCT(DisplayName = "Settlement Locations")
struct ARCSETTLEMENT_API FArcTQSContextProvider_Settlements : public FArcTQSContextProvider
{
	GENERATED_BODY()

	/** Search radius from the querier. 0 = all settlements. */
	UPROPERTY(EditAnywhere, Category = "Context", meta = (ClampMin = 0.0))
	float SearchRadius = 50000.0f;

	/** Optional: only include settlements matching these tags. */
	UPROPERTY(EditAnywhere, Category = "Context")
	FGameplayTagQuery SettlementTagFilter;

	/** Maximum number of settlement locations to produce. */
	UPROPERTY(EditAnywhere, Category = "Context", meta = (ClampMin = 1))
	int32 MaxLocations = 10;

	virtual void GenerateContextLocations(
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const override;
};
