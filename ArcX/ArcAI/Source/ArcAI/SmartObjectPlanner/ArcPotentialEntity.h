#pragma once
#include "GameplayTagContainer.h"
#include "MassEntityHandle.h"
#include "MassSmartObjectRequest.h"

#include "ArcPotentialEntity.generated.h"

USTRUCT()
struct FArcPotentialEntity
{
	GENERATED_BODY()

	FMassEntityHandle RequestingEntity;
	FMassEntityHandle EntityHandle;
	bool bUsedInPlan = false;
	FVector Location;
	FGameplayTagContainer Provides;
	FGameplayTagContainer Requires;
	bool bProcessed = false;
	TArray<FConstStructView> CustomConditions;
	FMassSmartObjectCandidateSlots FoundCandidateSlots;
};