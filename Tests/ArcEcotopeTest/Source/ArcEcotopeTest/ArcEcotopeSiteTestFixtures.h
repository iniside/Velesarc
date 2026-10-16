// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Simulation/ArcEcotopeSiteBaker.h"
#endif

namespace ArcEcotopeSiteTestFixtures
{
#if WITH_EDITOR
	inline FArcEcotopeMarginedHeightGrid MakeHeightGrid(
		const int32 SiteX,
		const int32 SiteY,
		const int32 MarginCells,
		const double CellSizeM,
		const double InitialHeightM)
	{
		FArcEcotopeMarginedHeightGrid HeightGrid;
		HeightGrid.SiteDimensions = FIntPoint(SiteX, SiteY);
		HeightGrid.MarginCells = MarginCells;
		HeightGrid.Dimensions = FIntPoint(SiteX + 2 * MarginCells, SiteY + 2 * MarginCells);
		HeightGrid.CellSizeM = CellSizeM;
		HeightGrid.HeightMeters.Init(InitialHeightM, HeightGrid.Dimensions.X * HeightGrid.Dimensions.Y);
		return HeightGrid;
	}
#endif
}
