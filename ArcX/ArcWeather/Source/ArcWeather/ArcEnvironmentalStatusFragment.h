// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcEnvironmentalStatusFragment.generated.h"

/** Shared config controlling how weather translates into condition application rates. */
USTRUCT()
struct ARCWEATHER_API FArcEnvironmentalStatusConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Wet condition saturation applied per second when humidity >= HumidityThreshold. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float WetRate = 5.f;

	/** Humidity percentage above which the cell counts as "raining". */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "100"))
	float HumidityThreshold = 70.f;

	/** Temperature (C) below which cold condition is applied. */
	UPROPERTY(EditAnywhere)
	float FreezeThreshold = 0.f;

	/** Chilled condition saturation applied per second when temperature < FreezeThreshold. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float ColdRate = 3.f;
};
