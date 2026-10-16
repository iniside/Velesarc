// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AArcTerrain;
class UWorld;

namespace UE::ArcEcotope
{
	/** Terrain a volume resolves against, plus the terms that ranked it. */
	struct FArcEcotopeTerrainChoice
	{
		AArcTerrain* Terrain = nullptr;
		/** |Z| of the volume centre in the selected terrain's local frame, in cm. */
		double VolumeCenterHeightCm = 0.0;
		int64 VolumeBuiltCellCount = 0;
		bool bVolumeCountSampled = false;
	};

	/** True when a vertical line in volume-local space is also vertical in terrain-local space. */
	ARCECOTOPE_API bool IsVerticalAxisAligned(const FTransform& ActorTransform, const FTransform& TerrainTransform);

	/** Inclusive terrain-cell rectangle covering a world AABB, the cell count it spans, and optionally the
	 *  AABB in the terrain's local frame, which the caller would otherwise re-derive and re-validate. */
	ARCECOTOPE_API bool TryComputeTerrainCellRect(
		const FBox& WorldBounds,
		const FTransform& TerrainTransform,
		double CellSize,
		FIntPoint& OutMinCell,
		FIntPoint& OutMaxCell,
		int64& OutCellCount,
		FBox* OutLocalBounds = nullptr);

	/**
	 * Number of terrain cells inside an inclusive rectangle that carry a built section. A rectangle
	 * wider than the sample budget on either axis is probed on a stride and the hits are scaled back to
	 * cell units, which bounds the lookups at the cost of an estimate; bOutSampled reports that trade.
	 */
	ARCECOTOPE_API int64 CountBuiltTerrainCells(
		const AArcTerrain& Terrain,
		const FIntPoint& MinCell,
		const FIntPoint& MaxCell,
		bool& bOutSampled);

	/**
	 * Every ArcTerrain is an unbounded plane, so the discriminator is which plane the volume sits on:
	 * the terrain whose local frame puts the volume centre closest to Z=0 wins, built cells under the
	 * volume break the tie and the path name settles the rest, so a world holding several overlapping
	 * terrains always resolves to the same one.
	 */
	ARCECOTOPE_API FArcEcotopeTerrainChoice SelectTerrainForVolume(
		UWorld& World,
		const FBox& WorldVolumeBounds,
		const FBox& WorldSampleBounds,
		const FTransform& ActorTransform);
}
