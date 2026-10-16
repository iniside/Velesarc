// Copyright Lukasz Baran. All Rights Reserved.

#include "Climate/ArcClimateSettings.h"

namespace ArcClimateSettingsFileLocal
{
	bool CheckScalar(const TCHAR* Name, const double Value, const double Minimum, const double Maximum, FText& OutError)
	{
		if (FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum)
		{
			return true;
		}
		OutError = FText::FromString(FString::Printf(
			TEXT("%s must be finite and inside [%g, %g]."), Name, Minimum, Maximum));
		return false;
	}

}

bool FArcClimateHydrologySettings::IsValid(FText& OutError) const
{
	using namespace ArcClimateSettingsFileLocal;
	if (!CheckScalar(TEXT("FlowToCatchmentAreaScale"), FlowToCatchmentAreaScale, 0.0, 1.0e9, OutError)
		|| !CheckScalar(TEXT("MissingFlowCatchmentAreaM2"), MissingFlowCatchmentAreaM2, 0.0, 1.0e9, OutError)
		|| !CheckScalar(TEXT("RiversWeight"), RiversWeight, 0.0, 1.0, OutError)
		|| !CheckScalar(TEXT("RiversGamma"), RiversGamma, 0.01, 10.0, OutError)
		|| !CheckScalar(TEXT("RiversChannelThreshold"), RiversChannelThreshold, 0.01, 1.0, OutError)
		|| !CheckScalar(TEXT("StillWaterCoolingBoost"), StillWaterCoolingBoost, 1.0, 8.0, OutError)
		|| !CheckScalar(TEXT("LapseRatePer100M"), LapseRatePer100M, 0.0, 1.0, OutError)
		|| !CheckScalar(TEXT("MoistureCoolingStrength"), MoistureCoolingStrength, 0.0, 1.0, OutError)
		|| !CheckScalar(TEXT("ColdAirDrainageStrength"), ColdAirDrainageStrength, 0.0, 1.0, OutError)
		|| !CheckScalar(TEXT("ColdAirPoolingDepthM"), ColdAirPoolingDepthM, 0.01, 200.0, OutError)
		|| !CheckScalar(TEXT("WaterHumidityReachM"), WaterHumidityReachM, 0.0, 2000.0, OutError))
	{
		return false;
	}
	OutError = FText::GetEmpty();
	return true;
}
bool FArcClimateFieldSettings::IsValid(FText& OutError) const
{
	using namespace ArcClimateSettingsFileLocal;
	if (!CheckScalar(TEXT("WetnessIndexMidpoint"), WetnessIndexMidpoint, 0.0, 30.0, OutError)
		|| !CheckScalar(TEXT("WetnessIndexWidth"), WetnessIndexWidth, 0.01, 30.0, OutError)
		|| !CheckScalar(TEXT("HeightAboveDrainageDecayM"), HeightAboveDrainageDecayM, 0.01, 1000.0, OutError)
		|| !CheckScalar(TEXT("ElevationQuantumM"), ElevationQuantumM, 0.0001, 1.0, OutError)
		|| !CheckScalar(TEXT("LatitudeDeg"), LatitudeDeg, 30.0, 60.0, OutError)
		|| !CheckScalar(TEXT("NorthYawDeg"), NorthYawDeg, 0.0, 360.0, OutError)
		|| !CheckScalar(TEXT("HorizonRadiusM"), HorizonRadiusM, 0.0, 10000.0, OutError)
		|| !CheckScalar(TEXT("HorizonAzimuths"), HorizonAzimuths, 1.0, 360.0, OutError)
		|| !CheckScalar(TEXT("MinReliefOffsetM"), MinReliefOffsetM, 0.0, 10000.0, OutError)
		|| !CheckScalar(TEXT("MinReliefM"), MinReliefM, 0.0, 10000.0, OutError)
		|| !CheckScalar(TEXT("PrevailingWindAzimuthDeg"), PrevailingWindAzimuthDeg, 0.0, 360.0, OutError)
		|| !CheckScalar(TEXT("WindAnisotropy"), WindAnisotropy, 0.0, 1.0, OutError)
		|| !CheckScalar(TEXT("RefShelterAngleDeg"), RefShelterAngleDeg, 0.01, 90.0, OutError)
		|| TemperatureLevel > 8 || HumidityLevel > 8 || SunLoadLevel > 8 || HorizonLevel > 8)
	{
		if (OutError.IsEmpty())
		{
			OutError = FText::FromString(TEXT("Climate field levels must not exceed 8."));
		}
		return false;
	}
	OutError = FText::GetEmpty();
	return true;
}

