// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Conditions/ArcMassActorGameplayAttributeCondition.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "Considerations/StateTreeCommonConsiderations.h"
#include "ArcUtilityConsideration.generated.h"

struct FGameplayTagQuery;
struct FGameplayAttribute;

/**
 * Base consideration for utility AI evaluation.
 * Derive to create custom considerations. Return raw [0,1] from Score().
 * The pipeline applies ResponseCurve then compensatory weight.
 */
USTRUCT(BlueprintType, meta = (ExcludeBaseStruct))
struct ARCAI_API FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual ~FArcUtilityConsideration() = default;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
	{
		return 1.0f;
	}

	UPROPERTY(EditAnywhere, Category = "Consideration")
	FStateTreeConsiderationResponseCurve ResponseCurve;

	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 0.01))
	float Weight = 1.0f;
};

USTRUCT(BlueprintType, DisplayName = "Distance Consideration")
struct ARCAI_API FArcUtilityConsideration_Distance : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 1.0))
	float MaxDistance = 5000.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, DisplayName = "Constant Consideration")
struct ARCAI_API FArcUtilityConsideration_Constant : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Value = 1.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, DisplayName = "Gameplay Tag Consideration")
struct ARCAI_API FArcUtilityConsideration_GameplayTag : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration")
	FGameplayTagQuery TagQuery;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, DisplayName = "Attribute Consideration")
struct ARCAI_API FArcUtilityConsideration_Attribute : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, Category = "Consideration")
	float MinValue = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Consideration")
	float MaxValue = 100.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};
