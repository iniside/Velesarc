// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcUtilityConsideration_OwnerAttribute.generated.h"

struct FGameplayAttribute;

/**
 * Reads a gameplay attribute from the querier (owner) actor's AbilitySystemComponent
 * and normalizes it to [0,1] using MinValue / MaxValue.
 */
USTRUCT(BlueprintType, DisplayName = "Owner Attribute Consideration")
struct ARCAI_API FArcUtilityConsideration_OwnerAttribute : public FArcUtilityConsideration
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
