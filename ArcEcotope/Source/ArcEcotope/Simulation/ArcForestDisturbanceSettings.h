// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcForestDisturbanceSettings.generated.h"

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestDisturbanceSettings
{
	GENERATED_BODY()

	/** Years between fires on average. 0 means never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events",
		meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float FireReturnIntervalYears = 120.0f;

	/** Years between windthrow events on average. 0 means never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events",
		meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float WindReturnIntervalYears = 60.0f;

	/** Smallest an event can be, measured from its centre. Sizes are drawn toward this end, so most events are nearer the minimum than the maximum. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events",
		meta = (ClampMin = "100.0", ClampMax = "1000000.0"))
	float MinEventRadiusCm = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events",
		meta = (ClampMin = "100.0", ClampMax = "1000000.0"))
	float MaxEventRadiusCm = 15000.0f;

	/** Height at which a stem takes half the wind's full force, in centimetres. Wind hits the TALL hardest, so this leaves the understory and opens the canopy; lower it to flatten more of the stand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events",
		meta = (ClampMin = "1.0", ClampMax = "100000.0"))
	float WindHalfSeverityHeightCm = 2000.0f;

	/** Height at which a stem takes half the fire's full force, in centimetres. Fire works the opposite way to wind — it kills the SHORT hardest — so this leaves big trees standing over a cleared floor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events",
		meta = (ClampMin = "1.0", ClampMax = "100000.0"))
	float FireHalfSeverityHeightCm = 5000.0f;

	/** Share of a burnt plant that goes up in smoke rather than being left on the ground as dead wood. High values leave a clean burn; low values leave the standing dead where they fell. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events",
		meta = (ClampMin = "0.0", ClampMax = "0.99"))
	float FireCombustedFraction = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeadwoodConsumedFraction = 0.6f;

	bool IsValid(FText& OutError) const;
};

/** Unreflected on purpose: never authored, never serialized. Fire = 0 / Wind = 1 index the deterministic keys. */
enum class EArcForestDisturbanceKind : uint8
{
	Fire = 0,
	Wind = 1
};

struct FArcForestDisturbanceEvent
{
	EArcForestDisturbanceKind Kind = EArcForestDisturbanceKind::Fire;
	FVector2D CentreLocal = FVector2D::ZeroVector;
	double RadiusCm = 0.0;
	double WindBearingRadians = 0.0;
};

namespace UE::ArcForest
{
	ARCECOTOPE_API double CalculateDisturbanceSeverity(
		const FArcForestDisturbanceSettings& Settings,
		const FArcForestDisturbanceEvent& Event,
		uint32 NoiseSeed,
		const FVector2D& PositionLocal);

	ARCECOTOPE_API double CalculateDisturbanceHeightFactor(
		EArcForestDisturbanceKind Kind,
		const FArcForestDisturbanceSettings& Settings,
		double HeightCm);
}
