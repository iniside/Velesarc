// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Conditions/ArcMassActorGameplayAttributeCondition.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "Considerations/StateTreeCommonConsiderations.h"
#include "ArcUtilityScorer.generated.h"

struct FGameplayTagQuery;
struct FGameplayAttribute;

/**
 * Base scorer for utility AI evaluation.
 * Derive to create custom scorers. Return raw [0,1] from Score().
 * The pipeline applies ResponseCurve then compensatory weight.
 */
USTRUCT(BlueprintType, meta = (ExcludeBaseStruct))
struct ARCAI_API FArcUtilityScorer
{
	GENERATED_BODY()

	virtual ~FArcUtilityScorer() = default;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
	{
		return 1.0f;
	}

	UPROPERTY(EditAnywhere, Category = "Scorer")
	FStateTreeConsiderationResponseCurve ResponseCurve;

	UPROPERTY(EditAnywhere, Category = "Scorer", meta = (ClampMin = 0.01))
	float Weight = 1.0f;
};

USTRUCT(BlueprintType, DisplayName = "Distance Scorer")
struct ARCAI_API FArcUtilityScorer_Distance : public FArcUtilityScorer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Scorer", meta = (ClampMin = 1.0))
	float MaxDistance = 5000.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, DisplayName = "Constant Scorer")
struct ARCAI_API FArcUtilityScorer_Constant : public FArcUtilityScorer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Scorer", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Value = 1.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, DisplayName = "Gameplay Tag Scorer")
struct ARCAI_API FArcUtilityScorer_GameplayTag : public FArcUtilityScorer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Scorer")
	FGameplayTagQuery TagQuery;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, DisplayName = "Attribute Scorer")
struct ARCAI_API FArcUtilityScorer_Attribute : public FArcUtilityScorer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Scorer")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, Category = "Scorer")
	float MinValue = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Scorer")
	float MaxValue = 100.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};
