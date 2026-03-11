// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcAreaSlotDefinition.generated.h"

/** Configuration for automatic vacancy posting to ArcKnowledge. */
USTRUCT(BlueprintType)
struct ARCAREA_API FArcAreaVacancyConfig
{
	GENERATED_BODY()

	/** Whether to automatically post a vacancy when this slot is unoccupied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vacancy")
	bool bAutoPostVacancy = false;

	/** Tags for the vacancy knowledge entry. If empty, uses "Area.Vacancy" + area tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vacancy")
	FGameplayTagContainer VacancyTags;

	/** Relevance of the vacancy posting (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vacancy", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float VacancyRelevance = 0.5f;
};

/** Design-time definition of a single slot within an area. */
USTRUCT(BlueprintType)
struct ARCAREA_API FArcAreaSlotDefinition
{
	GENERATED_BODY()

	/** NPC capabilities must match this query to be assigned to this slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FGameplayTagQuery RequirementQuery;

	/** Vacancy posting configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FArcAreaVacancyConfig VacancyConfig;
};
