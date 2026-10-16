// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSGenerator.h"
#include "SmartObjectRequestTypes.h"
#include "ArcTQSGenerator_SmartObjects.generated.h"

/**
 * Generator that produces one target item per smart object slot found around Center.
 */
USTRUCT(DisplayName = "Smart Object Slots")
struct ARCTARGETQUERY_API FArcTQSGenerator_SmartObjects : public FArcTQSGenerator
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Generator", meta = (ArcTQSRequiredBinding))
	FVector Center = FVector::ZeroVector;

	// Half-extent of the query box around Center
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

	virtual void GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const override;
	virtual EArcTQSWorldRequirements GetWorldRequirements() const override { return EArcTQSWorldRequirements::SmartObjects; }
};
