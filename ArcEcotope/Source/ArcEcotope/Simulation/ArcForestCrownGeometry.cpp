// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestCrownGeometry.h"

namespace ArcForestCrownGeometryPrivate
{
	int32 ResolveStratumIndex(const double HeightCm, const double StratumHeightCm, const int32 StratumCount)
	{
		return FMath::Clamp(FMath::FloorToInt32(HeightCm / StratumHeightCm), 0, StratumCount - 1);
	}
}

namespace UE::ArcForest::Crown
{
	double CalculateCrownDepthCm(const double HeightCm, const double CrownRadiusCm)
	{
		const double MaximumDepthCm = FMath::Max(HeightCm, 0.0);
		return FMath::Clamp(2.0 * CrownRadiusCm, 0.0, MaximumDepthCm);
	}

	void CalculateCrownStratumWeights(
		const double CrownBaseCm,
		const double CrownTopCm,
		const double StratumHeightCm,
		TArrayView<double> OutWeights)
	{
		const int32 StratumCount = OutWeights.Num();
		if (StratumCount <= 0)
		{
			return;
		}

		for (int32 StratumIndex = 0; StratumIndex < StratumCount; ++StratumIndex)
		{
			OutWeights[StratumIndex] = 0.0;
		}

		if (!(StratumHeightCm > 0.0))
		{
			OutWeights[StratumCount - 1] = 1.0;
			return;
		}

		const double ColumnTopCm = static_cast<double>(StratumCount) * StratumHeightCm;
		const double ClampedTopCm = FMath::Clamp(CrownTopCm, 0.0, ColumnTopCm);
		const double ClampedBaseCm = FMath::Clamp(CrownBaseCm, 0.0, ClampedTopCm);
		const double DepthCm = ClampedTopCm - ClampedBaseCm;

		if (!(DepthCm > 0.0))
		{
			OutWeights[ArcForestCrownGeometryPrivate::ResolveStratumIndex(
				ClampedTopCm, StratumHeightCm, StratumCount)] = 1.0;
			return;
		}

		const int32 FirstStratumIndex =
			ArcForestCrownGeometryPrivate::ResolveStratumIndex(ClampedBaseCm, StratumHeightCm, StratumCount);
		for (int32 StratumIndex = FirstStratumIndex; StratumIndex < StratumCount; ++StratumIndex)
		{
			const double StratumBottomCm = static_cast<double>(StratumIndex) * StratumHeightCm;
			if (StratumBottomCm >= ClampedTopCm)
			{
				break;
			}

			const double OverlapCm = FMath::Min(StratumBottomCm + StratumHeightCm, ClampedTopCm)
				- FMath::Max(StratumBottomCm, ClampedBaseCm);
			if (OverlapCm > 0.0)
			{
				OutWeights[StratumIndex] = OverlapCm / DepthCm;
			}
		}
	}

	double SumOccupancyAbove(
		const TArrayView<const double> CellStrata,
		const double HeightCm,
		const double StratumHeightCm)
	{
		const int32 StratumCount = CellStrata.Num();
		if (StratumCount <= 0)
		{
			return 0.0;
		}

		double Sum = 0.0;
		if (!(StratumHeightCm > 0.0) || !(HeightCm > 0.0))
		{
			for (int32 StratumIndex = 0; StratumIndex < StratumCount; ++StratumIndex)
			{
				Sum += CellStrata[StratumIndex];
			}
			return Sum;
		}

		const double ColumnTopCm = static_cast<double>(StratumCount) * StratumHeightCm;
		if (HeightCm >= ColumnTopCm)
		{
			return 0.0;
		}

		const int32 ReceiverStratumIndex =
			ArcForestCrownGeometryPrivate::ResolveStratumIndex(HeightCm, StratumHeightCm, StratumCount);
		const double ReceiverStratumTopCm =
			(static_cast<double>(ReceiverStratumIndex) + 1.0) * StratumHeightCm;
		Sum = ((ReceiverStratumTopCm - HeightCm) / StratumHeightCm) * CellStrata[ReceiverStratumIndex];

		for (int32 StratumIndex = ReceiverStratumIndex + 1; StratumIndex < StratumCount; ++StratumIndex)
		{
			Sum += CellStrata[StratumIndex];
		}
		return Sum;
	}

	void FStratifiedOccupancy::Reset(
		const int32 InCellCount,
		const int32 InStratumCount,
		const double InColumnTopCm)
	{
		CellCount = FMath::Max(InCellCount, 0);
		StratumCount = FMath::Max(InStratumCount, 1);
		ColumnTopCm = FMath::Max(InColumnTopCm, 0.0);
		StratumHeightCm = ColumnTopCm / static_cast<double>(StratumCount);
		Strata.Reset();
		Strata.SetNumZeroed(CellCount * StratumCount);
	}

	TArrayView<double> FStratifiedOccupancy::GetCellStrata(const int32 CellIndex)
	{
		check(IsValidCell(CellIndex));
		return TArrayView<double>(Strata.GetData() + CellIndex * StratumCount, StratumCount);
	}

	TArrayView<const double> FStratifiedOccupancy::GetCellStrata(const int32 CellIndex) const
	{
		check(IsValidCell(CellIndex));
		return TArrayView<const double>(Strata.GetData() + CellIndex * StratumCount, StratumCount);
	}

	double FStratifiedOccupancy::TotalAt(const int32 CellIndex) const
	{
		if (!IsValidCell(CellIndex))
		{
			return 0.0;
		}
		const double* CellStrata = Strata.GetData() + CellIndex * StratumCount;
		double Sum = 0.0;
		for (int32 StratumIndex = 0; StratumIndex < StratumCount; ++StratumIndex)
		{
			Sum += CellStrata[StratumIndex];
		}
		return Sum;
	}

	double FStratifiedOccupancy::AboveHeight(const int32 CellIndex, const double HeightCm) const
	{
		if (!IsValidCell(CellIndex))
		{
			return 0.0;
		}
		return SumOccupancyAbove(GetCellStrata(CellIndex), HeightCm, StratumHeightCm);
	}
}
