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

#include "Fragments/ArcNeedBands.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcNeedBands)

namespace ArcNeedBandsPrivate
{

FArcNeedBand MakeBand(float Min, float Max)
{
	FArcNeedBand Band;
	Band.Min = Min;
	Band.Max = Max;
	return Band;
}

} // namespace ArcNeedBandsPrivate

FArcNeedBandTableFragment::FArcNeedBandTableFragment()
{
	using namespace ArcNeedBandsPrivate;

	// needs.yaml:27-31 — Sated / Hungry / VeryHungry / Starving / StarvationHP.
	HungerBands.Add(MakeBand(0.0f, 31.0f));
	HungerBands.Add(MakeBand(31.0f, 61.0f));
	HungerBands.Add(MakeBand(61.0f, 81.0f));
	HungerBands.Add(MakeBand(81.0f, ArcNeeds::NeedMaxValue));
	HungerBands.Add(MakeBand(ArcNeeds::NeedMaxValue, TNumericLimits<float>::Max()));

	// needs.yaml:45-48 — Rested / Tired / Exhausted / OnTheEdge.
	FatigueBands.Add(MakeBand(0.0f, 21.0f));
	FatigueBands.Add(MakeBand(21.0f, 61.0f));
	FatigueBands.Add(MakeBand(61.0f, 81.0f));
	FatigueBands.Add(MakeBand(81.0f, TNumericLimits<float>::Max()));
}

namespace ArcNeeds
{

int32 BandIndexFor(const TArray<FArcNeedBand>& Bands, float Value)
{
	if (Bands.IsEmpty())
	{
		return INDEX_NONE;
	}
	if (Value < Bands[0].Min)
	{
		return 0;
	}
	for (int32 BandIndex = 0; BandIndex < Bands.Num(); ++BandIndex)
	{
		if (Value >= Bands[BandIndex].Min && Value < Bands[BandIndex].Max)
		{
			return BandIndex;
		}
	}
	return Bands.Num() - 1;
}

const TArray<FArcNeedBand>& HungerBandsFor(const FArcNeedBandTableFragment* BandTable)
{
	if (BandTable)
	{
		return BandTable->HungerBands;
	}

	static const FArcNeedBandTableFragment DefaultTable;
	return DefaultTable.HungerBands;
}

const TArray<FArcNeedBand>& FatigueBandsFor(const FArcNeedBandTableFragment* BandTable)
{
	if (BandTable)
	{
		return BandTable->FatigueBands;
	}

	static const FArcNeedBandTableFragment DefaultTable;
	return DefaultTable.FatigueBands;
}

void AdvanceNeed(FArcNeedFragment& Need, double Now, float SecondsPerGameHour, bool bForce)
{
	if (Need.LastUpdateTime <= 0.0)
	{
		Need.LastUpdateTime = Now;
		return;
	}

	const double Elapsed = Now - Need.LastUpdateTime;
	if (!bForce && Elapsed < EvaluateIntervalSeconds(SecondsPerGameHour))
	{
		return;
	}

	Need.CurrentValue = FMath::Clamp(
		Need.CurrentValue + Need.ChangeRate * static_cast<float>(Elapsed), 0.0f, NeedMaxValue);
	Need.LastUpdateTime = Now;
}

} // namespace ArcNeeds
