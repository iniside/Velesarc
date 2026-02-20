// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_AttributeRelative.generated.h"

UENUM(BlueprintType)
enum class EArcTQSAttributeCompareMode : uint8
{
	// TargetNorm / (TargetNorm + QuerierNorm). Equal values = 0.5, target higher = toward 1.0.
	Ratio,
	// (TargetNorm - QuerierNorm + 1) / 2. Maps [-1,1] difference into [0,1].
	Difference
};

/**
 * Scores items based on the relationship between a target actor's attribute
 * and the querier actor's attribute.
 *
 * Both values are first normalized to [0,1] using either a max attribute or manual max value,
 * then combined into a score using the selected comparison mode.
 *
 * Example use cases:
 *   - Prefer targets weaker than querier (Ratio mode + invert response curve)
 *   - Score by how much more health the target has vs querier (Difference mode)
 *   - Prefer evenly matched targets (Difference mode + bell curve response)
 */
USTRUCT(DisplayName = "Attribute Relative Score")
struct ARCAI_API FArcTQSStep_AttributeRelative : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_AttributeRelative()
	{
		StepType = EArcTQSStepType::Score;
	}

	// The gameplay attribute to read from the TARGET actor
	UPROPERTY(EditAnywhere, Category = "Target")
	FGameplayAttribute TargetAttribute;

	// If true, use TargetMaxAttribute for normalization. If false, use TargetManualMaxValue.
	UPROPERTY(EditAnywhere, Category = "Target")
	bool bUseTargetMaxAttribute = false;

	// Max attribute on the target actor for normalization
	UPROPERTY(EditAnywhere, Category = "Target", meta = (EditCondition = "bUseTargetMaxAttribute"))
	FGameplayAttribute TargetMaxAttribute;

	// Manual max value for normalizing the target attribute
	UPROPERTY(EditAnywhere, Category = "Target", meta = (EditCondition = "!bUseTargetMaxAttribute", ClampMin = 0.01))
	float TargetManualMaxValue = 100.0f;

	// The gameplay attribute to read from the QUERIER actor
	UPROPERTY(EditAnywhere, Category = "Querier")
	FGameplayAttribute QuerierAttribute;

	// If true, use QuerierMaxAttribute for normalization. If false, use QuerierManualMaxValue.
	UPROPERTY(EditAnywhere, Category = "Querier")
	bool bUseQuerierMaxAttribute = false;

	// Max attribute on the querier actor for normalization
	UPROPERTY(EditAnywhere, Category = "Querier", meta = (EditCondition = "bUseQuerierMaxAttribute"))
	FGameplayAttribute QuerierMaxAttribute;

	// Manual max value for normalizing the querier attribute
	UPROPERTY(EditAnywhere, Category = "Querier", meta = (EditCondition = "!bUseQuerierMaxAttribute", ClampMin = 0.01))
	float QuerierManualMaxValue = 100.0f;

	/**
	 * How to combine the two normalized values:
	 *   Ratio:      TargetNorm / (TargetNorm + QuerierNorm). Equal = 0.5, target higher = toward 1.
	 *   Difference:  (TargetNorm - QuerierNorm + 1) / 2. Maps [-1,1] difference to [0,1].
	 */
	UPROPERTY(EditAnywhere, Category = "Step")
	EArcTQSAttributeCompareMode CompareMode = EArcTQSAttributeCompareMode::Ratio;

	// If true, items without a resolvable target actor or ASC are filtered out
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bFilterItemsWithoutAttribute = false;

	// Score for items missing attribute data (when not filtering)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0, ClampMax = 1.0, EditCondition = "!bFilterItemsWithoutAttribute"))
	float DefaultScore = 0.5f;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
