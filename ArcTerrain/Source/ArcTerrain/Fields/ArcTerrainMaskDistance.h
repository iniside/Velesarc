// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ArcTerrainMaskDistance
{
	// Squared cell distances stay double: the lower-envelope intersection divides by 2*(X - Previous),
	// which loses cells to float rounding on a large grid.
	ARCTERRAIN_API void BuildSquaredCellDistanceToMask(
		const TConstArrayView<uint8> Mask,
		const int32 Width,
		const int32 Height,
		TArray<double>& OutSquaredCellDistances);

	ARCTERRAIN_API double UnreachableSquaredDistance(const int32 Width, const int32 Height);
}
