#pragma once
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "MassSmartObjectRequest.h"
#include "ArcKnowledgeTypes.h"

#include "ArcSmartObjectPlanStep.generated.h"

USTRUCT(BlueprintType)
struct FArcSmartObjectPlanStep
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Requires;
	
	UPROPERTY(EditAnywhere)
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FMassEntityHandle EntityHandle;

	UPROPERTY()
	FMassSmartObjectCandidateSlots FoundCandidateSlots;

	UPROPERTY()
	FArcKnowledgeHandle KnowledgeHandle;
};
