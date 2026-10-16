// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestDisturbanceSettings.h"

#include "Simulation/ArcForestDisturbanceNoise.h"

namespace ArcForestDisturbanceSettingsPrivate
{
	bool CheckScalar(const TCHAR* Name, const float Value, const float Minimum, const float Maximum, FText& OutError)
	{
		// Finiteness first: FMath::Clamp(NaN, Lo, Hi) returns Hi, so a NaN that reaches a clamp reads as the maximum.
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

bool FArcForestDisturbanceSettings::IsValid(FText& OutError) const
{
	using namespace ArcForestDisturbanceSettingsPrivate;

	const bool bScalarsValid =
		CheckScalar(TEXT("FireReturnIntervalYears"), FireReturnIntervalYears, 0.0f, 10000.0f, OutError)
		&& CheckScalar(TEXT("WindReturnIntervalYears"), WindReturnIntervalYears, 0.0f, 10000.0f, OutError)
		&& CheckScalar(TEXT("MinEventRadiusCm"), MinEventRadiusCm, 100.0f, 1000000.0f, OutError)
		&& CheckScalar(TEXT("MaxEventRadiusCm"), MaxEventRadiusCm, 100.0f, 1000000.0f, OutError)
		&& CheckScalar(TEXT("WindHalfSeverityHeightCm"), WindHalfSeverityHeightCm, 1.0f, 100000.0f, OutError)
		&& CheckScalar(TEXT("FireHalfSeverityHeightCm"), FireHalfSeverityHeightCm, 1.0f, 100000.0f, OutError)
		&& CheckScalar(TEXT("FireCombustedFraction"), FireCombustedFraction, 0.0f, 0.99f, OutError)
		&& CheckScalar(TEXT("DeadwoodConsumedFraction"), DeadwoodConsumedFraction, 0.0f, 1.0f, OutError);
	if (!bScalarsValid)
	{
		return false;
	}

	if (MinEventRadiusCm > MaxEventRadiusCm)
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("MinEventRadiusCm (%g) must not exceed MaxEventRadiusCm (%g)."),
			MinEventRadiusCm,
			MaxEventRadiusCm));
		return false;
	}

	OutError = FText::GetEmpty();
	return true;
}

namespace UE::ArcForest
{
	double CalculateDisturbanceSeverity(
		const FArcForestDisturbanceSettings& Settings,
		const FArcForestDisturbanceEvent& Event,
		const uint32 NoiseSeed,
		const FVector2D& PositionLocal)
	{
		const double Distance = FVector2D::Distance(PositionLocal, Event.CentreLocal);
		const double Radial = 1.0 - FMath::SmoothStep(0.6 * Event.RadiusCm, Event.RadiusCm, Distance);
		if (!(Radial > 0.0))
		{
			return 0.0;
		}
		const float NoiseValue = UE::ArcForest::SampleDisturbanceNoise01(
			FVector2f(static_cast<float>(PositionLocal.X), static_cast<float>(PositionLocal.Y)),
			NoiseSeed);
		constexpr double NoiseInfluence = 0.6;
		const double Severity = Radial * FMath::Lerp(1.0, static_cast<double>(NoiseValue), NoiseInfluence);
		return FMath::IsFinite(Severity) ? FMath::Clamp(Severity, 0.0, 1.0) : 0.0;
	}

	double CalculateDisturbanceHeightFactor(
		const EArcForestDisturbanceKind Kind,
		const FArcForestDisturbanceSettings& Settings,
		const double HeightCm)
	{
		const double Height = FMath::IsFinite(HeightCm) ? FMath::Max(0.0, HeightCm) : 0.0;
		const double HalfHeight = FMath::Max(
			1.0,
			static_cast<double>(Kind == EArcForestDisturbanceKind::Fire
				? Settings.FireHalfSeverityHeightCm
				: Settings.WindHalfSeverityHeightCm));
		return Kind == EArcForestDisturbanceKind::Fire
			? HalfHeight / (HalfHeight + Height)
			: Height / (HalfHeight + Height);
	}
}
