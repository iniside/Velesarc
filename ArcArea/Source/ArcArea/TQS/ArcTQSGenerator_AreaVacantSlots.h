// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "ArcTQSGenerator_AreaVacantSlots.generated.h"

/**
 * TQS Generator that produces one item per vacant slot across matching areas.
 * Items are Location-type at the area's position, with AreaHandle + SlotIndex in ItemData.
 */
USTRUCT(DisplayName = "Area Vacant Slots")
struct ARCAREA_API FArcTQSGenerator_AreaVacantSlots : public FArcTQSGenerator
{
	GENERATED_BODY()

	/** Filter areas by their gameplay tags. */
	UPROPERTY(EditAnywhere, Category = "Generator")
	FGameplayTagQuery AreaTagQuery;

	/** Maximum distance from querier. 0 = unlimited. */
	UPROPERTY(EditAnywhere, Category = "Generator", meta = (ClampMin = 0.0))
	float MaxDistance = 0.0f;

	virtual void GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const override;
	virtual void GetOutputSchema(TArray<FPropertyBagPropertyDesc>& OutDescs) const override;
};
