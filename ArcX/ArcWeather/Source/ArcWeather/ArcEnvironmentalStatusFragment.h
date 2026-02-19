// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcEnvironmentalStatusFragment.generated.h"

namespace UE::ArcMass::Signals
{
	const FName EnvironmentalStatusChanged = FName(TEXT("EnvironmentalStatusChanged"));
}

/** Per-entity environmental status levels, each in [0, 100]. */
USTRUCT()
struct ARCWEATHER_API FArcEnvironmentalStatusFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 0 = dry, 100 = fully soaked. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "100"))
	float Wetness = 0.f;

	/** 0 = not frozen, 100 = fully frozen. Only accumulates when wet. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "100"))
	float Frozen = 0.f;
};

/** Shared config controlling accumulation/decay rates. Same for all entities of an archetype. */
USTRUCT()
struct ARCWEATHER_API FArcEnvironmentalStatusConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Wetness gain per second when humidity >= HumidityThreshold. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float WetRate = 5.f;

	/** Wetness loss per second when humidity < HumidityThreshold. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float DryRate = 2.f;

	/** Humidity percentage above which the cell counts as "raining". */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "100"))
	float HumidityThreshold = 70.f;

	/** Temperature (C) below which freezing begins. */
	UPROPERTY(EditAnywhere)
	float FreezeThreshold = 0.f;

	/** Frozen gain per second (scaled by Wetness/100) when temperature < FreezeThreshold. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float FreezeRate = 3.f;

	/** Frozen loss per second when temperature >= FreezeThreshold. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float ThawRate = 4.f;
};
