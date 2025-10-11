#pragma once
#include "ArcSmartObjectPlanConditionEvaluator.h"
#include "GameplayTagContainer.h"
#include "MassEntityElementTypes.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcMassGoalPlanInfoSharedFragment.generated.h"

USTRUCT()
struct FArcMassGoalPlanInfoSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Provides;

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Requires;
	
	UPROPERTY(EditAnywhere)
	TArray<TInstancedStruct<FArcSmartObjectPlanConditionEvaluator>> CustomConditions;
};