// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_EntityTagFilter.generated.h"

/**
 * Filters Mass Entity targets by gameplay tags.
 * Reads FArcMassGameplayTagContainerFragment from the target entity.
 */
USTRUCT(DisplayName = "Entity Gameplay Tag Filter")
struct ARCAI_API FArcTQSStep_EntityTagFilter : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_EntityTagFilter()
	{
		StepType = EArcTQSStepType::Filter;
	}

	// Entity must have ALL of these tags to pass (empty = no requirement)
	UPROPERTY(EditAnywhere, Category = "Step")
	FGameplayTagContainer RequiredTags;

	// Entity must have NONE of these tags to pass (empty = no exclusion)
	UPROPERTY(EditAnywhere, Category = "Step")
	FGameplayTagContainer ExcludedTags;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
