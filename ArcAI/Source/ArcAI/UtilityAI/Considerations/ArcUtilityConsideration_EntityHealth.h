// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcUtilityConsideration_EntityHealth.generated.h"

/**
 * Reads the target Mass entity's FArcMassHealthFragment and returns
 * CurrentHealth / MaxHealth as a [0,1] score.
 */
USTRUCT(BlueprintType, DisplayName = "Entity Health Consideration")
struct ARCAI_API FArcUtilityConsideration_EntityHealth : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};
