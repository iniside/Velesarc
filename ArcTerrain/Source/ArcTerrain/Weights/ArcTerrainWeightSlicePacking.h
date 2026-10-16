// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Budget of the per-cell weight slice set: how many channels one cell may carry into the GPU weight
 * atlas. The tile file itself is not bounded by it — it stores every present channel — so this is the
 * ranking cutoff the atlas fill applies, not a storage limit.
 */
namespace ArcTerrainWeights
{
	/** Bits per slice slot; the budget below is what they address. */
	inline constexpr int32 GArcTerrainWeightSlotNumBits = 5;

	inline constexpr int32 GArcTerrainWeightWordNumSlots = 32 / GArcTerrainWeightSlotNumBits; // 6

	inline constexpr int32 GArcTerrainWeightTableNumWords = 4;

	/** Maximum slices one cell may carry into the atlas. */
	inline constexpr int32 GArcTerrainWeightMaxSlices = GArcTerrainWeightTableNumWords * GArcTerrainWeightWordNumSlots; // 24
}
