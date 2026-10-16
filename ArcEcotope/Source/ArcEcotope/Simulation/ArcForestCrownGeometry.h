// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

/**
 * Vertical crown geometry: where a crown sits in an absolute height column, and how much of a column
 * a receiver at a given height stands under.
 *
 * A crown's area is DISTRIBUTED over its depth, never replicated into each stratum it spans, so a
 * deep crown intercepts the same total as a shallow crown of equal radius: total leaf area is tied to
 * the crown's pi*r^2 alone and depth only redistributes it vertically. That is a deliberate
 * simplification - physically a deeper crown carries more leaf area - and it means
 * CrownRadiusHeightRatio now governs vertical crown geometry as well as horizontal.
 */
namespace UE::ArcForest::Crown
{
	/** Against a ~35 m column this is ~1.1 m per stratum, fine enough to separate the sub-7 m bulk of the stand. */
	constexpr int32 DefaultCanopyStratumCount = 32;

	/** Twice the crown radius, clamped to the stem height so a sapling's crown cannot start below ground. */
	ARCECOTOPE_API double CalculateCrownDepthCm(const double HeightCm, const double CrownRadiusCm);

	/**
	 * Fraction of the crown's area deposited into each stratum, summing to 1 over OutWeights.
	 *
	 * A crown reaching above the column clamps into the top stratum; a zero-depth crown deposits
	 * entirely into the stratum holding its top; a non-positive StratumHeightCm deposits into the
	 * top stratum, which SumOccupancyAbove then always counts.
	 */
	ARCECOTOPE_API void CalculateCrownStratumWeights(
		const double CrownBaseCm,
		const double CrownTopCm,
		const double StratumHeightCm,
		TArrayView<double> OutWeights);

	/**
	 * Occupancy standing above HeightCm: the strata fully above it, plus the part of its own stratum
	 * that lies above it, taken as a linear ramp within the stratum.
	 *
	 * A non-positive StratumHeightCm yields the whole column, which is the pre-stratification value.
	 */
	ARCECOTOPE_API double SumOccupancyAbove(
		const TArrayView<const double> CellStrata,
		const double HeightCm,
		const double StratumHeightCm);

	/**
	 * Crown occupancy of every record cell, resolved into strata that partition [0, ColumnTopCm], so a
	 * stratum index names the same absolute height in every cell and for every species.
	 */
	struct ARCECOTOPE_API FStratifiedOccupancy
	{
		/**
		 * ColumnTopCm is the tallest frozen species' MaxHeightCm and must stay fixed for the whole run:
		 * it is what ties a stratum index to a height, and the field carries no record of the top it
		 * was filled under.
		 */
		void Reset(const int32 InCellCount, const int32 InStratumCount, const double InColumnTopCm);

		int32 GetCellCount() const
		{
			return CellCount;
		}

		int32 GetStratumCount() const
		{
			return StratumCount;
		}

		double GetColumnTopCm() const
		{
			return ColumnTopCm;
		}

		double GetStratumHeightCm() const
		{
			return StratumHeightCm;
		}

		bool IsValidCell(const int32 CellIndex) const
		{
			return CellIndex >= 0 && CellIndex < CellCount;
		}

		TArrayView<double> GetCellStrata(const int32 CellIndex);
		TArrayView<const double> GetCellStrata(const int32 CellIndex) const;

		/** O(StratumCount); hoist it out of any loop that runs over more than the record cells. */
		double TotalAt(const int32 CellIndex) const;

		double AboveHeight(const int32 CellIndex, const double HeightCm) const;

	private:
		TArray<double> Strata;
		int32 CellCount = 0;
		int32 StratumCount = 0;
		double ColumnTopCm = 0.0;
		double StratumHeightCm = 0.0;
	};
}
