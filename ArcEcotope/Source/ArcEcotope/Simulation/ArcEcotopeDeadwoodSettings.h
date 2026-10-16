// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcEcotopeDeadwoodSettings.generated.h"

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcEcotopeDeadwoodSettings
{
	GENERATED_BODY()

	/**
	 * Share of the fallen logs and standing dead wood that rots away each year. The default is about
	 * seventeen years' residence on good soil; reference range 0.02-0.1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0001", ClampMax = "5.0"))
	float DeadwoodDecayPerYear = 0.06f;

	/**
	 * How heavy a dead plant has to be, in kilograms, to become a log or snag you can see on the ground.
	 * Raise it to stop the record list filling with dust; lower it to see smaller debris lying about.
	 *
	 * It also sets how long a visible piece lasts, since a record disappears once it has rotted below
	 * this weight. The default of 1 kg keeps the visible pieces at roughly the size real field surveys
	 * call coarse woody debris.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0.0001", ClampMax = "1000.0"))
	float MinimumTrackedDeadwoodKg = 1.0f;

	/**
	 * Share of wind-killed stems that tear their root plate out of the ground and become Uprooted
	 * records; the rest snap and become Log records. Deaths from any other cause stand as snags and
	 * do not consult this value.
	 *
	 * It is one draw per killed stem, so it is the share you get over a long run, not a quota on any one
	 * storm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UprootFractionOfWindKills = 0.4f;

	/**
	 * How thick a dead stem has to be at chest height, in centimetres, to count as wood at all.
	 * Anything thinner leaves no log or snag. 0 turns the gate off, so every dead stem is wood. Shed
	 * branches are exempt and admitted on MinimumTrackedDeadwoodKg instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
	float CoarseWoodyDebrisMinimumDbhCm = 10.0f;

	/**
	 * Chance per year that a standing dead stem falls over and becomes a downed log. Raise it to
	 * clear snags out of the stand quickly, lower it to leave standing dead wood up for decades.
	 *
	 * At the default half the snags standing today are down in about 23 years, which is the published
	 * band. Nothing stands a log back up, so how much standing dead wood a run shows is this rate
	 * balanced against how fast new snags are made. At 0 none ever falls.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SnagFallProbabilityPerYear = 0.03f;

	/**
	 * Chance per year that a living tree drops one branch big enough to leave on the ground. Raise it to
	 * litter a mature stand with fallen limbs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BranchShedProbabilityPerYear = 0.02f;

	/** Checks every parameter against the range its clamp metadata advertises. */
	bool IsValid(FText& OutError) const;
};

namespace UE::ArcEcotope::Deadwood
{
	constexpr double SnapBreakHeightFraction = 0.25;
	constexpr double FallBearingSpreadDeg = 15.0;
	constexpr double BranchDiameterFractionOfDbh = 0.15;
	constexpr double BranchLengthFractionOfHeight = 0.3;
	constexpr int32 FallSatelliteBranchCount = 3;
	constexpr double FallSatelliteSpreadCm = 150.0;

	/** Stands in for the temperature and moisture gating of decay this model lacks. */
	inline double CalculateDecayModerator(const double SoilCapacity)
	{
		return FMath::Clamp(0.5 + 0.5 * SoilCapacity, 0.0, 1.0);
	}

	inline double CalculateDecayedFraction(const float RatePerYear, const double Moderator)
	{
		return FMath::Clamp(static_cast<double>(RatePerYear) * Moderator, 0.0, 1.0);
	}

	/** The closed form holds only while DecayedFractionPerYear is fixed for the record's cell for the record's whole life. */
	inline int32 CalculateRetireYear(
		const int32 DeathYear,
		const double InitialBiomassKg,
		const double MinimumTrackedKg,
		const double DecayedFractionPerYear)
	{
		if (!(InitialBiomassKg > 0.0) || !FMath::IsFinite(InitialBiomassKg))
		{
			return DeathYear;
		}
		if (!(MinimumTrackedKg > 0.0))
		{
			return MAX_int32;
		}
		if (InitialBiomassKg <= MinimumTrackedKg || DecayedFractionPerYear >= 1.0)
		{
			return DeathYear;
		}
		if (DecayedFractionPerYear <= 0.0)
		{
			return MAX_int32;
		}
		const double Elapsed = FMath::CeilToDouble(
			FMath::Loge(MinimumTrackedKg / InitialBiomassKg) / FMath::Loge(1.0 - DecayedFractionPerYear));
		if (!FMath::IsFinite(Elapsed) || Elapsed >= static_cast<double>(MAX_int32 - DeathYear))
		{
			return MAX_int32;
		}
		return DeathYear + FMath::Max(0, static_cast<int32>(Elapsed));
	}

	inline int32 CalculateGeometricEventYear(
		const int32 StartYear,
		const double ProbabilityPerYear,
		const double UnitDraw)
	{
		if (!(ProbabilityPerYear > 0.0))
		{
			return MAX_int32;
		}
		if (ProbabilityPerYear >= 1.0)
		{
			return StartYear + 1;
		}
		const double ClampedDraw = FMath::Clamp(UnitDraw, UE_DOUBLE_SMALL_NUMBER, 1.0);
		const double Elapsed = FMath::FloorToDouble(
			FMath::Loge(ClampedDraw) / FMath::Loge(1.0 - ProbabilityPerYear));
		if (!FMath::IsFinite(Elapsed) || Elapsed >= static_cast<double>(MAX_int32 - StartYear - 1))
		{
			return MAX_int32;
		}
		return StartYear + 1 + FMath::Max(0, static_cast<int32>(Elapsed));
	}
}
