// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

/**
 * Heights over a site grid grown by MarginCells on every side, in metres, row-major. Every cell centre,
 * site or margin, sits on one uniform lattice, so CellSizeM is the true separation of every neighbour
 * pair. The solve runs over the whole margined grid, so a site cell's contributing area includes terrain
 * outside the site.
 */
struct FArcHydrologyGrid
{
	/** SiteDimensions + 2 * MarginCells on both axes. */
	FIntPoint Dimensions = FIntPoint::ZeroValue;

	FIntPoint SiteDimensions = FIntPoint::ZeroValue;

	int32 MarginCells = 0;

	double CellSizeM = 0.0;

	/** Row-major Dimensions.X * Dimensions.Y. The caller owns the storage for the duration of the solve. */
	TConstArrayView<double> HeightMeters;
};

struct FArcHydrologySettings
{
	/** Epsilon step of the depression fill, and the bucket width of the queue the fill is ordered by. */
	double ElevationQuantumM = 0.0;
};

/** Every field is one value per margined grid cell in row-major order. */
struct FArcHydrologySolution
{
	/** Sampled heights raised by the epsilon fill so no interior cell is a pit. */
	TArray<double> FilledHeights;

	/** Cells in the order the fill dequeued them: non-decreasing in FilledHeights, so its reverse is topological. */
	TArray<int32> TraversalOrder;

	/** Horn slope magnitude of FilledHeights, dimensionless. */
	TArray<float> TangentBeta;

	/** Steepest downslope neighbour, INDEX_NONE where no neighbour is strictly lower. */
	TArray<int32> D8Receiver;

	/** Channel cell each cell's receiver chain reaches, INDEX_NONE where the chain ends before one. */
	TArray<int32> HandBase;

	int32 FilledCellCount = 0;

	/** Cells the fill left level because another epsilon step would pass the elevation its run started from. */
	int32 SaturatedFlatCellCount = 0;

	int32 ChannelCellCount = 0;
};

/**
 * Grid hydrology over plain arrays in metres: no UObject, no world, no RNG. Every stage after
 * PriorityFloodEpsilon consumes that stage's outputs unchecked, so callers running the stages by hand
 * must pass them through in order; Solve does exactly that.
 */
namespace ArcTerrainHydrology
{
	ARCTERRAIN_API bool ValidateGrid(const FArcHydrologyGrid& Grid, int32& OutCellCount, FText& OutError);

	/** Priority flood with an epsilon gradient. OutTraversalOrder holds every cell exactly once on success. */
	ARCTERRAIN_API bool PriorityFloodEpsilon(
		const FArcHydrologyGrid& Grid,
		const FArcHydrologySettings& Settings,
		TArray<double>& OutFilledHeights,
		TArray<int32>& OutTraversalOrder,
		int32& OutFilledCellCount,
		int32& OutSaturatedFlatCellCount,
		FText& OutError);

	ARCTERRAIN_API void ComputeTangentBeta(
		const FArcHydrologyGrid& Grid,
		TConstArrayView<double> FilledHeights,
		TArray<float>& OutTangentBeta);

	ARCTERRAIN_API void ComputeD8Receivers(
		const FArcHydrologyGrid& Grid,
		TConstArrayView<double> FilledHeights,
		TConstArrayView<int32> TraversalOrder,
		TArray<int32>& OutD8Receiver);

	ARCTERRAIN_API void ComputeHAND(
		TConstArrayView<uint8> ChannelMask,
		TConstArrayView<int32> D8Receiver,
		TArray<int32>& OutHandBase);

	/**
	 * Accumulation is the contributing area in square metres at every margined cell and must be exactly
	 * one entry per cell. ChannelMask is either empty or one entry per cell; empty means the caller has
	 * no channel network, and the solution's HandBase then resolves nowhere.
	 */
	ARCTERRAIN_API bool Solve(
		const FArcHydrologyGrid& Grid,
		const FArcHydrologySettings& Settings,
		TConstArrayView<double> Accumulation,
		TConstArrayView<uint8> ChannelMask,
		FArcHydrologySolution& OutSolution,
		FText& OutError);
}
