#pragma once
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "MassSmartObjectRequest.h"
#include "ArcKnowledgeTypes.h"

#include "ArcPotentialEntity.generated.h"

USTRUCT()
struct FArcPotentialEntity
{
	GENERATED_BODY()

	FMassEntityHandle EntityHandle;
	FVector Location;
	FGameplayTagContainer Provides;
	FGameplayTagContainer Requires;
	TArray<FConstStructView> CustomConditions;
	FMassSmartObjectCandidateSlots FoundCandidateSlots;
	FArcKnowledgeHandle KnowledgeHandle;
};