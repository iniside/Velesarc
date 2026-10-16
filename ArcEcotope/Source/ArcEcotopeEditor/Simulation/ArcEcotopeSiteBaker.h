// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ArcEcotopeSiteChunkStorage.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"

class AArcEcotopeVolume;

struct ARCECOTOPEEDITOR_API FArcEcotopeSiteBakeResult
{
	/** Final writer metadata for the authoritative tiled producer. */
	FArcEcotopeSiteMetadata Metadata;

	FArcEcotopeSiteChunkManifest ChunkManifest;
	TArray<FSharedBuffer> ChunkPayloads;
};

/**
 * Volume-local terrain heights over the site grid grown by MarginCells on every side, the input every
 * site field generator reads. Every cell centre, site or margin, sits on the uniform lattice
 * LocalBounds.Min + (i + 0.5) * CellSizeCm, so CellSizeM is the true separation of every neighbour pair;
 * the snapshot's own trailing-cell centres are max-edge clamped and are not on this lattice. Metres
 * throughout: the bake converts centimetres exactly once, into this grid.
 */
struct ARCECOTOPEEDITOR_API FArcEcotopeMarginedHeightGrid
{
	/** SiteDimensions + 2 * MarginCells on both axes. */
	FIntPoint Dimensions = FIntPoint::ZeroValue;

	FIntPoint SiteDimensions = FIntPoint::ZeroValue;

	int32 MarginCells = 0;

	double CellSizeM = 0.0;

	/** Row-major Dimensions.X * Dimensions.Y. */
	TArray<double> HeightMeters;

	/** Site cell coordinates, negative or past SiteDimensions inside the margin. Callers must know the cell is on the grid. */
	int32 GetIndexFromSiteCell(const int32 SiteX, const int32 SiteY) const
	{
		return (SiteY + MarginCells) * Dimensions.X + (SiteX + MarginCells);
	}

	bool TryGetIndexFromSiteCell(const int32 SiteX, const int32 SiteY, int32& OutIndex) const
	{
		const int32 MarginedX = SiteX + MarginCells;
		const int32 MarginedY = SiteY + MarginCells;
		if (MarginedX < 0 || MarginedX >= Dimensions.X || MarginedY < 0 || MarginedY >= Dimensions.Y)
		{
			OutIndex = INDEX_NONE;
			return false;
		}

		OutIndex = MarginedY * Dimensions.X + MarginedX;
		return true;
	}
};

/** Editor-only game-thread terrain sampling for ArcEcotope volumes. */
class ARCECOTOPEEDITOR_API FArcEcotopeSiteBaker
{
public:
	/**
	 * Produces authoritative chunk-only metadata and payloads. OutResult is left unchanged on failure.
	 */
	static bool Bake(AArcEcotopeVolume& Volume, FArcEcotopeSiteBakeResult& OutResult, FText& OutError);
};
