#pragma once
#include "GameplayTagContainer.h"
#include "MassEntityHandle.h"
#include "MassSmartObjectRequest.h"

#include "ArcSmartObjectPlanStep.generated.h"

USTRUCT(BlueprintType)
struct FArcSmartObjectPlanStep
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Requires;
	
	UPROPERTY(EditAnywhere)
	FVector Location;

	UPROPERTY()
	FMassEntityHandle EntityHandle;

	UPROPERTY()
	FMassSmartObjectCandidateSlots FoundCandidateSlots;
};
