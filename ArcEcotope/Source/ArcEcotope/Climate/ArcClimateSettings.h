// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcClimateSettings.generated.h"

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcClimateHydrologySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.0", ClampMax = "1000000000.0"))
	float FlowToCatchmentAreaScale = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.0", ClampMax = "1000000000.0"))
	float MissingFlowCatchmentAreaM2 = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water")
	bool bUseRivers = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bUseRivers"))
	float RiversWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.01", ClampMax = "10.0", EditCondition = "bUseRivers"))
	float RiversGamma = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.01", ClampMax = "1.0", EditCondition = "bUseRivers"))
	float RiversChannelThreshold = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water")
	bool bUseFlowDirection = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "1.0", ClampMax = "8.0", EditCondition = "bUseFlowDirection"))
	float StillWaterCoolingBoost = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LapseRatePer100M = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoistureCoolingStrength = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ColdAirDrainageStrength = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.01", ClampMax = "200.0", Units = "m"))
	float ColdAirPoolingDepthM = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate")
	bool bColdAirWeightedBySkyView = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "2000.0", EditCondition = "bUseRivers", Units = "m"))
	float WaterHumidityReachM = 40.0f;

	bool IsValid(FText& OutError) const;
};

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcClimateFieldSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float WetnessIndexMidpoint = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.01", ClampMax = "30.0"))
	float WetnessIndexWidth = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.01", ClampMax = "1000.0", Units = "m"))
	float HeightAboveDrainageDecayM = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.0001", ClampMax = "1.0", Units = "m"))
	float ElevationQuantumM = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "30.0", ClampMax = "60.0", Units = "deg"))
	float LatitudeDeg = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
	float NorthYawDeg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate")
	bool bMirrorEastWest = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "10000.0", Units = "m"))
	float HorizonRadiusM = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "1", ClampMax = "360"))
	int32 HorizonAzimuths = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate")
	bool bAutoMinRelief = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "10000.0", Units = "m"))
	float MinReliefOffsetM = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "10000.0", Units = "m", EditCondition = "!bAutoMinRelief"))
	float MinReliefM = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
	float PrevailingWindAzimuthDeg = 270.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WindAnisotropy = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.01", ClampMax = "90.0", Units = "deg"))
	float RefShelterAngleDeg = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quality", meta = (ClampMin = "0", ClampMax = "8"))
	uint8 TemperatureLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quality", meta = (ClampMin = "0", ClampMax = "8"))
	uint8 HumidityLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quality", meta = (ClampMin = "0", ClampMax = "8"))
	uint8 SunLoadLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quality", meta = (ClampMin = "0", ClampMax = "8"))
	uint8 HorizonLevel = 3;

	bool IsValid(FText& OutError) const;
};
