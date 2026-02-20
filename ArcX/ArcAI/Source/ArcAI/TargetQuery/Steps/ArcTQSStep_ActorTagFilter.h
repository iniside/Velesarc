// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_ActorTagFilter.generated.h"

/**
 * Filters targets by gameplay tags on their resolved actor.
 * Uses IGameplayTagAssetInterface to read tags from the actor.
 * Works with Actor, MassEntity (resolved to actor), and SmartObject (resolved to owning actor) target types.
 */
USTRUCT(DisplayName = "Actor Gameplay Tag Filter")
struct ARCAI_API FArcTQSStep_ActorTagFilter : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_ActorTagFilter()
	{
		StepType = EArcTQSStepType::Filter;
	}

	// Actor must have ALL of these tags to pass (empty = no requirement)
	UPROPERTY(EditAnywhere, Category = "Step")
	FGameplayTagContainer RequiredTags;

	// Actor must have NONE of these tags to pass (empty = no exclusion)
	UPROPERTY(EditAnywhere, Category = "Step")
	FGameplayTagContainer ExcludedTags;

	// If true, items without a resolvable actor are filtered out. If false, they pass through.
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bFilterItemsWithoutActor = false;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
