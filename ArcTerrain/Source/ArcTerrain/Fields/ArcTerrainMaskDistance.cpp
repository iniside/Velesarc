// Copyright Lukasz Baran. All Rights Reserved.

#include "Fields/ArcTerrainMaskDistance.h"

namespace ArcTerrainMaskDistance
{
	double UnreachableSquaredDistance(const int32 Width, const int32 Height)
	{
		return static_cast<double>(Width) * Width + static_cast<double>(Height) * Height;
	}

	void BuildSquaredCellDistanceToMask(
		const TConstArrayView<uint8> Mask,
		const int32 Width,
		const int32 Height,
		TArray<double>& OutSquaredCellDistances)
	{
		const double Sentinel = UnreachableSquaredDistance(Width, Height);
		const int32 Unreachable = Width + Height + 1;
		OutSquaredCellDistances.Reset();
		OutSquaredCellDistances.SetNumUninitialized(Width * Height);

		TArray<int32> DownwardDistance;
		DownwardDistance.SetNumUninitialized(Height);
		for (int32 X = 0; X < Width; ++X)
		{
			int32 Running = Unreachable;
			for (int32 Y = 0; Y < Height; ++Y)
			{
				Running = Mask[X + Y * Width] != 0 ? 0 : FMath::Min(Running + 1, Unreachable);
				DownwardDistance[Y] = Running;
			}
			Running = Unreachable;
			for (int32 Y = Height - 1; Y >= 0; --Y)
			{
				Running = Mask[X + Y * Width] != 0 ? 0 : FMath::Min(Running + 1, Unreachable);
				const int32 Distance = FMath::Min(DownwardDistance[Y], Running);
				OutSquaredCellDistances[X + Y * Width] = Distance >= Unreachable
					? Sentinel
					: static_cast<double>(Distance) * Distance;
			}
		}

		const double BoundaryMin = -TNumericLimits<double>::Max();
		const double BoundaryMax = TNumericLimits<double>::Max();
		TArray<double> RowSource;
		TArray<int32> ParabolaIndex;
		TArray<double> ParabolaBoundary;
		RowSource.SetNumUninitialized(Width);
		ParabolaIndex.SetNumUninitialized(Width);
		ParabolaBoundary.SetNumUninitialized(Width + 1);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				RowSource[X] = OutSquaredCellDistances[X + Y * Width];
			}
			int32 Top = 0;
			ParabolaIndex[0] = 0;
			ParabolaBoundary[0] = BoundaryMin;
			ParabolaBoundary[1] = BoundaryMax;
			for (int32 X = 1; X < Width; ++X)
			{
				double Intersection = 0.0;
				while (true)
				{
					const int32 Previous = ParabolaIndex[Top];
					Intersection =
						((RowSource[X] + static_cast<double>(X) * X)
							- (RowSource[Previous] + static_cast<double>(Previous) * Previous))
						/ (2.0 * static_cast<double>(X) - 2.0 * static_cast<double>(Previous));
					if (Intersection > ParabolaBoundary[Top])
					{
						break;
					}
					--Top;
				}
				++Top;
				ParabolaIndex[Top] = X;
				ParabolaBoundary[Top] = Intersection;
				ParabolaBoundary[Top + 1] = BoundaryMax;
			}
			Top = 0;
			for (int32 X = 0; X < Width; ++X)
			{
				while (ParabolaBoundary[Top + 1] < static_cast<double>(X))
				{
					++Top;
				}
				const int32 Source = ParabolaIndex[Top];
				const double Offset = static_cast<double>(X) - static_cast<double>(Source);
				OutSquaredCellDistances[X + Y * Width] = Offset * Offset + RowSource[Source];
			}
		}
	}
}
