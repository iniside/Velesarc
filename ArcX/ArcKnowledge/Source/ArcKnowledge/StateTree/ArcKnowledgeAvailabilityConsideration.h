// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConsiderationBase.h"
#include "GameplayTagContainer.h"
#include "ArcKnowledgeAvailabilityConsideration.generated.h"

USTRUCT()
struct FArcKnowledgeAvailabilityConsiderationInstanceData
{
	GENERATED_BODY()

	/** Tag query to match against knowledge entries. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagQuery TagQuery;

	/** Maximum search distance from the querier. 0 = unlimited. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 0.0))
	float MaxDistance = 10000.0f;
};

/**
 * StateTree consideration: scores 0-1 based on whether matching knowledge
 * exists near the entity. Higher score = more matching entries nearby.
 * Used for utility-based decision making (e.g., "should I go look for resources?").
 */
USTRUCT(DisplayName = "Arc Knowledge Availability")
struct ARCKNOWLEDGE_API FArcKnowledgeAvailabilityConsideration : public FStateTreeConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcKnowledgeAvailabilityConsiderationInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	virtual float GetScore(FStateTreeExecutionContext& Context) const override;
};
