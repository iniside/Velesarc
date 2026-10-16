// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcTownFieldSettings.generated.h"

USTRUCT()
struct ARCTOWN_API FArcTownFieldSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Fields", meta = (ClampMin = "0.0001", ClampMax = "1.0", Units = "m"))
	double ElevationQuantumM = 0.01;

	UPROPERTY(EditAnywhere, Category = "Fields", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	double MaxBuildSlopeDeg = 15.0;

	UPROPERTY(EditAnywhere, Category = "Fields", meta = (ClampMin = "0.0", Units = "cm"))
	double MinWaterClearanceCm = 1500.0;

	UPROPERTY(EditAnywhere, Category = "Fields", meta = (ClampMin = "0.0", Units = "m"))
	double MinHeightAboveChannelM = 2.0;
};
