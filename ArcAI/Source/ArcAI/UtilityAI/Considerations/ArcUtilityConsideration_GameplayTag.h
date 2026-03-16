// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcUtilityConsideration_GameplayTag.generated.h"

/**
 * Lightweight single-tag check on the target actor's AbilitySystemComponent.
 * Returns 1.0 if the actor has the tag, 0.0 otherwise.
 * Use this instead of the full TagQuery consideration when you only need a single tag match.
 */
USTRUCT(BlueprintType, DisplayName = "Gameplay Tag Match Consideration")
struct ARCAI_API FArcUtilityConsideration_GameplayTagMatch : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration")
	FGameplayTag Tag;

	/** If true, require exact match. If false, parent tags also match. */
	UPROPERTY(EditAnywhere, Category = "Consideration")
	bool bExactMatch = false;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Checks the target actor's ASC tags against a tag container with configurable match type.
 * Lighter than full FGameplayTagQuery for simple Any/All matching.
 */
USTRUCT(BlueprintType, DisplayName = "Gameplay Tag Container Consideration")
struct ARCAI_API FArcUtilityConsideration_GameplayTagContainer : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration")
	FGameplayTagContainer Tags;

	/** Any = at least one tag matches. All = all tags must match. */
	UPROPERTY(EditAnywhere, Category = "Consideration")
	EGameplayContainerMatchType MatchType = EGameplayContainerMatchType::Any;

	/** If true, require exact match. If false, parent tags also match. */
	UPROPERTY(EditAnywhere, Category = "Consideration")
	bool bExactMatch = false;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};
