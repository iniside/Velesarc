// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TargetQuery/ArcTQSContextProvider.h"
#include "ArcTQSContextProvider_KnowledgeEntries.generated.h"

/**
 * Context provider that uses knowledge entry locations as context points.
 * Generators will run around each matching knowledge entry location.
 *
 * Example: generate a grid around each known resource location.
 */
USTRUCT(DisplayName = "Knowledge Entries")
struct ARCKNOWLEDGE_API FArcTQSContextProvider_KnowledgeEntries : public FArcTQSContextProvider
{
	GENERATED_BODY()

	/** Tag query to filter knowledge entries. */
	UPROPERTY(EditAnywhere, Category = "Context")
	FGameplayTagQuery TagQuery;

	/** Maximum search radius from querier. 0 = unlimited. */
	UPROPERTY(EditAnywhere, Category = "Context", meta = (ClampMin = 0.0))
	float MaxDistance = 0.0f;

	/** Maximum number of context locations to produce. */
	UPROPERTY(EditAnywhere, Category = "Context", meta = (ClampMin = 1))
	int32 MaxResults = 10;

	virtual void GenerateContextLocations(
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const override;
};
