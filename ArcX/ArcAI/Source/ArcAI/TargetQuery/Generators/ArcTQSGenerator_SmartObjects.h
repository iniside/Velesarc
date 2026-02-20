// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "SmartObjectRequestTypes.h"
#include "ArcTQSGenerator_SmartObjects.generated.h"

/**
 * Generator that produces one target item per smart object slot found around each context location.
 * Unlike the SmartObjects context provider (which outputs unique SO locations for other generators
 * to run around), this generator treats each slot as a scoreable target item.
 *
 * Deduplicates slots across overlapping context locations. Each slot is assigned the ContextIndex
 * of the first context location that found it.
 */
USTRUCT(DisplayName = "Smart Object Slots")
struct ARCAI_API FArcTQSGenerator_SmartObjects : public FArcTQSGenerator
{
	GENERATED_BODY()

	// Half-extent of the query box around each context location
	UPROPERTY(EditAnywhere, Category = "Generator", meta = (ClampMin = 0.0))
	float SearchRadius = 5000.0f;

	// Vertical half-extent of the query box
	UPROPERTY(EditAnywhere, Category = "Generator", meta = (ClampMin = 0.0))
	float VerticalExtent = 500.0f;

	// Optional filter for smart object activity tags
	UPROPERTY(EditAnywhere, Category = "Generator")
	FSmartObjectRequestFilter SmartObjectRequestFilter;

	// Whether to include already claimed slots
	UPROPERTY(EditAnywhere, Category = "Generator")
	bool bOnlyClaimable = true;

	// Overrides GenerateItems directly to deduplicate slots across context locations
	virtual void GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const override;
};
