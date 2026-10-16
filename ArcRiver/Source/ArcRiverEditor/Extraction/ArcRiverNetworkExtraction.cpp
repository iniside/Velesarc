// Copyright Lukasz Baran. All Rights Reserved.

#include "Extraction/ArcRiverNetworkExtraction.h"

#include "Algo/BinarySearch.h"
#include "Algo/Reverse.h"
#include "ArcRiverModule.h"
#include "Fields/ArcTerrainMaskDistance.h"

#define LOCTEXT_NAMESPACE "ArcRiverNetworkExtraction"

namespace ArcRiverNetworkExtractionPrivate
{
	constexpr int32 NeighborDX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
	constexpr int32 NeighborDY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

	void GatherNeighbors(
		const TArray<uint8>& Grid,
		const int32 X,
		const int32 Y,
		const int32 Width,
		const int32 Height,
		uint8 (&OutNeighbors)[8])
	{
		for (int32 Slot = 0; Slot < 8; ++Slot)
		{
			const int32 NeighborX = X + NeighborDX[Slot];
			const int32 NeighborY = Y + NeighborDY[Slot];
			const bool bInside = NeighborX >= 0 && NeighborX < Width && NeighborY >= 0 && NeighborY < Height;
			OutNeighbors[Slot] = bInside && Grid[NeighborX + NeighborY * Width] != 0 ? 1 : 0;
		}
	}

	int32 ThinSubIteration(
		TArray<uint8>& Grid,
		const int32 Width,
		const int32 Height,
		const int32 SubStep,
		TArray<int32>& ScratchMarked)
	{
		ScratchMarked.Reset();
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 Index = X + Y * Width;
				if (Grid[Index] == 0)
				{
					continue;
				}

				uint8 Neighbors[8];
				GatherNeighbors(Grid, X, Y, Width, Height, Neighbors);

				int32 NeighborCount = 0;
				for (int32 Slot = 0; Slot < 8; ++Slot)
				{
					NeighborCount += Neighbors[Slot];
				}
				if (NeighborCount < 2 || NeighborCount > 6)
				{
					continue;
				}

				int32 Transitions = 0;
				for (int32 Slot = 0; Slot < 8; ++Slot)
				{
					if (Neighbors[Slot] == 0 && Neighbors[(Slot + 1) % 8] != 0)
					{
						++Transitions;
					}
				}
				if (Transitions != 1)
				{
					continue;
				}

				const int32 North = Neighbors[0];
				const int32 East = Neighbors[2];
				const int32 South = Neighbors[4];
				const int32 West = Neighbors[6];
				const bool bCornerConditions = SubStep == 0
					? (North * East * South == 0 && East * South * West == 0)
					: (North * East * West == 0 && North * South * West == 0);
				if (!bCornerConditions)
				{
					continue;
				}

				ScratchMarked.Add(Index);
			}
		}

		for (const int32 Index : ScratchMarked)
		{
			Grid[Index] = 0;
		}
		return ScratchMarked.Num();
	}

	// Degree is ring arcs, not lit neighbours: a staircase corner lights NW, N and E, and NW+N are one arc.
	int32 CountSkeletonNeighborArcs(const TArray<uint8>& Grid, const int32 X, const int32 Y, const int32 Width, const int32 Height)
	{
		uint8 Neighbors[8];
		GatherNeighbors(Grid, X, Y, Width, Height, Neighbors);

		int32 LitCount = 0;
		int32 Arcs = 0;
		for (int32 Slot = 0; Slot < 8; ++Slot)
		{
			LitCount += Neighbors[Slot];
			if (Neighbors[Slot] == 0 && Neighbors[(Slot + 1) % 8] != 0)
			{
				++Arcs;
			}
		}
		return LitCount == 8 ? 1 : Arcs;
	}

	// First lit slot of each arc in the 8-neighbour ring (arc = cyclically contiguous run
	// of lit slots). All 8 lit -> 1 arc (matches CountSkeletonNeighborArcs).
	int32 ListSkeletonNeighborArcs(
		const TArray<uint8>& Grid,
		const int32 X,
		const int32 Y,
		const int32 Width,
		const int32 Height,
		int32 (&OutFirstSlots)[8])
	{
		uint8 Neighbors[8];
		GatherNeighbors(Grid, X, Y, Width, Height, Neighbors);

		int32 LitCount = 0;
		for (int32 Slot = 0; Slot < 8; ++Slot)
		{
			LitCount += Neighbors[Slot];
		}
		if (LitCount == 8)
		{
			OutFirstSlots[0] = 0;
			return 1;
		}

		int32 Arcs = 0;
		for (int32 Slot = 0; Slot < 8; ++Slot)
		{
			if (Neighbors[Slot] != 0 && Neighbors[(Slot + 7) % 8] == 0)
			{
				OutFirstSlots[Arcs] = Slot;
				++Arcs;
			}
		}
		return Arcs;
	}

	int32 NeighborIndexInSlot(
		const TArray<uint8>& Grid,
		const int32 Index,
		const int32 Slot,
		const int32 Width,
		const int32 Height)
	{
		const int32 NeighborX = Index % Width + NeighborDX[Slot];
		const int32 NeighborY = Index / Width + NeighborDY[Slot];
		if (NeighborX < 0 || NeighborX >= Width || NeighborY < 0 || NeighborY >= Height)
		{
			return INDEX_NONE;
		}
		const int32 NeighborIndex = NeighborX + NeighborY * Width;
		return Grid[NeighborIndex] != 0 ? NeighborIndex : INDEX_NONE;
	}

	double PointToSegmentDistance(const FVector2f& Point, const FVector2f& SegmentStart, const FVector2f& SegmentEnd)
	{
		const double PointX = static_cast<double>(Point.X);
		const double PointY = static_cast<double>(Point.Y);
		const double StartX = static_cast<double>(SegmentStart.X);
		const double StartY = static_cast<double>(SegmentStart.Y);
		const double DeltaX = static_cast<double>(SegmentEnd.X) - StartX;
		const double DeltaY = static_cast<double>(SegmentEnd.Y) - StartY;
		const double LengthSquared = DeltaX * DeltaX + DeltaY * DeltaY;
		if (LengthSquared <= 0.0)
		{
			const double ToPointX = PointX - StartX;
			const double ToPointY = PointY - StartY;
			return FMath::Sqrt(ToPointX * ToPointX + ToPointY * ToPointY);
		}
		const double Projection = FMath::Clamp(
			((PointX - StartX) * DeltaX + (PointY - StartY) * DeltaY) / LengthSquared, 0.0, 1.0);
		const double ToClosestX = PointX - (StartX + Projection * DeltaX);
		const double ToClosestY = PointY - (StartY + Projection * DeltaY);
		return FMath::Sqrt(ToClosestX * ToClosestX + ToClosestY * ToClosestY);
	}

	void SimplifyChain(
		const TArray<FVector2f>& SourcePoints,
		const TArray<float>& SourceHalfWidths,
		const float Tolerance,
		TConstArrayView<int32> ForcedIndices,
		TArray<FVector2f>& OutPoints,
		TArray<float>& OutHalfWidths,
		TArray<int32>& OutRetainedIndices)
	{
		OutPoints.Reset();
		OutHalfWidths.Reset();
		OutRetainedIndices.Reset();

		const int32 NumSource = SourcePoints.Num();
		if (NumSource < 2)
		{
			return;
		}

		TArray<uint8> bRetained;
		bRetained.Init(0, NumSource);
		bRetained[0] = 1;
		bRetained[NumSource - 1] = 1;

		TArray<int32> Anchors;
		Anchors.Reserve(ForcedIndices.Num() + 2);
		Anchors.Add(0);
		for (const int32 ForcedIndex : ForcedIndices)
		{
			if (ForcedIndex > 0 && ForcedIndex < NumSource - 1)
			{
				bRetained[ForcedIndex] = 1;
				Anchors.Add(ForcedIndex);
			}
		}
		Anchors.Add(NumSource - 1);
		Anchors.Sort();

		TArray<FIntPoint> PendingRanges;
		for (int32 Slot = 1; Slot < Anchors.Num(); ++Slot)
		{
			if (Anchors[Slot] > Anchors[Slot - 1])
			{
				PendingRanges.Add(FIntPoint(Anchors[Slot - 1], Anchors[Slot]));
			}
		}
		while (PendingRanges.Num() > 0)
		{
			const FIntPoint Range = PendingRanges.Pop(EAllowShrinking::No);
			int32 FarthestIndex = INDEX_NONE;
			double FarthestDistance = static_cast<double>(Tolerance);
			for (int32 Index = Range.X + 1; Index < Range.Y; ++Index)
			{
				const double Distance = PointToSegmentDistance(
					SourcePoints[Index], SourcePoints[Range.X], SourcePoints[Range.Y]);
				if (Distance > FarthestDistance)
				{
					FarthestDistance = Distance;
					FarthestIndex = Index;
				}
			}
			if (FarthestIndex == INDEX_NONE)
			{
				continue;
			}
			bRetained[FarthestIndex] = 1;
			PendingRanges.Add(FIntPoint(Range.X, FarthestIndex));
			PendingRanges.Add(FIntPoint(FarthestIndex, Range.Y));
		}

		for (int32 Index = 0; Index < NumSource; ++Index)
		{
			if (bRetained[Index] != 0)
			{
				OutRetainedIndices.Add(Index);
			}
		}

		const TArray<int32>& RetainedIndices = OutRetainedIndices;
		const int32 NumRetained = RetainedIndices.Num();
		OutPoints.Reserve(NumRetained);
		OutHalfWidths.Reserve(NumRetained);
		for (int32 Slot = 0; Slot < NumRetained; ++Slot)
		{
			const int32 SourceIndex = RetainedIndices[Slot];
			// Max, not sample: a widening mid-reach deviates geometrically by ~0, so simplification
			// drops it and a sampled width would carry the entrance width through the whole pool.
			const int32 SpanStart = Slot == 0
				? SourceIndex
				: (RetainedIndices[Slot - 1] + SourceIndex) / 2;
			const int32 SpanEnd = Slot == NumRetained - 1
				? SourceIndex
				: (SourceIndex + RetainedIndices[Slot + 1]) / 2;

			float MaxHalfWidth = SourceHalfWidths[SourceIndex];
			for (int32 Index = SpanStart; Index <= SpanEnd; ++Index)
			{
				MaxHalfWidth = FMath::Max(MaxHalfWidth, SourceHalfWidths[Index]);
			}

			OutPoints.Add(SourcePoints[SourceIndex]);
			OutHalfWidths.Add(MaxHalfWidth);
		}
	}

	struct FWorkChain
	{
		ArcRiverNetworkExtraction::FArcRiverChain Chain;
		int32 LowestTexelIndex = 0;
		double LengthTexels = 0.0;
		int32 EndJunctions[2] = {INDEX_NONE, INDEX_NONE};
	};

	constexpr int32 MaxPrunePasses = 64;

	bool PruneShortChains(
		TConstArrayView<FWorkChain> WorkChains,
		const int32 NumJunctions,
		const int32 MinChainTexels,
		TArray<uint8>& OutKeep,
		int32& OutPasses)
	{
		OutKeep.Init(1, WorkChains.Num());
		OutPasses = 0;

		TArray<int32> JunctionDegree;
		JunctionDegree.Init(0, NumJunctions);
		for (const FWorkChain& Work : WorkChains)
		{
			for (const int32 JunctionId : Work.EndJunctions)
			{
				if (JunctionId != INDEX_NONE)
				{
					++JunctionDegree[JunctionId];
				}
			}
		}

		for (int32 Pass = 0; Pass < MaxPrunePasses; ++Pass)
		{
			++OutPasses;
			int32 RemovedThisPass = 0;
			for (int32 ChainIndex = 0; ChainIndex < WorkChains.Num(); ++ChainIndex)
			{
				if (OutKeep[ChainIndex] == 0)
				{
					continue;
				}
				const FWorkChain& Work = WorkChains[ChainIndex];
				if (Work.LengthTexels >= static_cast<double>(MinChainTexels))
				{
					continue;
				}

				const int32 StartJunction = Work.EndJunctions[0];
				const int32 EndJunction = Work.EndJunctions[1];
				// Connectivity, not just length: dropping a chain whose two junctions are left with one
				// end each severs the network, so a junction-to-junction chain goes only while both its
				// junctions keep two other ends. A free end or a self-loop can never disconnect anything.
				const bool bFreeEnd = StartJunction == INDEX_NONE || EndJunction == INDEX_NONE;
				const bool bSelfLoop = StartJunction == EndJunction;
				const bool bPrunable = bFreeEnd || bSelfLoop
					|| (JunctionDegree[StartJunction] >= 3 && JunctionDegree[EndJunction] >= 3);
				if (!bPrunable)
				{
					continue;
				}

				OutKeep[ChainIndex] = 0;
				++RemovedThisPass;
				for (const int32 JunctionId : Work.EndJunctions)
				{
					if (JunctionId != INDEX_NONE)
					{
						--JunctionDegree[JunctionId];
					}
				}
			}

			if (RemovedThisPass == 0)
			{
				return true;
			}
		}

		return false;
	}

	struct FTexelHost
	{
		int32 ChainIndex = INDEX_NONE;
		int32 PointIndex = INDEX_NONE;
	};

	void PromoteFalseFreeEnds(
		TArray<FWorkChain>& WorkChains,
		TArray<FIntPoint>& OutJunctionTexels,
		const int32 Width)
	{
		TMap<int32, FTexelHost> TexelHosts;
		for (int32 ChainIndex = 0; ChainIndex < WorkChains.Num(); ++ChainIndex)
		{
			const TArray<FVector2f>& Points = WorkChains[ChainIndex].Chain.TexelXY;
			for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
			{
				const int32 LinearIndex =
					static_cast<int32>(Points[PointIndex].X) + static_cast<int32>(Points[PointIndex].Y) * Width;
				if (!TexelHosts.Contains(LinearIndex))
				{
					FTexelHost& Host = TexelHosts.Add(LinearIndex);
					Host.ChainIndex = ChainIndex;
					Host.PointIndex = PointIndex;
				}
			}
		}

		for (int32 ChainIndex = 0; ChainIndex < WorkChains.Num(); ++ChainIndex)
		{
			FWorkChain& Work = WorkChains[ChainIndex];
			const int32 NumPoints = Work.Chain.TexelXY.Num();
			if (NumPoints == 0)
			{
				continue;
			}
			for (int32 Side = 0; Side < 2; ++Side)
			{
				if (Work.EndJunctions[Side] != INDEX_NONE)
				{
					continue;
				}
				const int32 EndPointIndex = Side == 0 ? 0 : NumPoints - 1;
				const FVector2f EndXY = Work.Chain.TexelXY[EndPointIndex];
				const int32 LinearIndex =
					static_cast<int32>(EndXY.X) + static_cast<int32>(EndXY.Y) * Width;
				const FTexelHost* Host = TexelHosts.Find(LinearIndex);
				if (Host == nullptr || Host->ChainIndex == ChainIndex)
				{
					continue;
				}
				const int32 HostNumPoints = WorkChains[Host->ChainIndex].Chain.TexelXY.Num();
				if (Host->PointIndex == 0 || Host->PointIndex == HostNumPoints - 1)
				{
					continue;
				}

				const int32 JunctionId = OutJunctionTexels.Add(
					FIntPoint(static_cast<int32>(EndXY.X), static_cast<int32>(EndXY.Y)));
				Work.EndJunctions[Side] = JunctionId;

				ArcRiverNetworkExtraction::FArcRiverJunctionRef& HostRef =
					WorkChains[Host->ChainIndex].Chain.Junctions.AddDefaulted_GetRef();
				HostRef.JunctionId = JunctionId;
				HostRef.PointIndex = Host->PointIndex;
			}
		}
	}

	struct FStemMember
	{
		int32 ChainIndex = INDEX_NONE;
		bool bReversed = false;
	};

	void BuildChainGraph(
		TConstArrayView<FWorkChain> Chains,
		const int32 NumJunctions,
		TArray<int32>& OutEndVertices,
		TArray<TArray<int32>>& OutVertexChains)
	{
		OutEndVertices.SetNumUninitialized(Chains.Num() * 2);
		int32 NextFreeEndVertex = NumJunctions;
		for (int32 ChainIndex = 0; ChainIndex < Chains.Num(); ++ChainIndex)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const int32 JunctionId = Chains[ChainIndex].EndJunctions[Side];
				OutEndVertices[ChainIndex * 2 + Side] =
					JunctionId != INDEX_NONE ? JunctionId : NextFreeEndVertex++;
			}
		}

		OutVertexChains.Reset();
		OutVertexChains.SetNum(NextFreeEndVertex);
		for (int32 ChainIndex = 0; ChainIndex < Chains.Num(); ++ChainIndex)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				OutVertexChains[OutEndVertices[ChainIndex * 2 + Side]].Add(ChainIndex);
			}
		}
	}

	// Two sweeps are the exact diameter on trees; on cyclic graphs this is a deterministic heuristic.
	int32 FarthestVertex(
		const int32 Start,
		TConstArrayView<TArray<int32>> VertexChains,
		TConstArrayView<int32> EndVertices,
		TConstArrayView<FWorkChain> Chains,
		TConstArrayView<uint8> ChainUsed,
		TArray<int32>* OutParentVertex,
		TArray<int32>* OutParentChain)
	{
		struct FHeapEntry
		{
			double Dist = 0.0;
			int32 Vertex = INDEX_NONE;
		};
		struct FHeapLess
		{
			bool operator()(const FHeapEntry& Left, const FHeapEntry& Right) const
			{
				if (Left.Dist != Right.Dist)
				{
					return Left.Dist > Right.Dist;
				}
				return Left.Vertex < Right.Vertex;
			}
		};

		const int32 NumVertices = VertexChains.Num();
		TArray<double> Dist;
		Dist.Init(TNumericLimits<double>::Lowest(), NumVertices);
		Dist[Start] = 0.0;

		TArray<uint8> bFinalized;
		bFinalized.Init(0, NumVertices);

		if (OutParentVertex != nullptr)
		{
			OutParentVertex->Init(INDEX_NONE, NumVertices);
		}
		if (OutParentChain != nullptr)
		{
			OutParentChain->Init(INDEX_NONE, NumVertices);
		}

		const FHeapLess HeapLess;
		TArray<FHeapEntry> Heap;
		FHeapEntry StartEntry;
		StartEntry.Dist = 0.0;
		StartEntry.Vertex = Start;
		Heap.HeapPush(StartEntry, HeapLess);

		int32 Farthest = Start;
		double FarthestDist = 0.0;
		while (Heap.Num() > 0)
		{
			FHeapEntry Current;
			Heap.HeapPop(Current, HeapLess, EAllowShrinking::No);
			if (bFinalized[Current.Vertex] != 0)
			{
				continue;
			}
			bFinalized[Current.Vertex] = 1;

			if (Current.Dist > FarthestDist
				|| (Current.Dist == FarthestDist && Current.Vertex < Farthest))
			{
				FarthestDist = Current.Dist;
				Farthest = Current.Vertex;
			}

			for (const int32 ChainIndex : VertexChains[Current.Vertex])
			{
				if (ChainUsed[ChainIndex] != 0)
				{
					continue;
				}

				const int32 Other = EndVertices[ChainIndex * 2] == Current.Vertex
					? EndVertices[ChainIndex * 2 + 1]
					: EndVertices[ChainIndex * 2];
				if (bFinalized[Other] != 0)
				{
					continue;
				}

				const double NewDist = Current.Dist + Chains[ChainIndex].LengthTexels;
				if (NewDist > Dist[Other])
				{
					Dist[Other] = NewDist;
					if (OutParentVertex != nullptr)
					{
						(*OutParentVertex)[Other] = Current.Vertex;
					}
					if (OutParentChain != nullptr)
					{
						(*OutParentChain)[Other] = ChainIndex;
					}

					FHeapEntry Relaxed;
					Relaxed.Dist = NewDist;
					Relaxed.Vertex = Other;
					Heap.HeapPush(Relaxed, HeapLess);
				}
			}
		}

		return Farthest;
	}

	void ReconstructStem(
		const int32 U,
		const int32 V,
		TConstArrayView<int32> EndVertices,
		TConstArrayView<int32> ParentVertex,
		TConstArrayView<int32> ParentChain,
		TArray<FStemMember>& OutStem)
	{
		TArray<FStemMember> Reversed;
		int32 Current = V;
		while (Current != U)
		{
			const int32 ChainIndex = ParentChain[Current];
			const int32 EntryVertex = ParentVertex[Current];
			FStemMember& Member = Reversed.AddDefaulted_GetRef();
			Member.ChainIndex = ChainIndex;
			Member.bReversed = EndVertices[ChainIndex * 2] != EntryVertex;
			Current = EntryVertex;
		}
		Algo::Reverse(Reversed);
		OutStem = MoveTemp(Reversed);
	}

	void AssembleStems(
		TConstArrayView<FWorkChain> Chains,
		const int32 NumJunctions,
		TArray<TArray<FStemMember>>& OutStems)
	{
		OutStems.Reset();

		TArray<int32> EndVertices;
		TArray<TArray<int32>> VertexChains;
		BuildChainGraph(Chains, NumJunctions, EndVertices, VertexChains);

		TArray<uint8> ChainUsed;
		ChainUsed.Init(0, Chains.Num());

		TArray<int32> ParentVertex;
		TArray<int32> ParentChain;
		while (true)
		{
			int32 Seed = INDEX_NONE;
			double BestLength = -1.0;
			int32 BestLowestTexelIndex = MAX_int32;
			for (int32 ChainIndex = 0; ChainIndex < Chains.Num(); ++ChainIndex)
			{
				if (ChainUsed[ChainIndex] != 0)
				{
					continue;
				}
				const FWorkChain& Work = Chains[ChainIndex];
				if (Work.LengthTexels > BestLength
					|| (Work.LengthTexels == BestLength && Work.LowestTexelIndex < BestLowestTexelIndex))
				{
					BestLength = Work.LengthTexels;
					BestLowestTexelIndex = Work.LowestTexelIndex;
					Seed = ChainIndex;
				}
			}
			if (Seed == INDEX_NONE)
			{
				break;
			}

			const int32 U = FarthestVertex(
				EndVertices[Seed * 2],
				VertexChains,
				EndVertices,
				Chains,
				ChainUsed,
				nullptr,
				nullptr);
			const int32 V = FarthestVertex(
				U,
				VertexChains,
				EndVertices,
				Chains,
				ChainUsed,
				&ParentVertex,
				&ParentChain);

			TArray<FStemMember>& Stem = OutStems.AddDefaulted_GetRef();
			if (V == U)
			{
				FStemMember& Member = Stem.AddDefaulted_GetRef();
				Member.ChainIndex = Seed;
				Member.bReversed = false;
			}
			else
			{
				ReconstructStem(U, V, EndVertices, ParentVertex, ParentChain, Stem);
			}

			for (const FStemMember& Member : Stem)
			{
				ChainUsed[Member.ChainIndex] = 1;
			}
		}
	}

	int32 ConcatenateMemberFirstStep(
		const int32 Slot,
		const bool bReversed,
		const TArray<FVector2f>& StemTexelXY,
		const TArray<FVector2f>& MemberTexelXY)
	{
		const int32 NumPoints = MemberTexelXY.Num();
		const int32 TraversalFirst = bReversed ? (NumPoints - 1) : 0;
		const bool bSkipFirst = Slot > 0
			&& StemTexelXY.Last() == MemberTexelXY[TraversalFirst];
		return bSkipFirst ? 1 : 0;
	}

	void ConcatenateStem(
		TConstArrayView<FWorkChain> Chains,
		TConstArrayView<FStemMember> Stem,
		FWorkChain& OutStem)
	{
		OutStem = FWorkChain();
		OutStem.LowestTexelIndex = MAX_int32;

		for (int32 Slot = 0; Slot < Stem.Num(); ++Slot)
		{
			const FWorkChain& Member = Chains[Stem[Slot].ChainIndex];
			const bool bReversed = Stem[Slot].bReversed;
			const int32 NumPoints = Member.Chain.TexelXY.Num();

			const int32 StemPointsBeforeThisMember = OutStem.Chain.TexelXY.Num();
			const int32 FirstStep = ConcatenateMemberFirstStep(
				Slot, bReversed, OutStem.Chain.TexelXY, Member.Chain.TexelXY);
			const bool bSkipFirst = FirstStep == 1;
			for (int32 Step = FirstStep; Step < NumPoints; ++Step)
			{
				const int32 SourceIndex = bReversed ? NumPoints - 1 - Step : Step;
				OutStem.Chain.TexelXY.Add(Member.Chain.TexelXY[SourceIndex]);
				OutStem.Chain.HalfWidthTexels.Add(Member.Chain.HalfWidthTexels[SourceIndex]);
			}

			OutStem.LengthTexels += Member.LengthTexels;
			OutStem.LowestTexelIndex = FMath::Min(OutStem.LowestTexelIndex, Member.LowestTexelIndex);

			const int32 EntryJunction = Member.EndJunctions[bReversed ? 1 : 0];
			if (Slot == 0 && EntryJunction != INDEX_NONE)
			{
				ArcRiverNetworkExtraction::FArcRiverJunctionRef& Ref =
					OutStem.Chain.Junctions.AddDefaulted_GetRef();
				Ref.JunctionId = EntryJunction;
				Ref.PointIndex = 0;
			}

			for (const ArcRiverNetworkExtraction::FArcRiverJunctionRef& MemberRef : Member.Chain.Junctions)
			{
				const int32 MemberPointIndex = MemberRef.PointIndex;
				if (MemberPointIndex <= 0 || MemberPointIndex >= NumPoints - 1)
				{
					continue;
				}

				const int32 Step = bReversed ? (NumPoints - 1 - MemberPointIndex) : MemberPointIndex;
				if (bSkipFirst && Step == 0)
				{
					continue;
				}

				ArcRiverNetworkExtraction::FArcRiverJunctionRef& Ref =
					OutStem.Chain.Junctions.AddDefaulted_GetRef();
				Ref.JunctionId = MemberRef.JunctionId;
				Ref.PointIndex = StemPointsBeforeThisMember + Step - FirstStep;
			}

			const int32 ExitJunction = Member.EndJunctions[bReversed ? 0 : 1];
			if (ExitJunction != INDEX_NONE)
			{
				ArcRiverNetworkExtraction::FArcRiverJunctionRef& Ref =
					OutStem.Chain.Junctions.AddDefaulted_GetRef();
				Ref.JunctionId = ExitJunction;
				Ref.PointIndex = OutStem.Chain.TexelXY.Num() - 1;
			}
		}
	}
}

namespace ArcRiverNetworkExtraction
{
	void ExtractChains(
		TConstArrayView<float> Mask,
		FIntPoint Dims,
		float Threshold,
		float MinHalfWidthTexels,
		float SimplifyToleranceTexels,
		int32 MinChainTexels,
		TArray<FArcRiverChain>& OutChains,
		TArray<FIntPoint>& OutJunctionTexels,
		FArcRiverExtractionStats& OutStats,
		FText& OutError,
		TFunctionRef<bool(float /*Progress01*/)> ShouldContinue)
	{
		using namespace ArcRiverNetworkExtractionPrivate;

		OutChains.Reset();
		OutJunctionTexels.Reset();
		OutStats = FArcRiverExtractionStats();
		OutError = FText::GetEmpty();

		const int32 Width = Dims.X;
		const int32 Height = Dims.Y;
		if (Width <= 0 || Height <= 0)
		{
			UE_LOGF(LogArcRiver, Error,"River extraction needs a non-empty mask; got %dx%d.", Width, Height);
			OutError = FText::Format(
				LOCTEXT("EmptyMask", "River extraction needs a non-empty mask; the decoded Rivers mask is {0}x{1}."),
				FText::AsNumber(Width), FText::AsNumber(Height));
			return;
		}
		if (Width > MaxMaskDim || Height > MaxMaskDim)
		{
			UE_LOGF(LogArcRiver, Error,
				"River mask is %dx%d, above the %d per-side limit. Re-export the Rivers mask at or below %d per side; "
				"extraction runs on the game thread and a larger mask costs hundreds of MB and minutes of stall.",
				Width, Height, MaxMaskDim, MaxMaskDim);
			OutError = FText::Format(
				LOCTEXT("MaskTooLarge",
					"The Rivers mask is {0}x{1}, above the {2} per-side limit. Re-export it at or below {2} texels per side."),
				FText::AsNumber(Width), FText::AsNumber(Height), FText::AsNumber(MaxMaskDim));
			return;
		}

		const int32 NumTexels = Width * Height;
		if (Mask.Num() != NumTexels)
		{
			UE_LOGF(LogArcRiver, Error,"River mask has %d values for %dx%d texels.", Mask.Num(), Width, Height);
			OutError = FText::Format(
				LOCTEXT("MaskCountMismatch", "The Rivers mask carries {0} values for its {1}x{2} texels."),
				FText::AsNumber(Mask.Num()), FText::AsNumber(Width), FText::AsNumber(Height));
			return;
		}

		TArray<uint8> Skeleton;
		TArray<uint8> DryMask;
		Skeleton.SetNumUninitialized(NumTexels);
		DryMask.SetNumUninitialized(NumTexels);
		for (int32 Index = 0; Index < NumTexels; ++Index)
		{
			const bool bWater = Mask[Index] >= Threshold;
			Skeleton[Index] = bWater ? 1 : 0;
			DryMask[Index] = bWater ? 0 : 1;
		}

		// Seeded on the dry texels so a water texel's distance is its distance to the nearest bank, i.e. its half-width.
		TArray<double> SquaredDistanceToBank;
		ArcTerrainMaskDistance::BuildSquaredCellDistanceToMask(DryMask, Width, Height, SquaredDistanceToBank);
		DryMask.Empty();

		const double UnreachableSquaredDistanceSentinel = ArcTerrainMaskDistance::UnreachableSquaredDistance(Width, Height);
		for (int32 Index = 0; Index < NumTexels; ++Index)
		{
			if (Skeleton[Index] != 0 && SquaredDistanceToBank[Index] >= UnreachableSquaredDistanceSentinel)
			{
				UE_LOGF(LogArcRiver, Error,
					"River mask has no dry texel reachable from water texel (%d, %d), so its half-width is unbounded. "
					"The %.3f threshold is most likely below the mask's noise floor; raise it until dry land reads as dry.",
					Index % Width, Index / Width, Threshold);
				OutError = FText::Format(
					LOCTEXT("NoDryTexel",
						"The Rivers mask has no dry texel reachable from water texel ({0}, {1}), so its half-width is "
						"unbounded. ChannelThreshold ({2}) is most likely below the mask's noise floor; raise it until "
						"dry land reads as dry."),
					FText::AsNumber(Index % Width), FText::AsNumber(Index / Width), FText::AsNumber(Threshold));
				return;
			}
		}

		// The filter erodes the thinning input only: half-widths below still read the untouched field, so
		// a surviving river keeps its true width instead of losing the erosion radius off both banks.
		const double MinSquaredHalfWidth =
			static_cast<double>(MinHalfWidthTexels) * static_cast<double>(MinHalfWidthTexels);
		for (int32 Index = 0; Index < NumTexels; ++Index)
		{
			if (Skeleton[Index] == 0)
			{
				continue;
			}
			++OutStats.WaterTexelCount;
			if (SquaredDistanceToBank[Index] < MinSquaredHalfWidth)
			{
				Skeleton[Index] = 0;
			}
			else
			{
				++OutStats.WaterTexelsAfterWidthFilter;
			}
		}

		if (OutStats.WaterTexelsAfterWidthFilter == 0)
		{
			UE_LOGF(LogArcRiver, Error,
				"Every one of the %d water texels in the %dx%d Rivers mask is narrower than the %.2f texel "
				"minimum half width.",
				OutStats.WaterTexelCount, Width, Height, MinHalfWidthTexels);
			OutError = FText::Format(
				LOCTEXT("AllChannelsTooNarrow",
					"Every one of the {0} water texels in the {1}x{2} Rivers mask sits closer than {3} texels to a "
					"bank, so MinHalfWidth removes the whole network. Lower MinHalfWidth or lower ChannelThreshold."),
				FText::AsNumber(OutStats.WaterTexelCount), FText::AsNumber(Width), FText::AsNumber(Height),
				FText::AsNumber(MinHalfWidthTexels));
			return;
		}

		// A thickness bound, not an erosion-per-pass one: no mask is thicker than max(Dims) and a full
		// iteration peels at most one layer off each side of it.
		const int32 MaxIterations = FMath::Max(Width, Height) / 2 + 2;
		TArray<int32> ScratchMarked;
		int32 Iteration = 0;
		int32 RemovedLastIteration = 1;
		while (RemovedLastIteration > 0)
		{
			if (Iteration >= MaxIterations)
			{
				UE_LOGF(LogArcRiver, Error,"River mask thinning did not converge within %d iterations on a %dx%d mask.",
					MaxIterations, Width, Height);
				OutError = FText::Format(
					LOCTEXT("ThinningDidNotConverge",
						"Thinning the {0}x{1} Rivers mask to a centreline did not converge within {2} iterations."),
					FText::AsNumber(Width), FText::AsNumber(Height), FText::AsNumber(MaxIterations));
				return;
			}
			if (!ShouldContinue(static_cast<float>(Iteration) / static_cast<float>(MaxIterations)))
			{
				return;
			}

			const int32 RemovedFirstSubStep = ThinSubIteration(Skeleton, Width, Height, 0, ScratchMarked);
			const int32 RemovedSecondSubStep = ThinSubIteration(Skeleton, Width, Height, 1, ScratchMarked);
			RemovedLastIteration = RemovedFirstSubStep + RemovedSecondSubStep;
			++Iteration;
		}

		for (int32 Index = 0; Index < NumTexels; ++Index)
		{
			OutStats.SkeletonTexelCount += Skeleton[Index] != 0 ? 1 : 0;
		}

		TArray<uint8> SkeletonArcDegree;
		SkeletonArcDegree.SetNumUninitialized(NumTexels);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 Index = X + Y * Width;
				SkeletonArcDegree[Index] = Skeleton[Index] != 0
					? static_cast<uint8>(CountSkeletonNeighborArcs(Skeleton, X, Y, Width, Height))
					: 0;
			}
		}

		TArray<int32> JunctionIdAt;
		JunctionIdAt.Init(INDEX_NONE, NumTexels);
		for (int32 Index = 0; Index < NumTexels; ++Index)
		{
			if (Skeleton[Index] != 0 && SkeletonArcDegree[Index] >= 3)
			{
				JunctionIdAt[Index] = OutJunctionTexels.Add(FIntPoint(Index % Width, Index / Width));
			}
		}

		TArray<int32> ClusterIdAt;
		ClusterIdAt.Init(INDEX_NONE, NumTexels);
		TArray<int32> ClusterStack;
		OutJunctionTexels.Reset();
		for (int32 SeedIndex = 0; SeedIndex < NumTexels; ++SeedIndex)
		{
			if (JunctionIdAt[SeedIndex] == INDEX_NONE || ClusterIdAt[SeedIndex] != INDEX_NONE)
			{
				continue;
			}

			const int32 ClusterId = OutJunctionTexels.Add(
				FIntPoint(SeedIndex % Width, SeedIndex / Width));
			ClusterStack.Reset();
			ClusterStack.Add(SeedIndex);
			ClusterIdAt[SeedIndex] = ClusterId;

			while (ClusterStack.Num() > 0)
			{
				const int32 CurrentIndex = ClusterStack.Pop(EAllowShrinking::No);
				const int32 CurrentX = CurrentIndex % Width;
				const int32 CurrentY = CurrentIndex / Width;
				for (int32 Slot = 0; Slot < 8; ++Slot)
				{
					const int32 NeighborX = CurrentX + NeighborDX[Slot];
					const int32 NeighborY = CurrentY + NeighborDY[Slot];
					if (NeighborX < 0 || NeighborX >= Width || NeighborY < 0 || NeighborY >= Height)
					{
						continue;
					}
					const int32 NeighborIndex = NeighborX + NeighborY * Width;
					if (JunctionIdAt[NeighborIndex] == INDEX_NONE || ClusterIdAt[NeighborIndex] != INDEX_NONE)
					{
						continue;
					}
					ClusterIdAt[NeighborIndex] = ClusterId;
					ClusterStack.Add(NeighborIndex);
				}
			}
		}
		JunctionIdAt = MoveTemp(ClusterIdAt);

		TArray<uint8> bInteriorWalked;
		bInteriorWalked.Init(0, NumTexels);
		TSet<uint64> EmittedNodeEdges;
		TArray<FWorkChain> WorkChains;
		TArray<int32> PointIndices;

		for (int32 NodeIndex = 0; NodeIndex < NumTexels; ++NodeIndex)
		{
			if (Skeleton[NodeIndex] == 0)
			{
				continue;
			}
			const int32 NodeDegree = SkeletonArcDegree[NodeIndex];
			if (NodeDegree != 1 && NodeDegree < 3)
			{
				continue;
			}

			int32 ArcFirstSlots[8];
			const int32 ArcCount = ListSkeletonNeighborArcs(
				Skeleton, NodeIndex % Width, NodeIndex / Width, Width, Height, ArcFirstSlots);
			for (int32 ArcIndex = 0; ArcIndex < ArcCount; ++ArcIndex)
			{
				const int32 FirstStep = NeighborIndexInSlot(
					Skeleton, NodeIndex, ArcFirstSlots[ArcIndex], Width, Height);
				if (FirstStep == INDEX_NONE)
				{
					continue;
				}

				const int32 StartId = JunctionIdAt[NodeIndex];
				const int32 FirstId = JunctionIdAt[FirstStep];
				if (StartId != INDEX_NONE && FirstId == StartId)
				{
					continue;
				}

				const int32 FirstStepDegree = SkeletonArcDegree[FirstStep];
				const bool bFirstStepIsNode = FirstStepDegree == 1 || FirstStepDegree >= 3;
				if (bFirstStepIsNode)
				{
					const uint64 EdgeKey = static_cast<uint64>(FMath::Min(NodeIndex, FirstStep)) * NumTexels
						+ static_cast<uint64>(FMath::Max(NodeIndex, FirstStep));
					if (EmittedNodeEdges.Contains(EdgeKey))
					{
						continue;
					}
					EmittedNodeEdges.Add(EdgeKey);
				}
				else if (bInteriorWalked[FirstStep] != 0)
				{
					continue;
				}

				PointIndices.Reset();
				PointIndices.Add(NodeIndex);
				PointIndices.Add(FirstStep);
				if (!bFirstStepIsNode)
				{
					bInteriorWalked[FirstStep] = 1;

					int32 Previous = NodeIndex;
					int32 Current = FirstStep;
					while (true)
					{
						int32 Next = INDEX_NONE;
						for (int32 WalkSlot = 0; WalkSlot < 8; ++WalkSlot)
						{
							const int32 Candidate = NeighborIndexInSlot(Skeleton, Current, WalkSlot, Width, Height);
							if (Candidate != INDEX_NONE && Candidate != Previous)
							{
								Next = Candidate;
								break;
							}
						}
						if (Next == INDEX_NONE)
						{
							break;
						}

						PointIndices.Add(Next);
						const int32 NextDegree = SkeletonArcDegree[Next];
						if (NextDegree == 1 || NextDegree >= 3 || bInteriorWalked[Next] != 0)
						{
							break;
						}
						bInteriorWalked[Next] = 1;
						Previous = Current;
						Current = Next;
					}
				}

				++OutStats.ChainsBeforePrune;

				const int32 StartJunctionId = JunctionIdAt[PointIndices[0]];
				const int32 EndJunctionId = JunctionIdAt[PointIndices.Last()];

				FWorkChain& Work = WorkChains.AddDefaulted_GetRef();
				Work.LowestTexelIndex = NumTexels;
				for (int32 Step = 1; Step < PointIndices.Num(); ++Step)
				{
					const int32 PreviousIndex = PointIndices[Step - 1];
					const int32 CurrentIndex = PointIndices[Step];
					const bool bDiagonalStep = PreviousIndex % Width != CurrentIndex % Width
						&& PreviousIndex / Width != CurrentIndex / Width;
					Work.LengthTexels += bDiagonalStep ? UE_DOUBLE_SQRT_2 : 1.0;
				}
				Work.Chain.TexelXY.Reserve(PointIndices.Num());
				Work.Chain.HalfWidthTexels.Reserve(PointIndices.Num());
				for (const int32 Index : PointIndices)
				{
					Work.Chain.TexelXY.Add(FVector2f(
						static_cast<float>(Index % Width), static_cast<float>(Index / Width)));
					Work.Chain.HalfWidthTexels.Add(
						static_cast<float>(FMath::Sqrt(SquaredDistanceToBank[Index])));
					Work.LowestTexelIndex = FMath::Min(Work.LowestTexelIndex, Index);
				}
				Work.EndJunctions[0] = StartJunctionId;
				Work.EndJunctions[1] = EndJunctionId;
			}
		}

		int32 UnwalkedInteriorTexels = 0;
		for (int32 Index = 0; Index < NumTexels; ++Index)
		{
			if (SkeletonArcDegree[Index] == 2 && bInteriorWalked[Index] == 0)
			{
				++UnwalkedInteriorTexels;
			}
		}
		UE_CLOGF(UnwalkedInteriorTexels > 0, LogArcRiver, Warning,"River extraction dropped %d skeleton texels forming closed loops with no endpoint or junction.",
			UnwalkedInteriorTexels);

		TArray<uint8> bChainKept;
		if (!PruneShortChains(WorkChains, OutJunctionTexels.Num(), MinChainTexels, bChainKept, OutStats.PrunePasses))
		{
			OutStats.bPruneHitPassBound = true;
			UE_LOGF(LogArcRiver, Warning,
				"River chain pruning was still removing chains when it hit its %d-pass bound; short chains remain "
				"in the network.",
				MaxPrunePasses);
		}

		TArray<FWorkChain> KeptChains;
		KeptChains.Reserve(WorkChains.Num());
		for (int32 ChainIndex = 0; ChainIndex < WorkChains.Num(); ++ChainIndex)
		{
			if (bChainKept[ChainIndex] != 0)
			{
				KeptChains.Add(MoveTemp(WorkChains[ChainIndex]));
			}
		}
		WorkChains = MoveTemp(KeptChains);

		OutStats.ChainsAfterPrune = WorkChains.Num();

		PromoteFalseFreeEnds(WorkChains, OutJunctionTexels, Width);

		TArray<TArray<FStemMember>> Stems;
		AssembleStems(WorkChains, OutJunctionTexels.Num(), Stems);

		TArray<FWorkChain> StemChains;
		StemChains.Reserve(Stems.Num());
		for (const TArray<FStemMember>& Stem : Stems)
		{
			FWorkChain Concatenated;
			ConcatenateStem(WorkChains, Stem, Concatenated);
			if (Concatenated.LengthTexels < static_cast<double>(MinChainTexels))
			{
				continue;
			}
			StemChains.Add(MoveTemp(Concatenated));
		}
		OutStats.StemCount = StemChains.Num();

		StemChains.StableSort([](const FWorkChain& Left, const FWorkChain& Right)
			{
				return Left.LowestTexelIndex < Right.LowestTexelIndex;
			});

		TArray<int32> ForcedIndices;
		TArray<int32> RetainedIndices;
		OutChains.Reserve(StemChains.Num());
		for (FWorkChain& Work : StemChains)
		{
			ForcedIndices.Reset();
			ForcedIndices.Reserve(Work.Chain.Junctions.Num());
			for (const FArcRiverJunctionRef& Junction : Work.Chain.Junctions)
			{
				ForcedIndices.Add(Junction.PointIndex);
			}

			FArcRiverChain Simplified;
			SimplifyChain(Work.Chain.TexelXY, Work.Chain.HalfWidthTexels, SimplifyToleranceTexels,
				ForcedIndices, Simplified.TexelXY, Simplified.HalfWidthTexels, RetainedIndices);
			if (Simplified.TexelXY.Num() < 2)
			{
				continue;
			}

			Simplified.Junctions.Reserve(Work.Chain.Junctions.Num());
			for (const FArcRiverJunctionRef& Junction : Work.Chain.Junctions)
			{
				const int32 RemappedIndex = Algo::BinarySearch(RetainedIndices, Junction.PointIndex);
				if (RemappedIndex == INDEX_NONE)
				{
					UE_LOGF(LogArcRiver, Error,"River simplification dropped forced junction %d at point %d of a %d-point stem.",
						Junction.JunctionId, Junction.PointIndex, Work.Chain.TexelXY.Num());
					OutError = FText::Format(
						LOCTEXT("SimplifyDroppedJunction",
							"River simplification dropped forced junction {0} at point {1} of a {2}-point stem."),
						FText::AsNumber(Junction.JunctionId),
						FText::AsNumber(Junction.PointIndex),
						FText::AsNumber(Work.Chain.TexelXY.Num()));
					return;
				}

				FArcRiverJunctionRef& Remapped = Simplified.Junctions.AddDefaulted_GetRef();
				Remapped.JunctionId = Junction.JunctionId;
				Remapped.PointIndex = RemappedIndex;
			}

			OutChains.Add(MoveTemp(Simplified));
		}
	}
}

#undef LOCTEXT_NAMESPACE
