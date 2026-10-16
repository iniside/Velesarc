// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep.h"

float FArcTQSStep::EvaluateScore(const float RawScore) const
{
	if (RawScore < 0.0f)
	{
		return 0.0f;
	}
	const float CurveScore = ResponseCurve.Evaluate(FMath::Clamp(RawScore, 0.0f, 1.0f));
	if (!FMath::IsFinite(CurveScore))
	{
		return 0.0f;
	}
	return FMath::Clamp(CurveScore, 0.0f, 1.0f);
}
