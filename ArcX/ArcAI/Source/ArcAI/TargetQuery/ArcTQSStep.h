// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "Considerations/StateTreeCommonConsiderations.h"
#include "ArcTQSStep.generated.h"

UENUM(BlueprintType)
enum class EArcTQSStepType : uint8
{
	Filter,
	Score
};

/**
 * Base struct for TQS pipeline steps. Derive from this to create custom filters and scorers.
 *
 * Steps are stored as FInstancedStruct in the query definition and executed in order.
 * - Filter steps: set Item.bValid = false to discard items
 * - Score steps: return a 0-1 normalized score. Combined multiplicatively with compensation:
 *   CompensatedScore = FMath::Pow(RawScore, Weight)
 *   Weight < 1 softens influence, Weight > 1 sharpens it
 */
USTRUCT(BlueprintType)
struct ARCAI_API FArcTQSStep
{
	GENERATED_BODY()

	virtual ~FArcTQSStep() = default;

	/**
	 * Process a single target item.
	 * For Filter: set Item.bValid = false to discard.
	 * For Score: return a value in [0, 1]. The pipeline applies weight and multiplies into Item.Score.
	 */
	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
	{
		return 1.0f;
	}

	/**
	 * Batch process items. Default implementation calls ExecuteStep per valid item,
	 * applies compensatory scoring for Score steps, and invalidates for Filter steps.
	 * Override for steps that benefit from batch processing (e.g., spatial queries).
	 */
	virtual void ExecuteStepBatch(
		TArray<FArcTQSTargetItem>& Items,
		int32 StartIndex,
		int32 EndIndex,
		const FArcTQSQueryContext& QueryContext) const;

	UPROPERTY(EditAnywhere, Category = "Step")
	EArcTQSStepType StepType = EArcTQSStepType::Score;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (EditCondition = "StepType == EArcTQSStepType::Score"))
	FStateTreeConsiderationResponseCurve ResponseCurve;
	
	/**
	 * For Score steps: power exponent for compensatory scoring.
	 * CompensatedScore = Pow(RawScore, Weight)
	 * < 1.0 compresses toward 1.0 (softens), > 1.0 amplifies differences (sharpens).
	 * Ignored for Filter steps.
	 */
	UPROPERTY(EditAnywhere, Category = "Step", meta = (EditCondition = "StepType == EArcTQSStepType::Score", ClampMin = 0.01))
	float Weight = 1.0f;
};
