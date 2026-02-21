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
	FVector Location;
	FGameplayTagContainer Provides;
	FGameplayTagContainer Requires;
	TArray<FConstStructView> CustomConditions;
	FMassSmartObjectCandidateSlots FoundCandidateSlots;
};