// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcEcotopeSiteFieldSettings.generated.h"

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcEcotopeSiteFieldSettings
{
	GENERATED_BODY()

	/**
	 * Shallowest soil the bake will admit, in metres. With SoilSlopeShedCoefficient it sets the floor of
	 * the capacity field, which at the shipped defaults sits just above zero — so nowhere is completely
	 * barren. Author it to 0 and a truly barren cell becomes reachable, and a species whose
	 * FertilityResponse reads zero there is excluded from that cell outright.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "m"))
	float SoilDepthMinM = 0.05f;

	/** Soil depth that counts as fully fertile ground, in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.01", ClampMax = "100.0", Units = "m"))
	float SoilDepthNormalizerM = 1.0f;

	/**
	 * How much of its fertility a slope loses to being steep. Flat ground keeps all of it and the
	 * penalty reaches full strength at 40 degrees, so this is what makes valley floors read richer than
	 * the faces above them.
	 *
	 * At 1 a 40-degree face is barren rather than merely poor, and the shipped FertilityResponse then
	 * excludes it outright; at 0 steepness stops affecting fertility at all. It is a judgement value —
	 * fertility read off terrain has no published calibration. Reference range 0.3-0.8.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SoilSlopeShedCoefficient = 0.6f;

	/** Checks every parameter against the range its clamp metadata advertises. */
	bool IsValid(FText& OutError) const;
};
