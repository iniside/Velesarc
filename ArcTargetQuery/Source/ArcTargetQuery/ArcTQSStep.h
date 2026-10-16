// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "Math/ArcResponseCurve.h"
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
 * - Filter steps: a raw value <= 0 discards the item.
 * - Score steps: the pipeline passes the clamped raw value through ResponseCurve and multiplies the result into Item.Score.
 */
USTRUCT(BlueprintType)
struct ARCTARGETQUERY_API FArcTQSStep
{
	GENERATED_BODY()

	virtual ~FArcTQSStep() = default;

	/** Raw value returned when a step cannot evaluate an item; it scores 0 and fails every Filter. */
	static constexpr float UnscorableRaw = -1.0f;

	/**
	 * Returns the raw 0..1 value for one item, or a negative value (UnscorableRaw) when the item cannot be evaluated.
	 * Never apply ResponseCurve here. Neutral fallbacks (e.g. 0.5, DefaultScoreForNoHealth) are valid raw values and get curved.
	 * For Filter: a value <= 0 discards the item.
	 * For Score: the pipeline applies ResponseCurve through EvaluateScore; a negative value scores 0 without the curve.
	 */
	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
	{
		return 1.0f;
	}

	virtual EArcTQSWorldRequirements GetWorldRequirements() const { return EArcTQSWorldRequirements::None; }

	/** Returns 0 for a negative RawScore; otherwise clamps it to 0..1, applies ResponseCurve, and returns a finite 0..1 score factor. */
	float EvaluateScore(float RawScore) const;

	UPROPERTY()
	FGuid ID;

	UPROPERTY(EditAnywhere, Category = "Step")
	EArcTQSStepType StepType = EArcTQSStepType::Score;

	/** Applied by the pipeline to every Score step's raw value. Default Linear is identity. Filter steps ignore it. */
	UPROPERTY(EditAnywhere, Category = "Step", meta = (EditCondition = "StepType == EArcTQSStepType::Score"))
	FArcResponseCurve ResponseCurve;
};
