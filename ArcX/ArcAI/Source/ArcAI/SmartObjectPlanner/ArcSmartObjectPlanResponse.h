#pragma once
#include "ArcSmartObjectPlanContainer.h"
#include "ArcSmartObjectPlanRequestHandle.h"
#include "GameplayTagContainer.h"

#include "ArcSmartObjectPlanResponse.generated.h"

USTRUCT(BlueprintType)
struct FArcSmartObjectPlanResponse
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<FArcSmartObjectPlanContainer> Plans;

	FGameplayTagContainer AccumulatedTags;
	int32 TotalCost;

	FArcSmartObjectPlanRequestHandle Handle;
};