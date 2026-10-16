// Copyright Lukasz Baran. All Rights Reserved.

#include "Hydrology/ArcTerrainHydrology.h"

#include "Algo/Sort.h"

namespace ArcTerrainHydrologyPrivate
{
	constexpr int32 NeighbourCount = 8;

	/** E, NE, N, NW, W, SW, S, SE. Every neighbourhood walk uses this order, so both the fill's
	    enqueue sequence and the accumulation's summation order are reproducible. */
	constexpr int32 NeighbourOffsetX[NeighbourCount] = { 1, 1, 0, -1, -1, -1, 0, 1 };
	constexpr int32 NeighbourOffsetY[NeighbourCount] = { 0, 1, 1, 1, 0, -1, -1, -1 };

	/** Cap on the bucket count, so relief divided by the elevation quantum cannot allocate unbounded. */
	constexpr int64 MaxBucketCount = 1 << 23;

	int32 GetBucketOfHeight(const double HeightM, const double MinHeightM, const double QuantumM, const int32 BucketCount)
	{
		return FMath::Clamp(FMath::FloorToInt32((HeightM - MinHeightM) / QuantumM), 0, BucketCount - 1);
	}

	double GetNeighbourDistanceM(const int32 Direction, const double CellSizeM)
	{
		return (NeighbourOffsetX[Direction] != 0 && NeighbourOffsetY[Direction] != 0)
			? CellSizeM * UE_DOUBLE_SQRT_2
			: CellSizeM;
	}

	double SampleClamped(const TConstArrayView<double> Heights, const int32 Width, const int32 Height,
		const int32 CellX, const int32 CellY)
	{
		const int32 ClampedX = FMath::Clamp(CellX, 0, Width - 1);
		const int32 ClampedY = FMath::Clamp(CellY, 0, Height - 1);
		return Heights[ClampedY * Width + ClampedX];
	}

	double ComputeHornTangentBeta(const TConstArrayView<double> Heights, const int32 Width, const int32 Height,
		const int32 CellX, const int32 CellY, const double CellSizeM)
	{
		const double NorthWest = SampleClamped(Heights, Width, Height, CellX - 1, CellY + 1);
		const double North = SampleClamped(Heights, Width, Height, CellX, CellY + 1);
		const double NorthEast = SampleClamped(Heights, Width, Height, CellX + 1, CellY + 1);
		const double West = SampleClamped(Heights, Width, Height, CellX - 1, CellY);
		const double East = SampleClamped(Heights, Width, Height, CellX + 1, CellY);
		const double SouthWest = SampleClamped(Heights, Width, Height, CellX - 1, CellY - 1);
		const double South = SampleClamped(Heights, Width, Height, CellX, CellY - 1);
		const double SouthEast = SampleClamped(Heights, Width, Height, CellX + 1, CellY - 1);

		const double GradientX = ((NorthEast + 2.0 * East + SouthEast) - (NorthWest + 2.0 * West + SouthWest))
			/ (8.0 * CellSizeM);
		const double GradientY = ((NorthWest + 2.0 * North + NorthEast) - (SouthWest + 2.0 * South + SouthEast))
			/ (8.0 * CellSizeM);
		return FMath::Sqrt(GradientX * GradientX + GradientY * GradientY);
	}

	struct FBucketQueue
	{
		TArray<int32> Head;
		TArray<int32> Tail;
		TArray<int32> Next;

		void Initialize(const int32 BucketCount, const int32 CellCount)
		{
			Head.Init(INDEX_NONE, BucketCount);
			Tail.Init(INDEX_NONE, BucketCount);
			Next.Init(INDEX_NONE, CellCount);
		}

		void Push(const int32 Bucket, const int32 CellIndex)
		{
			Next[CellIndex] = INDEX_NONE;
			if (Head[Bucket] == INDEX_NONE)
			{
				Head[Bucket] = CellIndex;
			}
			else
			{
				Next[Tail[Bucket]] = CellIndex;
			}
			Tail[Bucket] = CellIndex;
		}
	};

	bool SetError(FText& OutError, const FString& Message)
	{
		OutError = FText::FromString(Message);
		return false;
	}
}

bool ArcTerrainHydrology::ValidateGrid(const FArcHydrologyGrid& Grid, int32& OutCellCount, FText& OutError)
{
	using namespace ArcTerrainHydrologyPrivate;

	OutCellCount = 0;

	const int32 Width = Grid.Dimensions.X;
	const int32 Height = Grid.Dimensions.Y;
	const int32 SiteWidth = Grid.SiteDimensions.X;
	const int32 SiteHeight = Grid.SiteDimensions.Y;

	if (Width < 3 || Height < 3 || SiteWidth <= 0 || SiteHeight <= 0
		|| Grid.MarginCells < 0
		|| SiteWidth + 2 * Grid.MarginCells != Width
		|| SiteHeight + 2 * Grid.MarginCells != Height)
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcEcotope moisture needs a margined height grid of at least 3x3 cells matching its site dimensions; got %dx%d over a %dx%d site with a %d cell margin."),
			Width, Height, SiteWidth, SiteHeight, Grid.MarginCells));
	}

	const int64 CellCount64 = static_cast<int64>(Width) * static_cast<int64>(Height);
	if (CellCount64 > MAX_int32 || Grid.HeightMeters.Num() != static_cast<int32>(CellCount64))
	{
		return SetError(OutError, TEXT("ArcEcotope moisture height grid does not match its declared dimensions."));
	}

	if (!FMath::IsFinite(Grid.CellSizeM) || Grid.CellSizeM <= 0.0)
	{
		return SetError(OutError, TEXT("ArcEcotope moisture requires a finite positive cell size in metres."));
	}

	OutCellCount = static_cast<int32>(CellCount64);
	OutError = FText::GetEmpty();
	return true;
}

bool ArcTerrainHydrology::PriorityFloodEpsilon(
	const FArcHydrologyGrid& Grid,
	const FArcHydrologySettings& Settings,
	TArray<double>& OutFilledHeights,
	TArray<int32>& OutTraversalOrder,
	int32& OutFilledCellCount,
	int32& OutSaturatedFlatCellCount,
	FText& OutError)
{
	using namespace ArcTerrainHydrologyPrivate;

	OutFilledHeights.Reset();
	OutTraversalOrder.Reset();
	OutFilledCellCount = 0;
	OutSaturatedFlatCellCount = 0;

	int32 CellCount = 0;
	if (!ValidateGrid(Grid, CellCount, OutError))
	{
		return false;
	}

	const int32 Width = Grid.Dimensions.X;
	const int32 Height = Grid.Dimensions.Y;

	const double QuantumM = Settings.ElevationQuantumM;
	if (!FMath::IsFinite(QuantumM) || QuantumM <= 0.0)
	{
		return SetError(OutError, TEXT("ArcTerrain hydrology requires a finite positive ElevationQuantumM."));
	}

	const TConstArrayView<double> SampledHeights = Grid.HeightMeters;
	double MinHeightM = SampledHeights[0];
	double MaxHeightM = SampledHeights[0];
	for (const double HeightM : SampledHeights)
	{
		if (!FMath::IsFinite(HeightM))
		{
			return SetError(OutError, TEXT("ArcEcotope moisture height grid contains a non-finite height."));
		}
		MinHeightM = FMath::Min(MinHeightM, HeightM);
		MaxHeightM = FMath::Max(MaxHeightM, HeightM);
	}

	const double ReliefM = MaxHeightM - MinHeightM;
	const double BucketSpan = ReliefM / QuantumM;
	if (!FMath::IsFinite(BucketSpan) || BucketSpan > static_cast<double>(MaxBucketCount - 2))
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcEcotope moisture needs %.0f elevation quanta to span %.1f m of relief, above the limit of %lld; raise ElevationQuantumM."),
			BucketSpan, ReliefM, MaxBucketCount - 2));
	}
	const int32 BucketCount = FMath::FloorToInt32(BucketSpan) + 2;

	TArray<double>& FilledHeights = OutFilledHeights;
	FilledHeights.Init(0.0, CellCount);

	// Elevation an epsilon run may not pass: one quantum above the real ground the run started from,
	// which on a grid quantised to that step is the next elevation it can hold. Max means the cell
	// carries its own sampled height and any run leaving it starts a fresh base.
	TArray<double> RunCeilingM;
	RunCeilingM.Init(TNumericLimits<double>::Max(), CellCount);

	TArray<uint8> Visited;
	Visited.Init(0, CellCount);
	TArray<int32>& TraversalOrder = OutTraversalOrder;
	TraversalOrder.Reserve(CellCount);

	FBucketQueue Queue;
	Queue.Initialize(BucketCount, CellCount);

	for (int32 CellY = 0; CellY < Height; ++CellY)
	{
		const bool bBorderRow = CellY == 0 || CellY == Height - 1;
		for (int32 CellX = 0; CellX < Width; ++CellX)
		{
			if (!bBorderRow && CellX != 0 && CellX != Width - 1)
			{
				continue;
			}

			const int32 CellIndex = CellY * Width + CellX;
			FilledHeights[CellIndex] = SampledHeights[CellIndex];
			Visited[CellIndex] = 1;
			Queue.Push(GetBucketOfHeight(FilledHeights[CellIndex], MinHeightM, QuantumM, BucketCount), CellIndex);
		}
	}

	int32 SaturatedFlatCellCount = 0;
	for (int32 Bucket = 0; Bucket < BucketCount; ++Bucket)
	{
		const int32 SegmentStart = TraversalOrder.Num();
		int32 CellIndex = Queue.Head[Bucket];
		while (CellIndex != INDEX_NONE)
		{
			TraversalOrder.Add(CellIndex);

			const int32 CellX = CellIndex % Width;
			const int32 CellY = CellIndex / Width;
			for (int32 Direction = 0; Direction < NeighbourCount; ++Direction)
			{
				const int32 NeighbourX = CellX + NeighbourOffsetX[Direction];
				const int32 NeighbourY = CellY + NeighbourOffsetY[Direction];
				if (NeighbourX < 0 || NeighbourX >= Width || NeighbourY < 0 || NeighbourY >= Height)
				{
					continue;
				}

				const int32 NeighbourIndex = NeighbourY * Width + NeighbourX;
				if (Visited[NeighbourIndex] != 0)
				{
					continue;
				}

				const double ProposedM = FilledHeights[CellIndex] + QuantumM;
				double AssignedM = 0.0;
				if (SampledHeights[NeighbourIndex] >= ProposedM)
				{
					AssignedM = SampledHeights[NeighbourIndex];
				}
				else if (ProposedM > RunCeilingM[CellIndex])
				{
					AssignedM = FMath::Max(SampledHeights[NeighbourIndex], FilledHeights[CellIndex]);
					++SaturatedFlatCellCount;
				}
				else
				{
					AssignedM = ProposedM;
				}

				Visited[NeighbourIndex] = 1;
				FilledHeights[NeighbourIndex] = AssignedM;
				RunCeilingM[NeighbourIndex] = AssignedM > SampledHeights[NeighbourIndex]
					? FMath::Min(RunCeilingM[CellIndex], ProposedM)
					: TNumericLimits<double>::Max();
				Queue.Push(GetBucketOfHeight(AssignedM, MinHeightM, QuantumM, BucketCount), NeighbourIndex);
			}

			CellIndex = Queue.Next[CellIndex];
		}

		// Cells sharing a bucket still differ in exact height, and one can drain into another; without
		// this sort the reversed order is not topological and the lower cell loses its upslope area.
		const int32 SegmentLength = TraversalOrder.Num() - SegmentStart;
		if (SegmentLength > 1)
		{
			const TArrayView<int32> Segment(TraversalOrder.GetData() + SegmentStart, SegmentLength);
			Algo::Sort(Segment, [&FilledHeights](const int32 Left, const int32 Right) -> bool
			{
				return FilledHeights[Left] != FilledHeights[Right]
					? FilledHeights[Left] < FilledHeights[Right]
					: Left < Right;
			});
		}
	}

	if (TraversalOrder.Num() != CellCount)
	{
		return SetError(OutError, TEXT("ArcEcotope moisture depression fill did not reach every height grid cell."));
	}

	int32 FilledCellCount = 0;
	for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
	{
		if (FilledHeights[CellIndex] > SampledHeights[CellIndex])
		{
			++FilledCellCount;
		}
	}

	OutFilledCellCount = FilledCellCount;
	OutSaturatedFlatCellCount = SaturatedFlatCellCount;
	OutError = FText::GetEmpty();
	return true;
}

void ArcTerrainHydrology::ComputeTangentBeta(
	const FArcHydrologyGrid& Grid,
	const TConstArrayView<double> FilledHeights,
	TArray<float>& OutTangentBeta)
{
	using namespace ArcTerrainHydrologyPrivate;

	const int32 Width = Grid.Dimensions.X;
	const int32 Height = Grid.Dimensions.Y;

	OutTangentBeta.Reset();
	OutTangentBeta.Init(0.0f, FilledHeights.Num());
	for (int32 CellY = 0; CellY < Height; ++CellY)
	{
		for (int32 CellX = 0; CellX < Width; ++CellX)
		{
			OutTangentBeta[CellY * Width + CellX] = static_cast<float>(
				ComputeHornTangentBeta(FilledHeights, Width, Height, CellX, CellY, Grid.CellSizeM));
		}
	}
}

void ArcTerrainHydrology::ComputeD8Receivers(
	const FArcHydrologyGrid& Grid,
	const TConstArrayView<double> FilledHeights,
	const TConstArrayView<int32> TraversalOrder,
	TArray<int32>& OutD8Receiver)
{
	using namespace ArcTerrainHydrologyPrivate;

	const int32 Width = Grid.Dimensions.X;
	const int32 Height = Grid.Dimensions.Y;
	const int32 CellCount = TraversalOrder.Num();
	const double CellSizeM = Grid.CellSizeM;

	OutD8Receiver.Reset();
	OutD8Receiver.Init(INDEX_NONE, CellCount);

	for (int32 OrderIndex = CellCount - 1; OrderIndex >= 0; --OrderIndex)
	{
		const int32 CellIndex = TraversalOrder[OrderIndex];
		const int32 CellX = CellIndex % Width;
		const int32 CellY = CellIndex / Width;

		int32 SteepestReceiver = INDEX_NONE;
		double SteepestSlope = 0.0;
		int32 FlatReceiver = INDEX_NONE;

		for (int32 Direction = 0; Direction < NeighbourCount; ++Direction)
		{
			const int32 NeighbourX = CellX + NeighbourOffsetX[Direction];
			const int32 NeighbourY = CellY + NeighbourOffsetY[Direction];
			if (NeighbourX < 0 || NeighbourX >= Width || NeighbourY < 0 || NeighbourY >= Height)
			{
				continue;
			}

			const int32 NeighbourIndex = NeighbourY * Width + NeighbourX;
			const double DropM = FilledHeights[CellIndex] - FilledHeights[NeighbourIndex];
			if (DropM <= 0.0)
			{
				// Equal height puts the neighbour in this cell's own bucket segment, which the sort
				// orders by index, so a lower-indexed one is strictly earlier in the reversed sweep.
				if (DropM == 0.0 && NeighbourIndex < CellIndex
					&& (FlatReceiver == INDEX_NONE || NeighbourIndex < FlatReceiver))
				{
					FlatReceiver = NeighbourIndex;
				}
				continue;
			}

			const double DirectionalSlope = DropM / GetNeighbourDistanceM(Direction, CellSizeM);
			if (DirectionalSlope > SteepestSlope)
			{
				SteepestSlope = DirectionalSlope;
				SteepestReceiver = NeighbourIndex;
			}
		}

		if (SteepestReceiver != INDEX_NONE)
		{
			OutD8Receiver[CellIndex] = SteepestReceiver;
			continue;
		}

		// The grid border is an outlet; a saturated flat is an interior dead end, so it hands its
		// flow across the flat instead of ending the chain there.
		const bool bBorderCell = CellX == 0 || CellY == 0 || CellX == Width - 1 || CellY == Height - 1;
		if (!bBorderCell && FlatReceiver != INDEX_NONE)
		{
			OutD8Receiver[CellIndex] = FlatReceiver;
		}
	}
}

void ArcTerrainHydrology::ComputeHAND(
	const TConstArrayView<uint8> ChannelMask,
	const TConstArrayView<int32> D8Receiver,
	TArray<int32>& OutHandBase)
{
	const int32 CellCount = ChannelMask.Num();

	OutHandBase.Reset();
	OutHandBase.Init(INDEX_NONE, CellCount);
	TArray<uint8> HandResolved;
	HandResolved.Init(0, CellCount);
	TArray<int32> WalkPath;
	for (int32 StartIndex = 0; StartIndex < CellCount; ++StartIndex)
	{
		if (HandResolved[StartIndex] != 0)
		{
			continue;
		}

		WalkPath.Reset();
		int32 CurrentIndex = StartIndex;
		int32 BaseIndex = INDEX_NONE;
		while (true)
		{
			if (HandResolved[CurrentIndex] != 0)
			{
				BaseIndex = OutHandBase[CurrentIndex];
				break;
			}
			if (ChannelMask[CurrentIndex] != 0)
			{
				BaseIndex = CurrentIndex;
				break;
			}

			WalkPath.Add(CurrentIndex);
			const int32 ReceiverIndex = D8Receiver[CurrentIndex];
			// The receiver chain decreases lexicographically on (FilledHeights, Index), so it cannot
			// cycle; the length test only bounds the walk if a future change breaks that.
			if (ReceiverIndex == INDEX_NONE || WalkPath.Num() > CellCount)
			{
				break;
			}
			CurrentIndex = ReceiverIndex;
		}

		if (BaseIndex != INDEX_NONE && HandResolved[BaseIndex] == 0)
		{
			OutHandBase[BaseIndex] = BaseIndex;
			HandResolved[BaseIndex] = 1;
		}
		for (const int32 PathIndex : WalkPath)
		{
			OutHandBase[PathIndex] = BaseIndex;
			HandResolved[PathIndex] = 1;
		}
	}
}

bool ArcTerrainHydrology::Solve(
	const FArcHydrologyGrid& Grid,
	const FArcHydrologySettings& Settings,
	const TConstArrayView<double> Accumulation,
	const TConstArrayView<uint8> ChannelMask,
	FArcHydrologySolution& OutSolution,
	FText& OutError)
{
	using namespace ArcTerrainHydrologyPrivate;

	OutSolution = FArcHydrologySolution();

	int32 CellCount = 0;
	if (!ValidateGrid(Grid, CellCount, OutError))
	{
		return false;
	}

	if (Accumulation.Num() != CellCount)
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTerrain hydrology needs one contributing area per margined cell; got %d for %d cells."),
			Accumulation.Num(), CellCount));
	}

	if (!ChannelMask.IsEmpty() && ChannelMask.Num() != CellCount)
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTerrain hydrology needs an empty channel mask or one entry per margined cell; got %d for %d cells."),
			ChannelMask.Num(), CellCount));
	}

	if (!PriorityFloodEpsilon(Grid, Settings, OutSolution.FilledHeights, OutSolution.TraversalOrder,
		OutSolution.FilledCellCount, OutSolution.SaturatedFlatCellCount, OutError))
	{
		return false;
	}

	ComputeTangentBeta(Grid, OutSolution.FilledHeights, OutSolution.TangentBeta);

	int32 ChannelCellCount = 0;
	for (const uint8 MaskValue : ChannelMask)
	{
		ChannelCellCount += MaskValue != 0 ? 1 : 0;
	}
	OutSolution.ChannelCellCount = ChannelCellCount;

	if (ChannelMask.IsEmpty())
	{
		// Without a channel the walk has no stop condition, so both passes would spend their full cost
		// to hand back the INDEX_NONE the consumer reads anyway.
		OutSolution.HandBase.Init(INDEX_NONE, CellCount);
	}
	else
	{
		ComputeD8Receivers(Grid, OutSolution.FilledHeights, OutSolution.TraversalOrder, OutSolution.D8Receiver);
		ComputeHAND(ChannelMask, OutSolution.D8Receiver, OutSolution.HandBase);
	}

	OutError = FText::GetEmpty();
	return true;
}
