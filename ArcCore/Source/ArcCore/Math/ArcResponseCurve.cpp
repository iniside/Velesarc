// Copyright Lukasz Baran. All Rights Reserved.

#include "Math/ArcResponseCurve.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcResponseCurve)

namespace ArcResponseCurvePrivate
{
float EvaluateFormula(const FArcResponseCurve& Curve, float X)
{
	const float M = Curve.Slope;
	const float K = Curve.Exponent;
	const float C = Curve.XShift;
	const float B = Curve.YShift;

	switch (Curve.Type)
	{
	case EArcResponseCurveType::Linear:
		return M * (X - C) + B;
	case EArcResponseCurveType::Polynomial:
		return M * FMath::Pow(X - C, K) + B;
	case EArcResponseCurveType::Logistic:
		return M / (1.0f + FMath::Exp(-10.0f * K * (X - 0.5f - C))) + B;
	case EArcResponseCurveType::Logit:
	{
		const float Shifted = X - C;
		return M * FMath::Loge(Shifted / (1.0f - Shifted)) / 5.0f + 0.5f + B;
	}
	case EArcResponseCurveType::Normal:
	{
		const float Offset = X - C - 0.5f;
		return M * FMath::Exp(-30.0f * K * Offset * Offset) + B;
	}
	case EArcResponseCurveType::Sine:
		return 0.5f * M * FMath::Sin(UE_TWO_PI * (X - C)) + 0.5f + B;
	default:
		return X;
	}
}
}

float FArcResponseCurve::Evaluate(float X) const
{
	if (Type == EArcResponseCurveType::Custom)
	{
		if (const FRichCurve* Curve = CurveInfo.GetRichCurveConst())
		{
			if (!Curve->IsEmpty())
			{
				return Curve->Eval(X);
			}
		}
		return X;
	}

	const float Result = ArcResponseCurvePrivate::EvaluateFormula(*this, FMath::Clamp(X, 0.0f, 1.0f));
	if (FMath::IsNaN(Result))
	{
		return 0.0f;
	}
	return FMath::Clamp(Result, 0.0f, 1.0f);
}
