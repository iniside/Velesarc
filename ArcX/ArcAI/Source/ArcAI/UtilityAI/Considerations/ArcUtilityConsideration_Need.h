// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcUtilityConsideration_Need.generated.h"

/**
 * Reads a need value from the target entity's need fragment (FArcNeedFragment-derived)
 * and returns CurrentValue normalized to [0,1].
 *
 * Select which need type via NeedType (MetaStruct selector for FArcNeedFragment subtypes).
 */
USTRUCT(BlueprintType, DisplayName = "Target Need Consideration")
struct ARCAI_API FArcUtilityConsideration_TargetNeed : public FArcUtilityConsideration
{
	GENERATED_BODY()

	/** Which need fragment type to read from the target entity. */
	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (MetaStruct = "/Script/ArcAI.ArcNeedFragment"))
	TObjectPtr<UScriptStruct> NeedType;

	/** Maximum expected need value for normalization. */
	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 0.01))
	float MaxNeedValue = 100.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Reads a need value from the querier (owner) entity's need fragment.
 */
USTRUCT(BlueprintType, DisplayName = "Owner Need Consideration")
struct ARCAI_API FArcUtilityConsideration_OwnerNeed : public FArcUtilityConsideration
{
	GENERATED_BODY()

	/** Which need fragment type to read from the owner entity. */
	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (MetaStruct = "/Script/ArcAI.ArcNeedFragment"))
	TObjectPtr<UScriptStruct> NeedType;

	/** Maximum expected need value for normalization. */
	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 0.01))
	float MaxNeedValue = 100.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};
