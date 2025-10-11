#pragma once
#include "ArcSmartObjectPlanStep.h"

#include "ArcSmartObjectPlanContainer.generated.h"

USTRUCT(BlueprintType)
struct FArcSmartObjectPlanContainer
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<FArcSmartObjectPlanStep> Items;
};