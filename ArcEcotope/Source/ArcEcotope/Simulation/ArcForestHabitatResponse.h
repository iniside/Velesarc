// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ArcForestBootstrap.h"

struct FArcEcotopeSiteChunkView;

namespace UE::ArcForest::Habitat
{
	constexpr int32 HabitatResponseCount = 3;

	/** Which term produced a hard zero; None when the aggregate is not a hard zero. */
	enum class EHabitatHardZeroReason : uint8
	{
		None,
		Slope,
		SunLoad,
		Wind,
		Fertility,
		Temperature,
		Moisture,
	};

	/** One species' response curves, resolved once so a per-sample loop need not rebuild the pointer table. */
	struct FHabitatResponseTable
	{
		const FArcForestFrozenResponseRange* Responses[HabitatResponseCount] = {};
	};

	/** Storage-independent site values needed to score one base-grid cell. */
	struct FHabitatSiteSample
	{
		float Slope = 0.0f;
		float Moisture = 0.0f;
		float RawFertility = 0.0f;
		float SunLoad = 0.0f;
		float WindExposure = 0.0f;
		float Temperature = 0.5f;
		float ModificationCut = 0.0f;
		float BareRock = 0.0f;
		float ChannelBuffer = 0.0f;
	};

	/** Reads one base cell through a chunk view; false means the view does not cover every required field. */
	ARCECOTOPE_API bool TryReadHabitatSiteSample(
		const FArcEcotopeSiteChunkView& View,
		int32 GlobalBaseX,
		int32 GlobalBaseY,
		FHabitatSiteSample& OutSample);

	/**
	 * Site-only habitat for one species and one cell, invariant for a whole run. The explicit zero
	 * preserves an authored hard exclusion independently of the precision chosen to store the term.
	 */
	struct FHabitatStaticFactor
	{
		float SiteHabitat = 1.0f;
		bool bHardZero = false;
		EHabitatHardZeroReason HardZeroReason = EHabitatHardZeroReason::None;
	};

	ARCECOTOPE_API double EvaluateResponse(const FArcForestFrozenResponseRange& Response, const float Value);

	/** Single source of the habitat factor order: 0 light, 1 moisture, 2 temperature. */
	ARCECOTOPE_API void GetHabitatResponses(
		const FArcForestFrozenSpeciesSetup& Model,
		const FArcForestFrozenResponseRange* (&OutResponses)[HabitatResponseCount]);

	/** 1 leaves a placement untouched, 0 removes it; a species at sensitivity 0 always gets 1. */
	inline double MakeExclusionFactor(const double Exclusion, const float ExclusionSensitivity)
	{
		return FMath::Clamp(1.0 - Exclusion * static_cast<double>(ExclusionSensitivity), 0.0, 1.0);
	}

	/** OutZeroReason reports the first zeroing factor in priority order, not every factor that is zero. */
	ARCECOTOPE_API double EvaluateHabitatThresholdMask(
		const FHabitatSiteSample& Sample,
		const FArcForestFrozenSpeciesSetup& Model,
		EHabitatHardZeroReason* OutZeroReason = nullptr);

	ARCECOTOPE_API void EvaluateHabitatResponseTerms(
		const FHabitatSiteSample& Sample,
		const FHabitatResponseTable& Responses,
		double Light,
		double (&OutOutputs)[HabitatResponseCount]);

	/**
	 * Light stays multiplicative at full weight because it is the only density-dependent term and
	 * therefore the only feedback the stand exerts on itself; rooting it would flatten that contrast.
	 */
	ARCECOTOPE_API double AggregateHabitat(
		const double (&Outputs)[HabitatResponseCount],
		double ThresholdMask);

	ARCECOTOPE_API void BuildStaticHabitatFactor(
		const FHabitatSiteSample& Sample,
		const FHabitatResponseTable& Responses,
		const FArcForestFrozenSpeciesSetup& Model,
		FHabitatStaticFactor& OutFactor);
}
