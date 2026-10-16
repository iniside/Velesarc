// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "ArcResponseCurve.generated.h"

UENUM(BlueprintType, meta = (ToolTip = "Shape of a response curve. Every type except Custom is a formula over input x clamped to 0..1, with output sanitised to 0..1."))
enum class EArcResponseCurveType : uint8
{
	Linear UMETA(ToolTip = "y = m*(x-c) + b"),
	Polynomial UMETA(ToolTip = "y = m*(x-c)^k + b. A negative (x-c) with a non-integer k has no real result and scores 0."),
	Logistic UMETA(ToolTip = "y = m / (1 + exp(-10*k*(x-0.5-c))) + b. An S-curve centred at x = 0.5 + c."),
	Logit UMETA(ToolTip = "y = m*ln((x-c) / (1-(x-c))) / 5 + 0.5 + b. The inverse of the logistic S-curve."),
	Normal UMETA(ToolTip = "y = m*exp(-30*k*(x-c-0.5)^2) + b. A bell centred at x = 0.5 + c."),
	Sine UMETA(ToolTip = "y = 0.5*m*sin(2*pi*(x-c)) + 0.5 + b. One full sine period over 0..1."),
	Custom UMETA(ToolTip = "Hand-authored rich curve. Slower than a formula. An empty curve returns the input unchanged, and the output is not sanitised.")
};

USTRUCT(BlueprintType, meta = (ToolTip = "Maps a normalised 0..1 input to a 0..1 score through a formula curve or, for Custom, a hand-authored rich curve."))
struct ARCCORE_API FArcResponseCurve
{
	GENERATED_BODY()

	float Evaluate(float X) const;

	UPROPERTY(EditAnywhere, Category = "Response Curve", meta = (ToolTip = "Curve shape. Formula types use Slope (m), Exponent (k), XShift (c) and YShift (b); Custom uses the rich curve."))
	EArcResponseCurveType Type = EArcResponseCurveType::Linear;

	UPROPERTY(EditAnywhere, Category = "Response Curve", DisplayName = "Slope (m)", meta = (EditCondition = "Type != EArcResponseCurveType::Custom", EditConditionHides,
		ToolTip = "m. Linear: slope. Polynomial, Logit: vertical scale. Logistic: height of the S. Normal: height of the bell. Sine: amplitude. A negative m flips the curve vertically."))
	float Slope = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Response Curve", DisplayName = "Exponent (k)", meta = (EditCondition = "Type != EArcResponseCurveType::Custom", EditConditionHides,
		ToolTip = "k. Polynomial: power applied to (x-c). Logistic: steepness of the S. Normal: narrowness of the bell. Unused by Linear, Logit and Sine."))
	float Exponent = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Response Curve", DisplayName = "X Shift (c)", meta = (EditCondition = "Type != EArcResponseCurveType::Custom", EditConditionHides,
		ToolTip = "c. Moves the curve right along x by c for every formula type. Logistic, Logit and Normal are centred at x = 0.5 + c."))
	float XShift = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Response Curve", DisplayName = "Y Shift (b)", meta = (EditCondition = "Type != EArcResponseCurveType::Custom", EditConditionHides,
		ToolTip = "b. Added to the output for every formula type, before the result is clamped to 0..1."))
	float YShift = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Response Curve", DisplayName = "Curve", meta = (EditCondition = "Type == EArcResponseCurveType::Custom", EditConditionHides,
		ToolTip = "Rich curve evaluated at the unclamped input. Used only by Custom. Empty returns the input unchanged."))
	FRuntimeFloatCurve CurveInfo;
};
