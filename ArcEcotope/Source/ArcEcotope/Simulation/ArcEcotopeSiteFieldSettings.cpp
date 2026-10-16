// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeSiteFieldSettings.h"

namespace ArcEcotopeSiteFieldSettingsPrivate
{
	bool CheckScalar(const TCHAR* Name, const double Value, const double Minimum, const double Maximum, FText& OutError)
	{
		if (FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum)
		{
			return true;
		}
		OutError = FText::FromString(FString::Printf(
			TEXT("%s must be finite and inside [%g, %g]."),
			Name,
			Minimum,
			Maximum));
		return false;
	}
}

bool FArcEcotopeSiteFieldSettings::IsValid(FText& OutError) const
{
	using namespace ArcEcotopeSiteFieldSettingsPrivate;

	const bool bScalarsValid =
		CheckScalar(TEXT("SoilDepthMinM"), SoilDepthMinM, 0.0, 100.0, OutError)
		&& CheckScalar(TEXT("SoilDepthNormalizerM"), SoilDepthNormalizerM, 0.01, 100.0, OutError)
		&& CheckScalar(TEXT("SoilSlopeShedCoefficient"), SoilSlopeShedCoefficient, 0.0, 1.0, OutError);
	if (!bScalarsValid)
	{
		return false;
	}

	OutError = FText::GetEmpty();
	return true;
}
