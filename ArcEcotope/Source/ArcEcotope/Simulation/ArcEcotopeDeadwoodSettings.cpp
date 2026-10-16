// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeDeadwoodSettings.h"

namespace ArcEcotopeDeadwoodSettingsPrivate
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

bool FArcEcotopeDeadwoodSettings::IsValid(FText& OutError) const
{
	using namespace ArcEcotopeDeadwoodSettingsPrivate;

	const bool bScalarsValid =
		CheckScalar(TEXT("DeadwoodDecayPerYear"), DeadwoodDecayPerYear, 0.0001, 5.0, OutError)
		&& CheckScalar(TEXT("MinimumTrackedDeadwoodKg"), MinimumTrackedDeadwoodKg, 0.0001, 1000.0, OutError)
		&& CheckScalar(TEXT("UprootFractionOfWindKills"), UprootFractionOfWindKills, 0.0, 1.0, OutError)
		&& CheckScalar(
			TEXT("CoarseWoodyDebrisMinimumDbhCm"),
			CoarseWoodyDebrisMinimumDbhCm,
			0.0,
			1000.0,
			OutError)
		&& CheckScalar(TEXT("SnagFallProbabilityPerYear"), SnagFallProbabilityPerYear, 0.0, 1.0, OutError)
		&& CheckScalar(TEXT("BranchShedProbabilityPerYear"), BranchShedProbabilityPerYear, 0.0, 1.0, OutError);
	if (!bScalarsValid)
	{
		return false;
	}

	OutError = FText::GetEmpty();
	return true;
}
