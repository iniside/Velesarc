/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Fragments/ArcNeedFragment.h"
#include "ArcNeedBands.generated.h"

namespace ArcNeeds
{
	constexpr float DefaultSecondsPerGameHour = 150.0f;

	/** needs.yaml:18 scale [0, 100]. */
	constexpr float NeedMaxValue = 100.0f;

	/** needs.yaml:29 VeryHungry and needs.yaml:47 Exhausted both open at 61 — "interrupts work". */
	constexpr float InterruptWorkValue = 61.0f;

	/** needs.yaml:31 StarvationHP — HP drain 2/min. */
	constexpr float StarvationHealthPerSecond = 2.0f / 60.0f;

	/** needs.yaml:42 recovery_per_hour 8.0, scaled by the sleeping spot's Comfort (needs.yaml:61-65). */
	constexpr float SleepRecoveryPerHour = 8.0f;
}

/** Half-open [Min, Max): needs.yaml writes integer ranges but CurrentValue is a float, so 30.4 must land in a band. */
USTRUCT()
struct ARCNEEDS_API FArcNeedBand
{
	GENERATED_BODY()

	UPROPERTY()
	float Min = 0.0f;

	UPROPERTY()
	float Max = 0.0f;
};

/** Band edges from GameDesign/mechanics/settlement/needs.yaml, one table per need. */
USTRUCT()
struct ARCNEEDS_API FArcNeedBandTableFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	FArcNeedBandTableFragment();

	UPROPERTY()
	TArray<FArcNeedBand> HungerBands;

	UPROPERTY()
	TArray<FArcNeedBand> FatigueBands;

	UPROPERTY()
	float SecondsPerGameHour = ArcNeeds::DefaultSecondsPerGameHour;
};

/** Both band arrays are TArrays, so the fragment is not trivially copyable. */
template <>
struct TMassFragmentTraits<FArcNeedBandTableFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

namespace ArcNeeds
{
	/** needs.yaml states every rate per game hour; the settlement rail's clock is UWorld::GetTimeSeconds. */
	inline float PerHourToPerSecond(float PerHour, float SecondsPerGameHour)
	{
		return PerHour / FMath::Max(SecondsPerGameHour, UE_SMALL_NUMBER);
	}

	/** One game-minute: a need is linear in time, so one large delta is the same value as sixty small ones. */
	inline double EvaluateIntervalSeconds(float SecondsPerGameHour)
	{
		return static_cast<double>(SecondsPerGameHour) / 60.0;
	}

	inline double RepostIntervalSeconds(float SecondsPerGameHour)
	{
		return static_cast<double>(SecondsPerGameHour);
	}

	inline float SleepRecoveryPerSecond(float SecondsPerGameHour)
	{
		return PerHourToPerSecond(SleepRecoveryPerHour, SecondsPerGameHour);
	}

	/** INDEX_NONE only for an empty table; the last band is open-ended upward and everything below the first edge is band 0. */
	ARCNEEDS_API int32 BandIndexFor(const TArray<FArcNeedBand>& Bands, float Value);

	ARCNEEDS_API const TArray<FArcNeedBand>& HungerBandsFor(const FArcNeedBandTableFragment* BandTable);

	ARCNEEDS_API const TArray<FArcNeedBand>& FatigueBandsFor(const FArcNeedBandTableFragment* BandTable);

	ARCNEEDS_API void AdvanceNeed(FArcNeedFragment& Need, double Now, float SecondsPerGameHour, bool bForce);
}
