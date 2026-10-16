// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeTerrainDataSettings.h"

namespace ArcEcotopeTerrainDataSettingsPrivate
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

bool FArcEcotopeTerrainDataSettings::IsValid(FText& OutError) const
{
	using namespace ArcEcotopeTerrainDataSettingsPrivate;

	const bool bDepthsValid =
		CheckScalar(TEXT("DepositsAtZeroDepthM"), DepositsAtZeroDepthM, 0.0, 100.0, OutError)
		&& CheckScalar(TEXT("DepositsAtMaxDepthM"), DepositsAtMaxDepthM, 0.0, 100.0, OutError)
		&& CheckScalar(TEXT("MissingDepositsDepthM"), MissingDepositsDepthM, 0.0, 100.0, OutError);
	if (!bDepthsValid)
	{
		return false;
	}

	const bool bWeightsValid =
		CheckScalar(TEXT("RockWeight"), RockWeight, 0.0, 1.0, OutError)
		&& CheckScalar(TEXT("WearWeight"), WearWeight, 0.0, 1.0, OutError)
		&& CheckScalar(TEXT("ScreeWeight"), ScreeWeight, 0.0, 1.0, OutError)
		&& CheckScalar(TEXT("FlowAlignStrength"), FlowAlignStrength, 0.0, 1.0, OutError);
	if (!bWeightsValid)
	{
		return false;
	}

	const bool bChannelBufferValid =
		CheckScalar(TEXT("TreeBufferHardDistanceCm"), TreeBufferHardDistanceCm, 0.0, 10000.0, OutError)
		&& CheckScalar(TEXT("TreeBufferFalloffDistanceCm"), TreeBufferFalloffDistanceCm, 0.0, 10000.0, OutError);
	if (!bChannelBufferValid)
	{
		return false;
	}

	OutError = FText::GetEmpty();
	return true;
}
