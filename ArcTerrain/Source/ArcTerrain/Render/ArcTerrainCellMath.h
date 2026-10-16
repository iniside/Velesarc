// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ArcTerrainCellMath
{
	/**
	 * The one production texel -> terrain-local scale: one double division, then ONE narrowing
	 * conversion. The GPU multiplies its canonical integer-to-DoubleFloat global texel index by this
	 * exact float, so any CPU mirror of a vertex position must use this function rather than
	 * re-deriving CellSize / Quads.
	 */
	inline float CellTexelToLocal(double CellSize, int32 NumQuads)
	{
		return static_cast<float>(CellSize / static_cast<double>(FMath::Max(NumQuads, 1)));
	}
}
