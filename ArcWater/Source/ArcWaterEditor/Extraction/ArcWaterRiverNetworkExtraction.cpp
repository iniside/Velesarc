// Copyright Lukasz Baran. All Rights Reserved.

#include "Extraction/ArcWaterRiverNetworkExtraction.h"

#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"

#include "Algo/Reverse.h"

#define LOCTEXT_NAMESPACE "ArcWaterRiverNetworkExtraction"

namespace ArcWaterRiverNetworkExtractionPrivate
{
	constexpr int32 NeighborDX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
	constexpr int32 NeighborDY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
	constexpr int32 CancellationStride = 65536;

	int32 FloorDivideTileCoordinate(const int32 Coordinate)
	{
		const int64 WideCoordinate = Coordinate;
		int64 TileCoordinate = WideCoordinate / ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid::TileSide;
		if (WideCoordinate < 0
			&& WideCoordinate % ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid::TileSide != 0)
		{
			--TileCoordinate;
		}
		return static_cast<int32>(TileCoordinate);
	}

	FIntPoint TileCoordinate(const FIntPoint Pixel)
	{
		return FIntPoint(FloorDivideTileCoordinate(Pixel.X), FloorDivideTileCoordinate(Pixel.Y));
	}

	int32 TilePixelIndex(const FIntPoint Pixel, const FIntPoint TileCoord)
	{
		const int64 LocalX = static_cast<int64>(Pixel.X)
			- static_cast<int64>(TileCoord.X) * ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid::TileSide;
		const int64 LocalY = static_cast<int64>(Pixel.Y)
			- static_cast<int64>(TileCoord.Y) * ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid::TileSide;
		return static_cast<int32>(LocalY * ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid::TileSide + LocalX);
	}

	bool TryNeighborCoordinate(const FIntPoint Pixel, const int32 Slot, FIntPoint& OutNeighbor)
	{
		const int64 NeighborX = static_cast<int64>(Pixel.X) + NeighborDX[Slot];
		const int64 NeighborY = static_cast<int64>(Pixel.Y) + NeighborDY[Slot];
		if (NeighborX < MIN_int32 || NeighborX > MAX_int32 || NeighborY < MIN_int32 || NeighborY > MAX_int32)
		{
			return false;
		}
		OutNeighbor = FIntPoint(static_cast<int32>(NeighborX), static_cast<int32>(NeighborY));
		return true;
	}

	bool RowMajorLess(const FIntPoint A, const FIntPoint B)
	{
		return A.Y < B.Y || (A.Y == B.Y && A.X < B.X);
	}

	void CollectActivePixels(
		const ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid& Grid,
		TArray<FIntPoint>& OutPixels,
		const bool bSortRowMajor)
	{
		OutPixels.Reset();
		OutPixels.Reserve(static_cast<int32>(Grid.ActivePixelCount));
		for (const TPair<FIntPoint, ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskTile>& Pair : Grid.Tiles)
		{
			const ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskTile& Tile = Pair.Value;
			for (TConstSetBitIterator<> Iterator(Tile.Pixels); Iterator; ++Iterator)
			{
				const int32 PixelIndex = Iterator.GetIndex();
				const int32 LocalX = PixelIndex % ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid::TileSide;
				const int32 LocalY = PixelIndex / ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid::TileSide;
				const int64 GlobalX = static_cast<int64>(Tile.TileCoord.X)
					* ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid::TileSide + LocalX;
				const int64 GlobalY = static_cast<int64>(Tile.TileCoord.Y)
					* ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid::TileSide + LocalY;
				OutPixels.Add(FIntPoint(static_cast<int32>(GlobalX), static_cast<int32>(GlobalY)));
			}
		}
		if (bSortRowMajor)
		{
			OutPixels.Sort(RowMajorLess);
		}
	}

	struct FGraphNode
	{
		int32 Pixel = INDEX_NONE;
		int32 JunctionId = INDEX_NONE;
		bool bOriginalEndpoint = false;
	};

	struct FGraphEdge
	{
		TArray<FIntPoint> DenseTexels;
		double LengthCm = 0.0;
		int32 NodeA = INDEX_NONE;
		int32 NodeB = INDEX_NONE;
		int32 Id = INDEX_NONE;
		int32 AtomicTieNodeId = INDEX_NONE;
		bool bAtomicClosed = false;
	};

	struct FPathCandidate
	{
		TArray<int32> NodeIds;
		TArray<int32> EdgeIds;
		double LengthCm = 0.0;
		int32 StartNode = INDEX_NONE;
		int32 EndNode = INDEX_NONE;
		bool bClosed = false;
		bool bValid = false;
	};

	struct FWorkingPath
	{
		ArcWaterRiverNetworkExtraction::FArcWaterRiverPath Path;
		TArray<FIntPoint> DenseTexels;
	};

	struct FDownBranch
	{
		double LengthCm = 0.0;
		int32 EndpointNode = INDEX_NONE;
		int32 FirstEdgeId = INDEX_NONE;
	};

	struct FBranchStart
	{
		FDownBranch Branch;
		int32 RootNode = INDEX_NONE;
	};

	struct FComponentPaths
	{
		TArray<FPathCandidate> Candidates;
	};

	bool ContinueOrCancel(TFunctionRef<bool(float)> ShouldContinue, const float Progress, FText& OutError)
	{
		if (ShouldContinue(FMath::Clamp(Progress, 0.0f, 1.0f)))
		{
			return true;
		}

		OutError = LOCTEXT("Canceled", "River network extraction was canceled.");
		return false;
	}

	void GatherNeighbors(
		const ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid& Grid,
		const FIntPoint Pixel,
		uint8 (&OutNeighbors)[8])
	{
		for (int32 Slot = 0; Slot < 8; ++Slot)
		{
			FIntPoint Neighbor;
			OutNeighbors[Slot] = TryNeighborCoordinate(Pixel, Slot, Neighbor) && Grid.IsActive(Neighbor) ? 1 : 0;
		}
	}

	bool IsOnSourceBoundary(const FIntPoint Pixel, const FIntRect& SourceRect)
	{
		return Pixel.X == SourceRect.Min.X || Pixel.X == SourceRect.Max.X - 1
			|| Pixel.Y == SourceRect.Min.Y || Pixel.Y == SourceRect.Max.Y - 1;
	}

	bool BuildClippedEndpointAnchors(
		const ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid& Grid,
		TSet<FIntPoint>& OutAnchors,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN BuildClippedEndpointAnchors t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END BuildClippedEndpointAnchors elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		OutAnchors.Reset();
		TArray<FIntPoint> ActivePixels;
		CollectActivePixels(Grid, ActivePixels, false);
		TSet<FIntPoint> BoundaryPixels;
		int64 WorkCount = 0;
		for (const FIntPoint Pixel : ActivePixels)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if ((WorkCount++ & 255ll) == 0
				&& !ContinueOrCancel(ShouldContinue, 0.08f, OutError))
			{
				return false;
			}
			if (IsOnSourceBoundary(Pixel, Grid.SourceRect))
			{
				BoundaryPixels.Add(Pixel);
			}
		}

		TSet<FIntPoint> UnvisitedBoundaryPixels = BoundaryPixels;
		TArray<FIntPoint> Stack;
		TArray<FIntPoint> ComponentPixels;
		for (const FIntPoint Seed : ActivePixels)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if (!UnvisitedBoundaryPixels.Remove(Seed))
			{
				continue;
			}
			Stack.Reset();
			ComponentPixels.Reset();
			Stack.Add(Seed);
			double SumX = 0.0;
			double SumY = 0.0;
			while (!Stack.IsEmpty())
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				if ((WorkCount++ & 255ll) == 0
					&& !ContinueOrCancel(ShouldContinue, 0.08f, OutError))
				{
					return false;
				}
				const FIntPoint Pixel = Stack.Pop(EAllowShrinking::No);
				ComponentPixels.Add(Pixel);
				SumX += Pixel.X;
				SumY += Pixel.Y;
				for (int32 Slot = 0; Slot < 8; ++Slot)
				{
					if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
					{
						RiverLoopHeartbeat = FPlatformTime::Seconds();
						UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
					}
					FIntPoint Neighbor;
					if (!TryNeighborCoordinate(Pixel, Slot, Neighbor)
						|| !IsOnSourceBoundary(Neighbor, Grid.SourceRect))
					{
						continue;
					}
					if (UnvisitedBoundaryPixels.Remove(Neighbor))
					{
						Stack.Add(Neighbor);
					}
				}
			}

			const double CenterX = SumX / ComponentPixels.Num();
			const double CenterY = SumY / ComponentPixels.Num();
			FIntPoint Anchor = Seed;
			double BestDistanceSquared = TNumericLimits<double>::Max();
			for (const FIntPoint Pixel : ComponentPixels)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				const double DX = static_cast<double>(Pixel.X) - CenterX;
				const double DY = static_cast<double>(Pixel.Y) - CenterY;
				const double DistanceSquared = DX * DX + DY * DY;
				if (DistanceSquared < BestDistanceSquared
					|| (DistanceSquared == BestDistanceSquared && RowMajorLess(Pixel, Anchor)))
				{
					Anchor = Pixel;
					BestDistanceSquared = DistanceSquared;
				}
			}
			OutAnchors.Add(Anchor);
		}
		return true;
	}

	bool ThinSubIteration(
		ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid& Grid,
		const TSet<FIntPoint>& ProtectedEndpoints,
		const int32 SubStep,
		TArray<FIntPoint>& ScratchMarked,
		int32& OutRemoved,
		int64& InOutVisits,
		TFunctionRef<bool(float)> ShouldContinue,
		const float Progress,
		FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN ThinSubIteration t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END ThinSubIteration elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		ScratchMarked.Reset();
		TArray<FIntPoint> ActivePixels;
		CollectActivePixels(Grid, ActivePixels, false);
		for (const FIntPoint Pixel : ActivePixels)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			++InOutVisits;
			if ((InOutVisits % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, Progress, OutError))
			{
				return false;
			}
			if (ProtectedEndpoints.Contains(Pixel))
			{
				continue;
			}

			uint8 Neighbors[8];
			GatherNeighbors(Grid, Pixel, Neighbors);

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
			if (bCornerConditions)
			{
				ScratchMarked.Add(Pixel);
			}
		}

		for (const FIntPoint Pixel : ScratchMarked)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			Grid.RemoveActivePixel(Pixel);
		}
		OutRemoved = ScratchMarked.Num();
		return true;
	}

	uint8 GetNeighborMask(
		const TArray<FIntPoint>& Pixels,
		const TMap<FIntPoint, int32>& PixelIdByCoordinate,
		const int32 Index,
		uint8 (&OutNeighbors)[8])
	{
		const FIntPoint Pixel = Pixels[Index];
		for (int32 Slot = 0; Slot < 8; ++Slot)
		{
			FIntPoint Neighbor;
			OutNeighbors[Slot] = TryNeighborCoordinate(Pixel, Slot, Neighbor)
				&& PixelIdByCoordinate.Contains(Neighbor) ? 1 : 0;
		}

		uint8 Mask = 0;
		for (int32 Slot = 0; Slot < 8; ++Slot)
		{
			if (OutNeighbors[Slot] == 0)
			{
				continue;
			}

			if ((Slot & 1) != 0)
			{
				const int32 PreviousOrthogonal = (Slot + 7) % 8;
				const int32 NextOrthogonal = (Slot + 1) % 8;
				if (OutNeighbors[PreviousOrthogonal] != 0 || OutNeighbors[NextOrthogonal] != 0)
				{
					continue;
				}
			}

			Mask |= static_cast<uint8>(1u << Slot);
		}
		return Mask;
	}

	int32 NeighborIndexInSlot(
		const TArray<FIntPoint>& Pixels,
		const TMap<FIntPoint, int32>& PixelIdByCoordinate,
		const int32 Index,
		const int32 Slot)
	{
		FIntPoint Neighbor;
		if (!TryNeighborCoordinate(Pixels[Index], Slot, Neighbor))
		{
			return INDEX_NONE;
		}
		const int32* NeighborIndex = PixelIdByCoordinate.Find(Neighbor);
		return NeighborIndex != nullptr ? *NeighborIndex : INDEX_NONE;
	}

	double StepLengthCm(const FIntPoint& A, const FIntPoint& B, const FVector2D& CmPerTexel)
	{
		const double DeltaX = (static_cast<double>(B.X) - static_cast<double>(A.X)) * CmPerTexel.X;
		const double DeltaY = (static_cast<double>(B.Y) - static_cast<double>(A.Y)) * CmPerTexel.Y;
		return FMath::Sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
	}

	double ComputePolylineLength(
		TConstArrayView<FIntPoint> Texels,
		const FVector2D& CmPerTexel,
		const bool bClosed)
	{
		double LengthCm = 0.0;
		for (int32 Index = 1; Index < Texels.Num(); ++Index)
		{
			LengthCm += StepLengthCm(Texels[Index - 1], Texels[Index], CmPerTexel);
		}
		if (bClosed && Texels.Num() > 1)
		{
			LengthCm += StepLengthCm(Texels.Last(), Texels[0], CmPerTexel);
		}
		return LengthCm;
	}

	void MarkEdgeVisited(TArray<uint8>& VisitedSlots, const int32 A, const int32 Slot, const int32 B)
	{
		VisitedSlots[A] |= static_cast<uint8>(1u << Slot);
		VisitedSlots[B] |= static_cast<uint8>(1u << ((Slot + 4) % 8));
	}

	bool IsEdgeVisited(const TArray<uint8>& VisitedSlots, const int32 Index, const int32 Slot)
	{
		return (VisitedSlots[Index] & static_cast<uint8>(1u << Slot)) != 0;
	}

	bool BuildGraphNeighborMasks(
		const TArray<FIntPoint>& Pixels,
		const TMap<FIntPoint, int32>& PixelIdByCoordinate,
		const TArray<uint8>& Degrees,
		const TArray<uint8>& ProtectedEndpoints,
		TArray<uint8>& OutNeighborMasks,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN BuildGraphNeighborMasks t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END BuildGraphNeighborMasks elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		OutNeighborMasks.SetNumUninitialized(Pixels.Num());
		TSet<int32> JunctionPixels;
		for (int32 Index = 0; Index < Pixels.Num(); ++Index)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if ((Index % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.52f, OutError))
			{
				return false;
			}
			uint8 Neighbors[8];
			OutNeighborMasks[Index] = GetNeighborMask(Pixels, PixelIdByCoordinate, Index, Neighbors);
			if (Degrees[Index] >= 3)
			{
				JunctionPixels.Add(Index);
			}
		}

		TSet<int32> AssignedJunctionPixels;
		TArray<int32> Stack;
		TArray<int32> ClusterPixels;
		TSet<int32> ClusterPixelSet;
		TMap<int32, int32> ParentByPixel;
		TSet<int32> TerminalPixels;
		TArray<int32> PruneQueue;
		TSet<int32> PrunedPixels;
		int64 ClusterSteps = 0;
		int32 ClusterCount = 0;
		int32 TreeEdgeCount = 0;
		int32 PrunedPixelCount = 0;
		for (int32 SeedIndex = 0; SeedIndex < Pixels.Num(); ++SeedIndex)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if ((SeedIndex % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.53f, OutError))
			{
				return false;
			}
			if (!JunctionPixels.Contains(SeedIndex) || AssignedJunctionPixels.Contains(SeedIndex))
			{
				continue;
			}
			++ClusterCount;

			Stack.Reset();
			ClusterPixels.Reset();
			ClusterPixelSet.Reset();
			ParentByPixel.Reset();
			Stack.Add(SeedIndex);
			AssignedJunctionPixels.Add(SeedIndex);
			ParentByPixel.Add(SeedIndex, INDEX_NONE);
			while (Stack.Num() > 0)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				++ClusterSteps;
				if ((ClusterSteps % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.53f, OutError))
				{
					return false;
				}
				const int32 Current = Stack.Pop(EAllowShrinking::No);
				ClusterPixels.Add(Current);
				ClusterPixelSet.Add(Current);
				const uint8 NeighborMask = OutNeighborMasks[Current];
				for (int32 Slot = 0; Slot < 8; ++Slot)
				{
					if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
					{
						RiverLoopHeartbeat = FPlatformTime::Seconds();
						UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
					}
					if ((NeighborMask & static_cast<uint8>(1u << Slot)) == 0)
					{
						continue;
					}
					const int32 Neighbor = NeighborIndexInSlot(Pixels, PixelIdByCoordinate, Current, Slot);
					if (!JunctionPixels.Contains(Neighbor) || AssignedJunctionPixels.Contains(Neighbor))
					{
						continue;
					}
					AssignedJunctionPixels.Add(Neighbor);
					ParentByPixel.Add(Neighbor, Current);
					Stack.Add(Neighbor);
				}
			}
			ClusterPixels.Sort();

			for (const int32 Pixel : ClusterPixels)
			{
				if ((++ClusterSteps % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.54f, OutError))
				{
					return false;
				}
				for (int32 Slot = 0; Slot < 8; ++Slot)
				{
					const uint8 SlotBit = static_cast<uint8>(1u << Slot);
					if ((OutNeighborMasks[Pixel] & SlotBit) == 0)
					{
						continue;
					}
					const int32 Neighbor = NeighborIndexInSlot(Pixels, PixelIdByCoordinate, Pixel, Slot);
					if (ClusterPixelSet.Contains(Neighbor))
					{
						OutNeighborMasks[Pixel] &= static_cast<uint8>(~SlotBit);
						OutNeighborMasks[Neighbor] &= static_cast<uint8>(~static_cast<uint8>(1u << ((Slot + 4) % 8)));
					}
				}
			}

			for (const int32 Pixel : ClusterPixels)
			{
				if ((++ClusterSteps % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.54f, OutError))
				{
					return false;
				}
				const int32* Parent = ParentByPixel.Find(Pixel);
				if (Parent == nullptr || *Parent == INDEX_NONE)
				{
					continue;
				}
				for (int32 Slot = 0; Slot < 8; ++Slot)
				{
					if (NeighborIndexInSlot(Pixels, PixelIdByCoordinate, Pixel, Slot) != *Parent)
					{
						continue;
					}
					OutNeighborMasks[Pixel] |= static_cast<uint8>(1u << Slot);
					OutNeighborMasks[*Parent] |= static_cast<uint8>(1u << ((Slot + 4) % 8));
					++TreeEdgeCount;
					break;
				}
			}

			TerminalPixels.Reset();
			for (const int32 Pixel : ClusterPixels)
			{
				if ((++ClusterSteps % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.54f, OutError))
				{
					return false;
				}
				bool bTerminal = ProtectedEndpoints[Pixel] != 0;
				for (int32 Slot = 0; Slot < 8 && !bTerminal; ++Slot)
				{
					if ((OutNeighborMasks[Pixel] & static_cast<uint8>(1u << Slot)) == 0)
					{
						continue;
					}
					const int32 Neighbor = NeighborIndexInSlot(Pixels, PixelIdByCoordinate, Pixel, Slot);
					bTerminal = !ClusterPixelSet.Contains(Neighbor);
				}
				if (bTerminal)
				{
					TerminalPixels.Add(Pixel);
				}
			}
			if (TerminalPixels.Num() == 0)
			{
				continue;
			}

			PruneQueue.Reset();
			PrunedPixels.Reset();
			for (const int32 Pixel : ClusterPixels)
			{
				if ((++ClusterSteps % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.54f, OutError))
				{
					return false;
				}
				const int32 Degree = FMath::CountBits(static_cast<uint32>(OutNeighborMasks[Pixel]));
				if (!TerminalPixels.Contains(Pixel) && Degree <= 1)
				{
					PruneQueue.Add(Pixel);
				}
			}
			int32 QueueHead = 0;
			while (QueueHead < PruneQueue.Num())
			{
				if ((++ClusterSteps % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.54f, OutError))
				{
					return false;
				}
				const int32 Pixel = PruneQueue[QueueHead++];
				if (TerminalPixels.Contains(Pixel) || PrunedPixels.Contains(Pixel)
					|| FMath::CountBits(static_cast<uint32>(OutNeighborMasks[Pixel])) > 1)
				{
					continue;
				}
				const uint8 RemovedMask = OutNeighborMasks[Pixel];
				OutNeighborMasks[Pixel] = 0;
				PrunedPixels.Add(Pixel);
				++PrunedPixelCount;
				for (int32 Slot = 0; Slot < 8; ++Slot)
				{
					if ((RemovedMask & static_cast<uint8>(1u << Slot)) == 0)
					{
						continue;
					}
					const int32 Neighbor = NeighborIndexInSlot(Pixels, PixelIdByCoordinate, Pixel, Slot);
					OutNeighborMasks[Neighbor] &= static_cast<uint8>(~static_cast<uint8>(1u << ((Slot + 4) % 8)));
					const int32 NeighborDegree = FMath::CountBits(static_cast<uint32>(OutNeighborMasks[Neighbor]));
					if (ClusterPixelSet.Contains(Neighbor) && !TerminalPixels.Contains(Neighbor) && NeighborDegree <= 1)
					{
						PruneQueue.Add(Neighbor);
					}
				}
			}
		}
		UE_LOGF(LogTemp, Display,"[RiverGen] NORMALIZE clusters=%d treeEdges=%d prunedPixels=%d",
			ClusterCount, TreeEdgeCount, PrunedPixelCount);
		return true;
	}

	bool BuildGraphNodes(
		const TArray<FIntPoint>& Pixels,
		const TArray<uint8>& OriginalDegrees,
		const TArray<uint8>& NeighborMasks,
		const TArray<uint8>& ProtectedEndpoints,
		TArray<FGraphNode>& OutNodes,
		TMap<int32, int32>& OutNodeByPixel,
		TArray<int32>& OutIsolatedPixels,
		TArray<FIntPoint>& OutJunctionTexels,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN BuildGraphNodes t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END BuildGraphNodes elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		for (int32 Index = 0; Index < Pixels.Num(); ++Index)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if ((Index % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.55f, OutError))
			{
				return false;
			}
			const int32 NormalizedDegree = FMath::CountBits(static_cast<uint32>(NeighborMasks[Index]));
			if (NormalizedDegree == 1 || NormalizedDegree >= 3)
			{
				FGraphNode& Node = OutNodes.AddDefaulted_GetRef();
				Node.Pixel = Index;
				Node.bOriginalEndpoint = OriginalDegrees[Index] == 1 && ProtectedEndpoints[Index] == 0;
				if (NormalizedDegree >= 3)
				{
					Node.JunctionId = OutJunctionTexels.Add(Pixels[Index]);
				}
			}
			else if (OriginalDegrees[Index] == 0)
			{
				OutIsolatedPixels.Add(Index);
			}
		}

		for (int32 NodeId = 0; NodeId < OutNodes.Num(); ++NodeId)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if ((NodeId & 255) == 0 && !ContinueOrCancel(ShouldContinue, 0.56f, OutError))
			{
				return false;
			}
			OutNodeByPixel.Add(OutNodes[NodeId].Pixel, NodeId);
		}
		return true;
	}

	bool TraceGraphEdge(
		const int32 StartNodeId,
		const int32 StartPixel,
		const int32 StartSlot,
		const TArray<FIntPoint>& Pixels,
		const TMap<FIntPoint, int32>& PixelIdByCoordinate,
		const TArray<uint8>& NeighborMasks,
		const TMap<int32, int32>& NodeByPixel,
		TArray<uint8>& VisitedSlots,
		FGraphEdge& OutEdge,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN TraceGraphEdge t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END TraceGraphEdge elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		OutEdge.DenseTexels.Add(Pixels[StartPixel]);
		int32 Previous = StartPixel;
		int32 Current = NeighborIndexInSlot(Pixels, PixelIdByCoordinate, StartPixel, StartSlot);
		MarkEdgeVisited(VisitedSlots, Previous, StartSlot, Current);
		OutEdge.DenseTexels.Add(Pixels[Current]);

		for (int32 Step = 0; Step <= NeighborMasks.Num(); ++Step)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if ((Step % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.59f, OutError))
			{
				return false;
			}
			const int32* EndNodeId = NodeByPixel.Find(Current);
			if (EndNodeId != nullptr)
			{
				OutEdge.NodeA = StartNodeId;
				OutEdge.NodeB = *EndNodeId;
				return true;
			}

			const uint8 NeighborMask = NeighborMasks[Current];
			int32 NextSlot = INDEX_NONE;
			for (int32 Slot = 0; Slot < 8; ++Slot)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				if ((NeighborMask & static_cast<uint8>(1u << Slot)) == 0)
				{
					continue;
				}
				const int32 Candidate = NeighborIndexInSlot(Pixels, PixelIdByCoordinate, Current, Slot);
				if (Candidate != Previous)
				{
					NextSlot = Slot;
					break;
				}
			}
			if (NextSlot == INDEX_NONE)
			{
				OutError = LOCTEXT("BrokenSkeletonEdge", "A skeleton edge ended without reaching an endpoint or junction.");
				return false;
			}

			const int32 Next = NeighborIndexInSlot(Pixels, PixelIdByCoordinate, Current, NextSlot);
			MarkEdgeVisited(VisitedSlots, Current, NextSlot, Next);
			Previous = Current;
			Current = Next;
			OutEdge.DenseTexels.Add(Pixels[Current]);
		}

		OutError = LOCTEXT("SkeletonEdgeDidNotTerminate", "A skeleton edge traversal did not terminate.");
		return false;
	}

	bool TraceStandaloneCycle(
		const int32 StartPixel,
		const int32 StartSlot,
		const TArray<FIntPoint>& Pixels,
		const TMap<FIntPoint, int32>& PixelIdByCoordinate,
		const TArray<uint8>& NeighborMasks,
		TArray<uint8>& VisitedSlots,
		FGraphEdge& OutEdge,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN TraceStandaloneCycle t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END TraceStandaloneCycle elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		OutEdge.DenseTexels.Add(Pixels[StartPixel]);
		int32 Previous = StartPixel;
		int32 Current = NeighborIndexInSlot(Pixels, PixelIdByCoordinate, StartPixel, StartSlot);
		MarkEdgeVisited(VisitedSlots, Previous, StartSlot, Current);
		OutEdge.DenseTexels.Add(Pixels[Current]);

		for (int32 Step = 0; Step <= NeighborMasks.Num(); ++Step)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if ((Step % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.63f, OutError))
			{
				return false;
			}
			if (Current == StartPixel)
			{
				OutEdge.DenseTexels.Pop(EAllowShrinking::No);
				OutEdge.bAtomicClosed = true;
				return true;
			}

			const uint8 NeighborMask = NeighborMasks[Current];
			int32 NextSlot = INDEX_NONE;
			for (int32 Slot = 0; Slot < 8; ++Slot)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				if ((NeighborMask & static_cast<uint8>(1u << Slot)) == 0)
				{
					continue;
				}
				const int32 Candidate = NeighborIndexInSlot(Pixels, PixelIdByCoordinate, Current, Slot);
				if (Candidate != Previous)
				{
					NextSlot = Slot;
					break;
				}
			}
			if (NextSlot == INDEX_NONE)
			{
				OutError = LOCTEXT("BrokenSkeletonCycle", "A degree-two skeleton component did not form a closed cycle.");
				return false;
			}

			const int32 Next = NeighborIndexInSlot(Pixels, PixelIdByCoordinate, Current, NextSlot);
			MarkEdgeVisited(VisitedSlots, Current, NextSlot, Next);
			Previous = Current;
			Current = Next;
			OutEdge.DenseTexels.Add(Pixels[Current]);
		}

		OutError = LOCTEXT("SkeletonCycleDidNotTerminate", "A skeleton cycle traversal did not terminate.");
		return false;
	}

	bool BuildGraphEdges(
		const TArray<FIntPoint>& Pixels,
		const TMap<FIntPoint, int32>& PixelIdByCoordinate,
		const TArray<uint8>& NeighborMasks,
		const FVector2D& CmPerTexel,
		const TArray<FGraphNode>& Nodes,
		const TMap<int32, int32>& NodeByPixel,
		TArray<FGraphEdge>& OutEdges,
		TArray<TArray<int32>>& OutAdjacency,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN BuildGraphEdges t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END BuildGraphEdges elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		TArray<uint8> VisitedSlots;
		VisitedSlots.Init(0, NeighborMasks.Num());

		for (int32 NodeId = 0; NodeId < Nodes.Num(); ++NodeId)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if ((NodeId & 255) == 0 && !ContinueOrCancel(ShouldContinue, 0.58f, OutError))
			{
				return false;
			}

			const int32 Pixel = Nodes[NodeId].Pixel;
			const uint8 NeighborMask = NeighborMasks[Pixel];
			for (int32 Slot = 0; Slot < 8; ++Slot)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				if ((NeighborMask & static_cast<uint8>(1u << Slot)) == 0 || IsEdgeVisited(VisitedSlots, Pixel, Slot))
				{
					continue;
				}

				FGraphEdge Edge;
				if (!TraceGraphEdge(NodeId, Pixel, Slot, Pixels, PixelIdByCoordinate, NeighborMasks, NodeByPixel,
					VisitedSlots, Edge, ShouldContinue, OutError))
				{
					return false;
				}

				if (Edge.NodeA == Edge.NodeB)
				{
					Edge.bAtomicClosed = true;
					if (Edge.DenseTexels.Num() > 1 && Edge.DenseTexels[0] == Edge.DenseTexels.Last())
					{
						Edge.DenseTexels.Pop(EAllowShrinking::No);
					}
				}
				Edge.LengthCm = ComputePolylineLength(Edge.DenseTexels, CmPerTexel, Edge.bAtomicClosed);
				Edge.Id = OutEdges.Num();
				OutEdges.Add(MoveTemp(Edge));
			}
		}

		for (int32 Pixel = 0; Pixel < NeighborMasks.Num(); ++Pixel)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if ((Pixel % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.62f, OutError))
			{
				return false;
			}
			if (FMath::CountBits(static_cast<uint32>(NeighborMasks[Pixel])) != 2 || NodeByPixel.Contains(Pixel))
			{
				continue;
			}

			const uint8 NeighborMask = NeighborMasks[Pixel];
			for (int32 Slot = 0; Slot < 8; ++Slot)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				if ((NeighborMask & static_cast<uint8>(1u << Slot)) == 0 || IsEdgeVisited(VisitedSlots, Pixel, Slot))
				{
					continue;
				}

				FGraphEdge Edge;
				if (!TraceStandaloneCycle(Pixel, Slot, Pixels, PixelIdByCoordinate, NeighborMasks, VisitedSlots, Edge, ShouldContinue, OutError))
				{
					return false;
				}
				Edge.LengthCm = ComputePolylineLength(Edge.DenseTexels, CmPerTexel, true);
				Edge.Id = OutEdges.Num();
				OutEdges.Add(MoveTemp(Edge));
			}
		}

		OutAdjacency.SetNum(Nodes.Num());
		int32 NextAtomicNodeId = Nodes.Num();
		int32 AdjacencyBuildCount = 0;
		for (FGraphEdge& Edge : OutEdges)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			++AdjacencyBuildCount;
			if ((AdjacencyBuildCount % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.66f, OutError))
			{
				return false;
			}
			if (Edge.bAtomicClosed)
			{
				Edge.AtomicTieNodeId = Edge.NodeA != INDEX_NONE ? Edge.NodeA : NextAtomicNodeId++;
				continue;
			}
			if (!OutAdjacency.IsValidIndex(Edge.NodeA) || !OutAdjacency.IsValidIndex(Edge.NodeB))
			{
				OutError = LOCTEXT("InvalidGraphEdge", "A skeleton edge did not resolve to two graph nodes.");
				return false;
			}
			OutAdjacency[Edge.NodeA].Add(Edge.Id);
			OutAdjacency[Edge.NodeB].Add(Edge.Id);
		}
		return true;
	}

	bool LexicographicallyLess(TConstArrayView<int32> Left, TConstArrayView<int32> Right)
	{
		const int32 CommonNum = FMath::Min(Left.Num(), Right.Num());
		for (int32 Index = 0; Index < CommonNum; ++Index)
		{
			if (Left[Index] != Right[Index])
			{
				return Left[Index] < Right[Index];
			}
		}
		return Left.Num() < Right.Num();
	}

	void CanonicalizeCandidate(FPathCandidate& Candidate)
	{
		if (Candidate.NodeIds.Num() < 2)
		{
			Candidate.StartNode = Candidate.NodeIds.Num() == 1 ? Candidate.NodeIds[0] : Candidate.StartNode;
			Candidate.EndNode = Candidate.StartNode;
			return;
		}

		Candidate.StartNode = Candidate.NodeIds[0];
		Candidate.EndNode = Candidate.NodeIds.Last();
		if (Candidate.StartNode > Candidate.EndNode)
		{
			Algo::Reverse(Candidate.NodeIds);
			Algo::Reverse(Candidate.EdgeIds);
			Swap(Candidate.StartNode, Candidate.EndNode);
		}
	}

	bool IsCandidateBetter(const FPathCandidate& Candidate, const FPathCandidate& CurrentBest)
	{
		if (!Candidate.bValid)
		{
			return false;
		}
		if (!CurrentBest.bValid)
		{
			return true;
		}
		if (Candidate.LengthCm != CurrentBest.LengthCm)
		{
			return Candidate.LengthCm > CurrentBest.LengthCm;
		}
		if (Candidate.StartNode != CurrentBest.StartNode)
		{
			return Candidate.StartNode < CurrentBest.StartNode;
		}
		if (Candidate.EndNode != CurrentBest.EndNode)
		{
			return Candidate.EndNode < CurrentBest.EndNode;
		}
		return LexicographicallyLess(Candidate.EdgeIds, CurrentBest.EdgeIds);
	}

	void ConsiderCandidate(FPathCandidate Candidate, FPathCandidate& InOutBest)
	{
		CanonicalizeCandidate(Candidate);
		if (IsCandidateBetter(Candidate, InOutBest))
		{
			InOutBest = MoveTemp(Candidate);
		}
	}

	bool IsBranchBetter(const FDownBranch& Candidate, const FDownBranch& Current)
	{
		if (Candidate.EndpointNode == INDEX_NONE)
		{
			return false;
		}
		if (Current.EndpointNode == INDEX_NONE || Candidate.LengthCm != Current.LengthCm)
		{
			return Current.EndpointNode == INDEX_NONE || Candidate.LengthCm > Current.LengthCm;
		}
		if (Candidate.EndpointNode != Current.EndpointNode)
		{
			return Candidate.EndpointNode < Current.EndpointNode;
		}
		return Candidate.FirstEdgeId < Current.FirstEdgeId;
	}

	bool BuildPathBetweenTreeNodes(
		const int32 StartNode,
		const int32 EndNode,
		const TArray<TArray<int32>>& Adjacency,
		const TArray<FGraphEdge>& Edges,
		const TBitArray<>& TreeEdges,
		FPathCandidate& InOutCandidate,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		TArray<int32> ParentNode;
		TArray<int32> ParentEdge;
		ParentNode.Init(INDEX_NONE, Adjacency.Num());
		ParentEdge.Init(INDEX_NONE, Adjacency.Num());
		TArray<int32> Stack;
		Stack.Add(StartNode);
		ParentNode[StartNode] = StartNode;
		int32 TraversalCount = 0;
		while (Stack.Num() > 0 && ParentNode[EndNode] == INDEX_NONE)
		{
			if ((++TraversalCount & 1023) == 0 && !ContinueOrCancel(ShouldContinue, 0.75f, OutError))
			{
				return false;
			}
			const int32 NodeId = Stack.Pop(EAllowShrinking::No);
			for (const int32 EdgeId : Adjacency[NodeId])
			{
				const FGraphEdge& Edge = Edges[EdgeId];
				if (!TreeEdges[EdgeId])
				{
					continue;
				}
				const int32 NextNode = Edge.NodeA == NodeId ? Edge.NodeB : Edge.NodeA;
				if (ParentNode[NextNode] != INDEX_NONE)
				{
					continue;
				}
				ParentNode[NextNode] = NodeId;
				ParentEdge[NextNode] = EdgeId;
				Stack.Add(NextNode);
			}
		}
		if (ParentNode[EndNode] == INDEX_NONE)
		{
			return false;
		}

		TArray<int32> ReverseNodes;
		TArray<int32> ReverseEdges;
		int32 Current = EndNode;
		ReverseNodes.Add(Current);
		int32 ReconstructionCount = 0;
		while (Current != StartNode)
		{
			if ((++ReconstructionCount & 1023) == 0 && !ContinueOrCancel(ShouldContinue, 0.75f, OutError))
			{
				return false;
			}
			ReverseEdges.Add(ParentEdge[Current]);
			Current = ParentNode[Current];
			ReverseNodes.Add(Current);
		}
		Algo::Reverse(ReverseNodes);
		Algo::Reverse(ReverseEdges);
		InOutCandidate.NodeIds = MoveTemp(ReverseNodes);
		InOutCandidate.EdgeIds = MoveTemp(ReverseEdges);
		CanonicalizeCandidate(InOutCandidate);
		return true;
	}

	bool FindTreeDiameter(
		const TArray<int32>& ComponentNodes,
		const TArray<TArray<int32>>& Adjacency,
		const TArray<FGraphEdge>& Edges,
		const TBitArray<>& TreeEdges,
		FPathCandidate& OutCandidate,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN FindTreeDiameter t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END FindTreeDiameter elapsed=%.3fs",
				FPlatformTime::Seconds() - RiverLogStart);
		};
		TArray<int32> ParentNode;
		ParentNode.Init(INDEX_NONE, Adjacency.Num());
		TArray<int32> Order;
		Order.Reserve(ComponentNodes.Num());
		Order.Add(ComponentNodes[0]);
		ParentNode[ComponentNodes[0]] = ComponentNodes[0];
		for (int32 OrderIndex = 0; OrderIndex < Order.Num(); ++OrderIndex)
		{
			if ((OrderIndex & 1023) == 0 && !ContinueOrCancel(ShouldContinue, 0.72f, OutError))
			{
				return false;
			}
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			const int32 NodeId = Order[OrderIndex];
			for (const int32 EdgeId : Adjacency[NodeId])
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				const FGraphEdge& Edge = Edges[EdgeId];
				if (!TreeEdges[EdgeId])
				{
					continue;
				}
				const int32 NextNode = Edge.NodeA == NodeId ? Edge.NodeB : Edge.NodeA;
				if (ParentNode[NextNode] != INDEX_NONE)
				{
					continue;
				}
				ParentNode[NextNode] = NodeId;
				Order.Add(NextNode);
			}
		}

		TArray<double> DownLength;
		TArray<int32> DownEndpoint;
		DownLength.Init(0.0, Adjacency.Num());
		DownEndpoint.Init(INDEX_NONE, Adjacency.Num());
		FPathCandidate BestEndpoints;
		for (int32 ReverseIndex = Order.Num() - 1; ReverseIndex >= 0; --ReverseIndex)
		{
			if ((ReverseIndex & 1023) == 0 && !ContinueOrCancel(ShouldContinue, 0.74f, OutError))
			{
				return false;
			}
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			const int32 NodeId = Order[ReverseIndex];
			DownEndpoint[NodeId] = NodeId;
			FDownBranch BestBranch;
			FDownBranch SecondBranch;
			for (const int32 EdgeId : Adjacency[NodeId])
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				const FGraphEdge& Edge = Edges[EdgeId];
				if (!TreeEdges[EdgeId])
				{
					continue;
				}
				const int32 ChildNode = Edge.NodeA == NodeId ? Edge.NodeB : Edge.NodeA;
				if (ParentNode[ChildNode] != NodeId)
				{
					continue;
				}

				FDownBranch Branch;
				Branch.LengthCm = Edge.LengthCm + DownLength[ChildNode];
				Branch.EndpointNode = DownEndpoint[ChildNode];
				Branch.FirstEdgeId = EdgeId;
				if (IsBranchBetter(Branch, BestBranch))
				{
					SecondBranch = BestBranch;
					BestBranch = Branch;
				}
				else if (IsBranchBetter(Branch, SecondBranch))
				{
					SecondBranch = Branch;
				}
			}

			if (BestBranch.EndpointNode != INDEX_NONE)
			{
				DownLength[NodeId] = BestBranch.LengthCm;
				DownEndpoint[NodeId] = BestBranch.EndpointNode;

				FPathCandidate EndpointPath;
				EndpointPath.NodeIds = {NodeId, BestBranch.EndpointNode};
				EndpointPath.LengthCm = BestBranch.LengthCm;
				EndpointPath.bValid = true;
				ConsiderCandidate(MoveTemp(EndpointPath), BestEndpoints);
			}
			if (SecondBranch.EndpointNode != INDEX_NONE)
			{
				FPathCandidate ThroughPath;
				ThroughPath.NodeIds = {BestBranch.EndpointNode, SecondBranch.EndpointNode};
				ThroughPath.LengthCm = BestBranch.LengthCm + SecondBranch.LengthCm;
				ThroughPath.bValid = true;
				ConsiderCandidate(MoveTemp(ThroughPath), BestEndpoints);
			}
		}

		if (!BestEndpoints.bValid)
		{
			return false;
		}
		OutCandidate = BestEndpoints;
		return BuildPathBetweenTreeNodes(
			BestEndpoints.StartNode, BestEndpoints.EndNode, Adjacency, Edges, TreeEdges,
			OutCandidate, ShouldContinue, OutError);
	}

	bool IsBranchStartBetter(const FBranchStart& Candidate, const FBranchStart& Current)
	{
		if (IsBranchBetter(Candidate.Branch, Current.Branch))
		{
			return true;
		}
		if (IsBranchBetter(Current.Branch, Candidate.Branch))
		{
			return false;
		}
		return Candidate.RootNode < Current.RootNode;
	}

	void SortBranchStarts(TArray<FBranchStart>& Starts)
	{
		Starts.Sort([](const FBranchStart& Left, const FBranchStart& Right)
		{
			return IsBranchStartBetter(Left, Right);
		});
	}

	bool AppendRootedBranches(
		const TArray<int32>& ComponentNodes,
		const FPathCandidate& MainCandidate,
		const TArray<TArray<int32>>& Adjacency,
		const TArray<FGraphEdge>& Edges,
		const TBitArray<>& TreeEdges,
		TArray<FPathCandidate>& OutCandidates,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		TBitArray<> MainEdges(false, Edges.Num());
		for (const int32 EdgeId : MainCandidate.EdgeIds)
		{
			MainEdges[EdgeId] = true;
		}

		TArray<int32> ParentNode;
		ParentNode.Init(INDEX_NONE, Adjacency.Num());
		TArray<int32> Order;
		Order.Reserve(ComponentNodes.Num());
		for (const int32 MainNode : MainCandidate.NodeIds)
		{
			ParentNode[MainNode] = MainNode;
			Order.Add(MainNode);
		}
		for (int32 OrderIndex = 0; OrderIndex < Order.Num(); ++OrderIndex)
		{
			if ((OrderIndex & 1023) == 0 && !ContinueOrCancel(ShouldContinue, 0.76f, OutError))
			{
				return false;
			}
			const int32 NodeId = Order[OrderIndex];
			for (const int32 EdgeId : Adjacency[NodeId])
			{
				if (!TreeEdges[EdgeId] || MainEdges[EdgeId])
				{
					continue;
				}
				const FGraphEdge& Edge = Edges[EdgeId];
				const int32 NextNode = Edge.NodeA == NodeId ? Edge.NodeB : Edge.NodeA;
				if (ParentNode[NextNode] != INDEX_NONE)
				{
					continue;
				}
				ParentNode[NextNode] = NodeId;
				Order.Add(NextNode);
			}
		}
		if (Order.Num() != ComponentNodes.Num())
		{
			OutError = LOCTEXT("RootedTreeFailed", "A river component could not be rooted from its main path.");
			return false;
		}

		TArray<double> DownLength;
		TArray<int32> DownEndpoint;
		TArray<int32> BestChildEdge;
		DownLength.Init(0.0, Adjacency.Num());
		DownEndpoint.Init(INDEX_NONE, Adjacency.Num());
		BestChildEdge.Init(INDEX_NONE, Adjacency.Num());
		for (int32 ReverseIndex = Order.Num() - 1; ReverseIndex >= 0; --ReverseIndex)
		{
			if ((ReverseIndex & 1023) == 0 && !ContinueOrCancel(ShouldContinue, 0.78f, OutError))
			{
				return false;
			}
			const int32 NodeId = Order[ReverseIndex];
			DownEndpoint[NodeId] = NodeId;
			FDownBranch BestBranch;
			for (const int32 EdgeId : Adjacency[NodeId])
			{
				if (!TreeEdges[EdgeId] || MainEdges[EdgeId])
				{
					continue;
				}
				const FGraphEdge& Edge = Edges[EdgeId];
				const int32 ChildNode = Edge.NodeA == NodeId ? Edge.NodeB : Edge.NodeA;
				if (ParentNode[ChildNode] != NodeId)
				{
					continue;
				}
				FDownBranch Branch;
				Branch.LengthCm = Edge.LengthCm + DownLength[ChildNode];
				Branch.EndpointNode = DownEndpoint[ChildNode];
				Branch.FirstEdgeId = EdgeId;
				if (IsBranchBetter(Branch, BestBranch))
				{
					BestBranch = Branch;
				}
			}
			if (BestBranch.EndpointNode != INDEX_NONE)
			{
				DownLength[NodeId] = BestBranch.LengthCm;
				DownEndpoint[NodeId] = BestBranch.EndpointNode;
				BestChildEdge[NodeId] = BestBranch.FirstEdgeId;
			}
		}

		TArray<FBranchStart> Pending;
		for (const int32 MainNode : MainCandidate.NodeIds)
		{
			for (const int32 EdgeId : Adjacency[MainNode])
			{
				if (!TreeEdges[EdgeId] || MainEdges[EdgeId])
				{
					continue;
				}
				const FGraphEdge& Edge = Edges[EdgeId];
				const int32 ChildNode = Edge.NodeA == MainNode ? Edge.NodeB : Edge.NodeA;
				if (ParentNode[ChildNode] != MainNode)
				{
					continue;
				}
				FBranchStart& Start = Pending.AddDefaulted_GetRef();
				Start.RootNode = MainNode;
				Start.Branch.LengthCm = Edge.LengthCm + DownLength[ChildNode];
				Start.Branch.EndpointNode = DownEndpoint[ChildNode];
				Start.Branch.FirstEdgeId = EdgeId;
			}
		}
		SortBranchStarts(Pending);
		Algo::Reverse(Pending);

		int32 BranchCount = 0;
		int32 BranchEdgeCount = 0;
		while (Pending.Num() > 0)
		{
			if ((++BranchCount & 1023) == 0 && !ContinueOrCancel(ShouldContinue, 0.8f, OutError))
			{
				return false;
			}
			const FBranchStart Start = Pending.Pop(EAllowShrinking::No);
			FPathCandidate Candidate;
			Candidate.NodeIds.Add(Start.RootNode);
			Candidate.StartNode = Start.RootNode;
			Candidate.bValid = true;
			TArray<FBranchStart> ChildStarts;
			int32 CurrentNode = Start.RootNode;
			int32 CurrentEdge = Start.Branch.FirstEdgeId;
			while (CurrentEdge != INDEX_NONE)
			{
				if ((++BranchEdgeCount & 1023) == 0 && !ContinueOrCancel(ShouldContinue, 0.8f, OutError))
				{
					return false;
				}
				const FGraphEdge& Edge = Edges[CurrentEdge];
				const int32 NextNode = Edge.NodeA == CurrentNode ? Edge.NodeB : Edge.NodeA;
				Candidate.EdgeIds.Add(CurrentEdge);
				Candidate.NodeIds.Add(NextNode);
				Candidate.LengthCm += Edge.LengthCm;
				const int32 ContinuationEdge = BestChildEdge[NextNode];
				for (const int32 ChildEdgeId : Adjacency[NextNode])
				{
					if (!TreeEdges[ChildEdgeId] || MainEdges[ChildEdgeId] || ChildEdgeId == ContinuationEdge)
					{
						continue;
					}
					const FGraphEdge& ChildEdge = Edges[ChildEdgeId];
					const int32 ChildNode = ChildEdge.NodeA == NextNode ? ChildEdge.NodeB : ChildEdge.NodeA;
					if (ParentNode[ChildNode] != NextNode)
					{
						continue;
					}
					FBranchStart& ChildStart = ChildStarts.AddDefaulted_GetRef();
					ChildStart.RootNode = NextNode;
					ChildStart.Branch.LengthCm = ChildEdge.LengthCm + DownLength[ChildNode];
					ChildStart.Branch.EndpointNode = DownEndpoint[ChildNode];
					ChildStart.Branch.FirstEdgeId = ChildEdgeId;
				}
				CurrentNode = NextNode;
				CurrentEdge = ContinuationEdge;
			}
			Candidate.EndNode = Candidate.NodeIds.Last();
			OutCandidates.Add(MoveTemp(Candidate));
			SortBranchStarts(ChildStarts);
			for (int32 ChildIndex = ChildStarts.Num() - 1; ChildIndex >= 0; --ChildIndex)
			{
				Pending.Add(ChildStarts[ChildIndex]);
			}
		}
		return true;
	}

	int32 FindUnionRoot(TArray<int32>& UnionParent, const int32 NodeId)
	{
		int32 Root = NodeId;
		while (UnionParent[Root] != Root)
		{
			Root = UnionParent[Root];
		}
		int32 Current = NodeId;
		while (UnionParent[Current] != Current)
		{
			const int32 Next = UnionParent[Current];
			UnionParent[Current] = Root;
			Current = Next;
		}
		return Root;
	}

	bool DecomposeGraphPaths(
		const TArray<TArray<int32>>& Adjacency,
		const TArray<FGraphEdge>& Edges,
		TArray<FPathCandidate>& OutCandidates,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		OutCandidates.Reset();
		TArray<FComponentPaths> Components;
		for (const FGraphEdge& Edge : Edges)
		{
			if (!Edge.bAtomicClosed)
			{
				continue;
			}
			FComponentPaths& Component = Components.AddDefaulted_GetRef();
			FPathCandidate& Candidate = Component.Candidates.AddDefaulted_GetRef();
			Candidate.NodeIds.Add(Edge.AtomicTieNodeId);
			Candidate.EdgeIds.Add(Edge.Id);
			Candidate.LengthCm = Edge.LengthCm;
			Candidate.bClosed = true;
			Candidate.bValid = true;
			CanonicalizeCandidate(Candidate);
		}

		TBitArray<> VisitedNodes(false, Adjacency.Num());
		TBitArray<> ComponentEdgeSeen(false, Edges.Num());
		TBitArray<> TreeEdges(false, Edges.Num());
		TArray<int32> UnionParent;
		UnionParent.SetNumUninitialized(Adjacency.Num());
		TArray<int32> Queue;
		int32 ComponentCount = 0;
		int32 DecompositionEdgeCount = 0;
		for (int32 SeedNode = 0; SeedNode < Adjacency.Num(); ++SeedNode)
		{
			if (VisitedNodes[SeedNode] || Adjacency[SeedNode].Num() == 0)
			{
				continue;
			}
			if ((++ComponentCount & 255) == 0 && !ContinueOrCancel(ShouldContinue, 0.7f, OutError))
			{
				return false;
			}

			TArray<int32> ComponentNodes;
			TArray<int32> ComponentEdges;
			Queue.Reset();
			Queue.Add(SeedNode);
			VisitedNodes[SeedNode] = true;
			for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
			{
				if ((QueueIndex & 1023) == 0 && !ContinueOrCancel(ShouldContinue, 0.7f, OutError))
				{
					return false;
				}
				const int32 NodeId = Queue[QueueIndex];
				ComponentNodes.Add(NodeId);
				for (const int32 EdgeId : Adjacency[NodeId])
				{
					const FGraphEdge& Edge = Edges[EdgeId];
					if (Edge.bAtomicClosed)
					{
						continue;
					}
					if (!ComponentEdgeSeen[EdgeId])
					{
						ComponentEdgeSeen[EdgeId] = true;
						ComponentEdges.Add(EdgeId);
					}
					const int32 NextNode = Edge.NodeA == NodeId ? Edge.NodeB : Edge.NodeA;
					if (!VisitedNodes[NextNode])
					{
						VisitedNodes[NextNode] = true;
						Queue.Add(NextNode);
					}
				}
			}

			ComponentNodes.Sort();
			ComponentEdges.Sort();
			for (const int32 NodeId : ComponentNodes)
			{
				UnionParent[NodeId] = NodeId;
			}
			TArray<int32> ExcludedEdges;
			for (const int32 EdgeId : ComponentEdges)
			{
				if ((++DecompositionEdgeCount & 1023) == 0 && !ContinueOrCancel(ShouldContinue, 0.71f, OutError))
				{
					return false;
				}
				const FGraphEdge& Edge = Edges[EdgeId];
				const int32 RootA = FindUnionRoot(UnionParent, Edge.NodeA);
				const int32 RootB = FindUnionRoot(UnionParent, Edge.NodeB);
				if (RootA == RootB)
				{
					ExcludedEdges.Add(EdgeId);
					continue;
				}
				TreeEdges[EdgeId] = true;
				const int32 LowerRoot = FMath::Min(RootA, RootB);
				const int32 HigherRoot = FMath::Max(RootA, RootB);
				UnionParent[HigherRoot] = LowerRoot;
			}

			FComponentPaths Component;
			if (ComponentEdges.Num() - ExcludedEdges.Num() > 0)
			{
				FPathCandidate MainCandidate;
				if (!FindTreeDiameter(ComponentNodes, Adjacency, Edges, TreeEdges, MainCandidate, ShouldContinue, OutError))
				{
					if (OutError.IsEmpty())
					{
						OutError = LOCTEXT("TreeDiameterFailed", "A tree component did not produce a weighted diameter.");
					}
					return false;
				}
				MainCandidate.bValid = true;
				Component.Candidates.Add(MainCandidate);
				if (!AppendRootedBranches(ComponentNodes, MainCandidate, Adjacency, Edges, TreeEdges,
					Component.Candidates, ShouldContinue, OutError))
				{
					return false;
				}
			}
			for (const int32 EdgeId : ExcludedEdges)
			{
				const FGraphEdge& Edge = Edges[EdgeId];
				FPathCandidate& Candidate = Component.Candidates.AddDefaulted_GetRef();
				Candidate.NodeIds = {Edge.NodeA, Edge.NodeB};
				Candidate.EdgeIds.Add(EdgeId);
				Candidate.LengthCm = Edge.LengthCm;
				Candidate.StartNode = Edge.NodeA;
				Candidate.EndNode = Edge.NodeB;
				Candidate.bValid = true;
			}
			if (Component.Candidates.Num() > 0)
			{
				Components.Add(MoveTemp(Component));
			}
			for (const int32 EdgeId : ComponentEdges)
			{
				ComponentEdgeSeen[EdgeId] = false;
				TreeEdges[EdgeId] = false;
			}
		}

		Components.Sort([](const FComponentPaths& Left, const FComponentPaths& Right)
		{
			if (IsCandidateBetter(Left.Candidates[0], Right.Candidates[0]))
			{
				return true;
			}
			if (IsCandidateBetter(Right.Candidates[0], Left.Candidates[0]))
			{
				return false;
			}
			return Left.Candidates[0].EdgeIds[0] < Right.Candidates[0].EdgeIds[0];
		});
		for (FComponentPaths& Component : Components)
		{
			OutCandidates.Append(MoveTemp(Component.Candidates));
		}
		return true;
	}

	void AddJunctionRef(
		const FGraphNode& Node,
		const int32 PointIndex,
		ArcWaterRiverNetworkExtraction::FArcWaterRiverPath& Path)
	{
		if (Node.JunctionId == INDEX_NONE)
		{
			return;
		}
		ArcWaterRiverNetworkExtraction::FArcWaterRiverJunctionRef& Ref = Path.Junctions.AddDefaulted_GetRef();
		Ref.JunctionId = Node.JunctionId;
		Ref.PointIndex = PointIndex;
	}

	void AppendOrientedEdge(
		const FGraphEdge& Edge,
		const int32 FromNode,
		TArray<FIntPoint>& InOutTexels)
	{
		const bool bForward = Edge.NodeA == FromNode;
		if (bForward)
		{
			for (int32 Index = 0; Index < Edge.DenseTexels.Num(); ++Index)
			{
				if (InOutTexels.Num() == 0 || InOutTexels.Last() != Edge.DenseTexels[Index])
				{
					InOutTexels.Add(Edge.DenseTexels[Index]);
				}
			}
		}
		else
		{
			for (int32 Index = Edge.DenseTexels.Num() - 1; Index >= 0; --Index)
			{
				if (InOutTexels.Num() == 0 || InOutTexels.Last() != Edge.DenseTexels[Index])
				{
					InOutTexels.Add(Edge.DenseTexels[Index]);
				}
			}
		}
	}

	bool BuildWorkingPath(
		const FPathCandidate& Candidate,
		const TArray<FGraphNode>& Nodes,
		const TArray<FGraphEdge>& Edges,
		const FVector2D& CmPerTexel,
		FWorkingPath& OutPath,
		FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN BuildWorkingPath t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END BuildWorkingPath elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		OutPath.Path.LengthCm = Candidate.LengthCm;
		OutPath.Path.bClosed = Candidate.bClosed;
		if (Candidate.bClosed)
		{
			if (Candidate.EdgeIds.Num() != 1)
			{
				OutError = LOCTEXT("InvalidClosedCandidate", "A closed river candidate did not resolve to one cycle edge.");
				return false;
			}
			const FGraphEdge& Edge = Edges[Candidate.EdgeIds[0]];
			OutPath.DenseTexels = Edge.DenseTexels;
			if (Edge.NodeA != INDEX_NONE)
			{
				AddJunctionRef(Nodes[Edge.NodeA], 0, OutPath.Path);
			}
		}
		else
		{
			if (Candidate.NodeIds.Num() != Candidate.EdgeIds.Num() + 1)
			{
				OutError = LOCTEXT("InvalidOpenCandidate", "An open river candidate has inconsistent node and edge sequences.");
				return false;
			}
			AddJunctionRef(Nodes[Candidate.NodeIds[0]], 0, OutPath.Path);
			for (int32 EdgeIndex = 0; EdgeIndex < Candidate.EdgeIds.Num(); ++EdgeIndex)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				AppendOrientedEdge(Edges[Candidate.EdgeIds[EdgeIndex]], Candidate.NodeIds[EdgeIndex], OutPath.DenseTexels);
				AddJunctionRef(Nodes[Candidate.NodeIds[EdgeIndex + 1]], OutPath.DenseTexels.Num() - 1, OutPath.Path);
			}
			OutPath.Path.bStartIsOriginalEndpoint = Nodes[Candidate.NodeIds[0]].bOriginalEndpoint;
			OutPath.Path.bEndIsOriginalEndpoint = Nodes[Candidate.NodeIds.Last()].bOriginalEndpoint;
		}

		OutPath.Path.Points.Reserve(OutPath.DenseTexels.Num());
		for (const FIntPoint& Texel : OutPath.DenseTexels)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			OutPath.Path.Points.Add(FVector2D(
				static_cast<double>(Texel.X) * CmPerTexel.X,
				static_cast<double>(Texel.Y) * CmPerTexel.Y));
		}
		return true;
	}

	uint64 TexelKey(const FIntPoint& Texel)
	{
		return (static_cast<uint64>(static_cast<uint32>(Texel.X)) << 32)
			| static_cast<uint64>(static_cast<uint32>(Texel.Y));
	}

	void AddEdgeCorridor(const FIntPoint& A, const FIntPoint& B, TSet<uint64>& InOutAllowed)
	{
		InOutAllowed.Add(TexelKey(A));
		InOutAllowed.Add(TexelKey(B));
		if (A.X != B.X && A.Y != B.Y)
		{
			InOutAllowed.Add(TexelKey(FIntPoint(B.X, A.Y)));
			InOutAllowed.Add(TexelKey(FIntPoint(A.X, B.Y)));
		}
	}

	bool SegmentStaysInCorridor(const FIntPoint& Start, const FIntPoint& End, const TSet<uint64>& Allowed)
	{
		int32 X = Start.X;
		int32 Y = Start.Y;
		const int32 DeltaX = FMath::Abs(End.X - Start.X);
		const int32 DeltaY = FMath::Abs(End.Y - Start.Y);
		const int32 StepX = FMath::Sign(End.X - Start.X);
		const int32 StepY = FMath::Sign(End.Y - Start.Y);
		int32 ProgressX = 0;
		int32 ProgressY = 0;
		if (!Allowed.Contains(TexelKey(FIntPoint(X, Y))))
		{
			return false;
		}

		while (ProgressX < DeltaX || ProgressY < DeltaY)
		{
			const int64 XDecision = static_cast<int64>(1 + 2 * ProgressX) * static_cast<int64>(DeltaY);
			const int64 YDecision = static_cast<int64>(1 + 2 * ProgressY) * static_cast<int64>(DeltaX);
			if (XDecision == YDecision)
			{
				if (!Allowed.Contains(TexelKey(FIntPoint(X + StepX, Y)))
					|| !Allowed.Contains(TexelKey(FIntPoint(X, Y + StepY))))
				{
					return false;
				}
				X += StepX;
				Y += StepY;
				++ProgressX;
				++ProgressY;
			}
			else if (XDecision < YDecision)
			{
				X += StepX;
				++ProgressX;
			}
			else
			{
				Y += StepY;
				++ProgressY;
			}
			if (!Allowed.Contains(TexelKey(FIntPoint(X, Y))))
			{
				return false;
			}
		}
		return true;
	}

	double PointToSegmentDistance(
		const FVector2D& Point,
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd)
	{
		const FVector2D Segment = SegmentEnd - SegmentStart;
		const double LengthSquared = Segment.SquaredLength();
		if (LengthSquared <= 0.0)
		{
			return FVector2D::Distance(Point, SegmentStart);
		}
		const double Projection = FMath::Clamp(FVector2D::DotProduct(Point - SegmentStart, Segment) / LengthSquared, 0.0, 1.0);
		return FVector2D::Distance(Point, SegmentStart + Segment * Projection);
	}

	bool SimplifyOrderedRange(
		const FWorkingPath& Source,
		TConstArrayView<int32> OrderedIndices,
		const double ToleranceCm,
		TBitArray<>& InOutRetained,
		int64& InOutIterations,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		if (OrderedIndices.Num() < 2)
		{
			return true;
		}

		TArray<FIntPoint> PendingRanges;
		PendingRanges.Add(FIntPoint(0, OrderedIndices.Num() - 1));
		while (PendingRanges.Num() > 0)
		{
			++InOutIterations;
			if ((InOutIterations & 4095) == 0 && !ContinueOrCancel(ShouldContinue, 0.94f, OutError))
			{
				return false;
			}

			const FIntPoint Range = PendingRanges.Pop(EAllowShrinking::No);
			if (Range.Y - Range.X <= 1)
			{
				continue;
			}

			const int32 StartSourceIndex = OrderedIndices[Range.X];
			const int32 EndSourceIndex = OrderedIndices[Range.Y];
			int32 FarthestOrderIndex = INDEX_NONE;
			double FarthestDistance = -1.0;
			for (int32 OrderIndex = Range.X + 1; OrderIndex < Range.Y; ++OrderIndex)
			{
				++InOutIterations;
				if ((InOutIterations & 4095) == 0 && !ContinueOrCancel(ShouldContinue, 0.94f, OutError))
				{
					return false;
				}
				const int32 SourceIndex = OrderedIndices[OrderIndex];
				const double Distance = PointToSegmentDistance(
					Source.Path.Points[SourceIndex],
					Source.Path.Points[StartSourceIndex],
					Source.Path.Points[EndSourceIndex]);
				if (Distance > FarthestDistance)
				{
					FarthestDistance = Distance;
					FarthestOrderIndex = OrderIndex;
				}
			}

			const bool bWithinTolerance = FarthestDistance <= ToleranceCm;
			bool bWithinCorridor = false;
			if (bWithinTolerance)
			{
				TSet<uint64> RangeCorridor;
				for (int32 OrderIndex = Range.X + 1; OrderIndex <= Range.Y; ++OrderIndex)
				{
					AddEdgeCorridor(
						Source.DenseTexels[OrderedIndices[OrderIndex - 1]],
						Source.DenseTexels[OrderedIndices[OrderIndex]],
						RangeCorridor);
				}
				bWithinCorridor = SegmentStaysInCorridor(
					Source.DenseTexels[StartSourceIndex], Source.DenseTexels[EndSourceIndex], RangeCorridor);
			}
			if (bWithinCorridor)
			{
				continue;
			}

			const int32 SplitOrderIndex = !bWithinTolerance && FarthestOrderIndex != INDEX_NONE
				? FarthestOrderIndex
				: (Range.X + Range.Y) / 2;
			InOutRetained[OrderedIndices[SplitOrderIndex]] = true;
			PendingRanges.Add(FIntPoint(Range.X, SplitOrderIndex));
			PendingRanges.Add(FIntPoint(SplitOrderIndex, Range.Y));
		}
		return true;
	}

	bool SimplifyPath(
		FWorkingPath& InOutPath,
		const double ToleranceCm,
		TFunctionRef<bool(float)> ShouldContinue,
		FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN SimplifyPath t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END SimplifyPath elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		const int32 PointCount = InOutPath.Path.Points.Num();
		if (ToleranceCm <= 0.0 || PointCount < 3)
		{
			return true;
		}

		TBitArray<> Retained(false, PointCount);
		TArray<int32> Anchors;
		Anchors.Add(0);
		for (const ArcWaterRiverNetworkExtraction::FArcWaterRiverJunctionRef& Ref : InOutPath.Path.Junctions)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			Anchors.AddUnique(Ref.PointIndex);
		}
		if (!InOutPath.Path.bClosed)
		{
			Anchors.AddUnique(PointCount - 1);
		}
		else if (Anchors.Num() == 1)
		{
			int32 FarthestIndex = 1;
			double FarthestDistanceSquared = -1.0;
			for (int32 Index = 1; Index < PointCount; ++Index)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				const double DistanceSquared = FVector2D::DistSquared(InOutPath.Path.Points[0], InOutPath.Path.Points[Index]);
				if (DistanceSquared > FarthestDistanceSquared)
				{
					FarthestDistanceSquared = DistanceSquared;
					FarthestIndex = Index;
				}
			}
			Anchors.Add(FarthestIndex);
		}

		Anchors.Sort();
		for (const int32 Anchor : Anchors)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			Retained[Anchor] = true;
		}

		int64 SimplificationIterations = 0;
		const int32 RangeCount = InOutPath.Path.bClosed ? Anchors.Num() : Anchors.Num() - 1;
		for (int32 RangeIndex = 0; RangeIndex < RangeCount; ++RangeIndex)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			const int32 StartIndex = Anchors[RangeIndex];
			const int32 EndIndex = InOutPath.Path.bClosed
				? Anchors[(RangeIndex + 1) % Anchors.Num()]
				: Anchors[RangeIndex + 1];
			TArray<int32> OrderedIndices;
			int32 CurrentIndex = StartIndex;
			OrderedIndices.Add(CurrentIndex);
			while (CurrentIndex != EndIndex)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				CurrentIndex = (CurrentIndex + 1) % PointCount;
				OrderedIndices.Add(CurrentIndex);
			}
			if (!SimplifyOrderedRange(InOutPath, OrderedIndices, ToleranceCm,
				Retained, SimplificationIterations, ShouldContinue, OutError))
			{
				return false;
			}
		}

		TArray<int32> NewIndexByOld;
		NewIndexByOld.Init(INDEX_NONE, PointCount);
		TArray<FVector2D> SimplifiedPoints;
		SimplifiedPoints.Reserve(PointCount);
		for (int32 Index = 0; Index < PointCount; ++Index)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if (Retained[Index])
			{
				NewIndexByOld[Index] = SimplifiedPoints.Add(InOutPath.Path.Points[Index]);
			}
		}
		for (ArcWaterRiverNetworkExtraction::FArcWaterRiverJunctionRef& Ref : InOutPath.Path.Junctions)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			Ref.PointIndex = NewIndexByOld[Ref.PointIndex];
		}
		InOutPath.Path.Points = MoveTemp(SimplifiedPoints);
		return true;
	}
}

namespace ArcWaterRiverNetworkExtraction
{
	bool FArcWaterRiverMaskGrid::AddActivePixel(const FIntPoint Pixel)
	{
		if (Pixel.X < SourceRect.Min.X || Pixel.X >= SourceRect.Max.X
			|| Pixel.Y < SourceRect.Min.Y || Pixel.Y >= SourceRect.Max.Y
			|| ActivePixelCount == MAX_int64)
		{
			return false;
		}
		const FIntPoint TargetTileCoord = ArcWaterRiverNetworkExtractionPrivate::TileCoordinate(Pixel);
		FArcWaterRiverMaskTile* Tile = Tiles.Find(TargetTileCoord);
		if (Tile == nullptr)
		{
			FArcWaterRiverMaskTile NewTile;
			NewTile.TileCoord = TargetTileCoord;
			NewTile.Pixels.Init(false, TileSide * TileSide);
			Tile = &Tiles.Add(TargetTileCoord, MoveTemp(NewTile));
		}
		const int32 PixelIndex = ArcWaterRiverNetworkExtractionPrivate::TilePixelIndex(Pixel, TargetTileCoord);
		if (Tile->Pixels[PixelIndex])
		{
			return true;
		}
		Tile->Pixels[PixelIndex] = true;
		++Tile->ActivePixelCount;
		++ActivePixelCount;
		return true;
	}

	bool FArcWaterRiverMaskGrid::IsActive(const FIntPoint Pixel) const
	{
		if (Pixel.X < SourceRect.Min.X || Pixel.X >= SourceRect.Max.X
			|| Pixel.Y < SourceRect.Min.Y || Pixel.Y >= SourceRect.Max.Y)
		{
			return false;
		}
		const FIntPoint TargetTileCoord = ArcWaterRiverNetworkExtractionPrivate::TileCoordinate(Pixel);
		const FArcWaterRiverMaskTile* Tile = Tiles.Find(TargetTileCoord);
		return Tile != nullptr && Tile->Pixels[ArcWaterRiverNetworkExtractionPrivate::TilePixelIndex(Pixel, TargetTileCoord)];
	}

	bool FArcWaterRiverMaskGrid::RemoveActivePixel(const FIntPoint Pixel)
	{
		const FIntPoint TargetTileCoord = ArcWaterRiverNetworkExtractionPrivate::TileCoordinate(Pixel);
		FArcWaterRiverMaskTile* Tile = Tiles.Find(TargetTileCoord);
		if (Tile == nullptr)
		{
			return false;
		}
		const int32 PixelIndex = ArcWaterRiverNetworkExtractionPrivate::TilePixelIndex(Pixel, TargetTileCoord);
		if (!Tile->Pixels[PixelIndex])
		{
			return false;
		}
		Tile->Pixels[PixelIndex] = false;
		--Tile->ActivePixelCount;
		--ActivePixelCount;
		if (Tile->ActivePixelCount == 0)
		{
			Tiles.Remove(TargetTileCoord);
		}
		return true;
	}

	int64 FArcWaterRiverMaskGrid::GetAllocatedBytes() const
	{
		int64 Bytes = Tiles.GetAllocatedSize();
		for (const TPair<FIntPoint, FArcWaterRiverMaskTile>& Pair : Tiles)
		{
			Bytes += Pair.Value.Pixels.GetAllocatedSize();
		}
		return Bytes;
	}

	bool ExtractNetwork(
		FArcWaterRiverMaskGrid&& Mask,
		const FVector2D& CmPerTexel,
		double SplineToleranceCm,
		TArray<FArcWaterRiverPath>& OutPaths,
		TArray<FIntPoint>& OutJunctionTexels,
		FText& OutError,
		TFunctionRef<bool(float)> ShouldContinue)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN ExtractNetwork t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END ExtractNetwork elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		using namespace ArcWaterRiverNetworkExtractionPrivate;

		OutPaths.Reset();
		OutJunctionTexels.Reset();
		OutError = FText::GetEmpty();

		const int64 SourceWidth = static_cast<int64>(Mask.SourceRect.Max.X) - Mask.SourceRect.Min.X;
		const int64 SourceHeight = static_cast<int64>(Mask.SourceRect.Max.Y) - Mask.SourceRect.Min.Y;
		if (SourceWidth < 2 || SourceHeight < 2)
		{
			OutError = LOCTEXT("InvalidSourceRect", "River extraction needs a valid source rectangle with at least two texels on each axis.");
			return false;
		}
		if (Mask.ActivePixelCount < 0 || Mask.ActivePixelCount > MAX_int32)
		{
			OutError = LOCTEXT("InvalidActivePixelCount", "The sparse Rivers mask exceeds the supported active-pixel count.");
			return false;
		}
		if (!FMath::IsFinite(CmPerTexel.X) || !FMath::IsFinite(CmPerTexel.Y)
			|| CmPerTexel.X <= 0.0 || CmPerTexel.Y <= 0.0)
		{
			OutError = LOCTEXT("InvalidTexelScale", "CmPerTexel must contain finite positive X and Y scales.");
			return false;
		}
		if (!FMath::IsFinite(SplineToleranceCm) || SplineToleranceCm < 0.0)
		{
			OutError = LOCTEXT("InvalidSplineTolerance", "SplineToleranceCm must be finite and non-negative.");
			return false;
		}
		if (!ContinueOrCancel(ShouldContinue, 0.0f, OutError))
		{
			return false;
		}

		int64 CountedActivePixels = 0;
		for (const TPair<FIntPoint, FArcWaterRiverMaskTile>& Pair : Mask.Tiles)
		{
			const FArcWaterRiverMaskTile& Tile = Pair.Value;
			if (Pair.Key != Tile.TileCoord || Tile.Pixels.Num() != FArcWaterRiverMaskGrid::TileSide * FArcWaterRiverMaskGrid::TileSide
				|| Tile.ActivePixelCount <= 0)
			{
				OutError = LOCTEXT("InvalidSparseMaskTile", "The sparse Rivers mask contains an invalid or empty occupied tile.");
				return false;
			}
			int32 CountedTilePixels = 0;
			for (TConstSetBitIterator<> Iterator(Tile.Pixels); Iterator; ++Iterator)
			{
				++CountedTilePixels;
			}
			if (CountedTilePixels != Tile.ActivePixelCount)
			{
				OutError = LOCTEXT("InvalidSparseMaskTileCount", "A sparse Rivers mask tile has an inconsistent active-pixel count.");
				return false;
			}
			CountedActivePixels += CountedTilePixels;
		}
		if (CountedActivePixels != Mask.ActivePixelCount)
		{
			OutError = LOCTEXT("InvalidSparseMaskCount", "The sparse Rivers mask active-pixel count does not match its occupied tiles.");
			return false;
		}

		TArray<FIntPoint> InitialPixels;
		CollectActivePixels(Mask, InitialPixels, false);
		for (const FIntPoint Pixel : InitialPixels)
		{
			if (Pixel.X < Mask.SourceRect.Min.X || Pixel.X >= Mask.SourceRect.Max.X
				|| Pixel.Y < Mask.SourceRect.Min.Y || Pixel.Y >= Mask.SourceRect.Max.Y)
			{
				OutError = LOCTEXT("SparseMaskOutsideSource", "The sparse Rivers mask contains an active pixel outside its source rectangle.");
				return false;
			}
		}
		InitialPixels.Empty();

		UE_LOGF(LogTemp, Display,"[RiverGen] MASK rect=%ls..%ls tiles=%d active=%lld bytes=%lld",
			*Mask.SourceRect.Min.ToString(), *Mask.SourceRect.Max.ToString(), Mask.Tiles.Num(),
			Mask.ActivePixelCount, Mask.GetAllocatedBytes());
		if (Mask.ActivePixelCount == 0)
		{
			return ContinueOrCancel(ShouldContinue, 1.0f, OutError);
		}
		TSet<FIntPoint> ProtectedEndpointCoordinates;
		if (!BuildClippedEndpointAnchors(Mask, ProtectedEndpointCoordinates,
			ShouldContinue, OutError))
		{
			return false;
		}

		const int64 MaxIterations = Mask.ActivePixelCount + 1;
		TArray<FIntPoint> ScratchMarked;
		int64 Iteration = 0;
		int64 ThinningVisits = 0;
		const double ThinningStart = FPlatformTime::Seconds();
		int32 RemovedLastIteration = 1;
		while (RemovedLastIteration > 0)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if (Iteration >= MaxIterations)
			{
				OutError = FText::Format(
					LOCTEXT("ThinningDidNotConverge", "Thinning the sparse Rivers mask did not converge within {0} iterations."),
					FText::AsNumber(MaxIterations));
				return false;
			}
			const float ThinningProgress = 0.08f + 0.34f * static_cast<float>(Iteration) / static_cast<float>(MaxIterations);
			int32 RemovedFirst = 0;
			int32 RemovedSecond = 0;
			if (!ThinSubIteration(Mask, ProtectedEndpointCoordinates, 0,
				ScratchMarked, RemovedFirst, ThinningVisits,
				ShouldContinue, ThinningProgress, OutError)
				|| !ThinSubIteration(Mask, ProtectedEndpointCoordinates, 1,
					ScratchMarked, RemovedSecond, ThinningVisits,
					ShouldContinue, ThinningProgress, OutError))
			{
				return false;
			}
			RemovedLastIteration = RemovedFirst + RemovedSecond;
			UE_LOGF(LogTemp, Display,"[RiverGen] THIN iteration=%lld removedFirst=%d removedSecond=%d active=%lld elapsed=%.3fs",
				Iteration, RemovedFirst, RemovedSecond, Mask.ActivePixelCount, FPlatformTime::Seconds() - ThinningStart);
			++Iteration;
		}
		UE_LOGF(LogTemp, Display,"[RiverGen] THIN COMPLETE iterations=%lld visits=%lld active=%lld tiles=%d bytes=%lld elapsed=%.3fs",
			Iteration, ThinningVisits, Mask.ActivePixelCount, Mask.Tiles.Num(), Mask.GetAllocatedBytes(),
			FPlatformTime::Seconds() - ThinningStart);

		TArray<FIntPoint> Pixels;
		CollectActivePixels(Mask, Pixels, true);
		TMap<FIntPoint, int32> PixelIdByCoordinate;
		PixelIdByCoordinate.Reserve(Pixels.Num());
		TArray<uint8> ProtectedEndpoints;
		ProtectedEndpoints.SetNumUninitialized(Pixels.Num());
		for (int32 Index = 0; Index < Pixels.Num(); ++Index)
		{
			PixelIdByCoordinate.Add(Pixels[Index], Index);
			ProtectedEndpoints[Index] = ProtectedEndpointCoordinates.Contains(Pixels[Index]) ? 1 : 0;
		}
		ProtectedEndpointCoordinates.Empty();
		const int64 CompactBytes = Pixels.GetAllocatedSize() + PixelIdByCoordinate.GetAllocatedSize()
			+ ProtectedEndpoints.GetAllocatedSize();
		UE_LOGF(LogTemp, Display,"[RiverGen] COMPACT pixels=%d bytes=%lld elapsed=%.3fs",
			Pixels.Num(), CompactBytes, FPlatformTime::Seconds() - RiverLogStart);

		TArray<uint8> Degrees;
		Degrees.SetNumUninitialized(Pixels.Num());
		for (int32 Index = 0; Index < Pixels.Num(); ++Index)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if ((Index % CancellationStride) == 0 && !ContinueOrCancel(ShouldContinue, 0.47f, OutError))
			{
				return false;
			}
			uint8 Neighbors[8];
			Degrees[Index] = static_cast<uint8>(FMath::CountBits(
				static_cast<uint32>(GetNeighborMask(Pixels, PixelIdByCoordinate, Index, Neighbors))));
		}

		TArray<uint8> GraphNeighborMasks;
		if (!BuildGraphNeighborMasks(Pixels, PixelIdByCoordinate, Degrees, ProtectedEndpoints,
			GraphNeighborMasks, ShouldContinue, OutError))
		{
			return false;
		}

		TArray<FIntPoint> LocalJunctionTexels;
		TArray<FGraphNode> Nodes;
		TMap<int32, int32> NodeByPixel;
		TArray<int32> IsolatedPixels;
		if (!BuildGraphNodes(Pixels, Degrees, GraphNeighborMasks, ProtectedEndpoints, Nodes,
			NodeByPixel, IsolatedPixels, LocalJunctionTexels,
			ShouldContinue, OutError))
		{
			return false;
		}
		ProtectedEndpoints.Empty();

		TArray<FGraphEdge> Edges;
		TArray<TArray<int32>> Adjacency;
		if (!BuildGraphEdges(Pixels, PixelIdByCoordinate, GraphNeighborMasks, CmPerTexel, Nodes, NodeByPixel,
			Edges, Adjacency, ShouldContinue, OutError))
		{
			return false;
		}
		Mask = FArcWaterRiverMaskGrid();
		GraphNeighborMasks.Empty();
		int64 GraphBytes = Pixels.GetAllocatedSize() + PixelIdByCoordinate.GetAllocatedSize()
			+ Degrees.GetAllocatedSize() + Nodes.GetAllocatedSize() + Edges.GetAllocatedSize()
			+ Adjacency.GetAllocatedSize();
		for (const TArray<int32>& NodeEdges : Adjacency)
		{
			GraphBytes += NodeEdges.GetAllocatedSize();
		}
		for (const FGraphEdge& Edge : Edges)
		{
			GraphBytes += Edge.DenseTexels.GetAllocatedSize();
		}
		UE_LOGF(LogTemp, Display,"[RiverGen] GRAPH pixels=%d nodes=%d edges=%d junctions=%d isolated=%d bytes=%lld elapsed=%.3fs",
			Pixels.Num(), Nodes.Num(), Edges.Num(), LocalJunctionTexels.Num(), IsolatedPixels.Num(),
			GraphBytes, FPlatformTime::Seconds() - RiverLogStart);
		Degrees.Empty();
		NodeByPixel.Empty();
		PixelIdByCoordinate.Empty();
		Pixels.Empty();

		TArray<FPathCandidate> Candidates;
		if (!DecomposeGraphPaths(Adjacency, Edges, Candidates, ShouldContinue, OutError))
		{
			return false;
		}

		TArray<FArcWaterRiverPath> LocalPaths;
		LocalPaths.Reserve(Candidates.Num());
		for (const FPathCandidate& Candidate : Candidates)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			FWorkingPath WorkingPath;
			UE_LOGF(LogTemp, Display,"[RiverGen] PATH selected index=%d edges=%d nodes=%d length=%.3f closed=%d", LocalPaths.Num(), Candidate.EdgeIds.Num(), Candidate.NodeIds.Num(), Candidate.LengthCm, Candidate.bClosed ? 1 : 0);
			if (!BuildWorkingPath(Candidate, Nodes, Edges, CmPerTexel, WorkingPath, OutError))
			{
				return false;
			}
			if (!SimplifyPath(WorkingPath, SplineToleranceCm, ShouldContinue, OutError))
			{
				return false;
			}
			LocalPaths.Add(MoveTemp(WorkingPath.Path));
			UE_LOGF(LogTemp, Display,"[RiverGen] PATH stored index=%d points=%d junctions=%d", LocalPaths.Num() - 1, LocalPaths.Last().Points.Num(), LocalPaths.Last().Junctions.Num());
		}

		if (IsolatedPixels.Num() > 0)
		{
			UE_LOGF(LogTemp, Warning,"[RiverGen] Ignored %d isolated river skeleton pixel(s).", IsolatedPixels.Num());
		}

		if (!ContinueOrCancel(ShouldContinue, 1.0f, OutError))
		{
			return false;
		}
		OutPaths = MoveTemp(LocalPaths);
		OutJunctionTexels = MoveTemp(LocalJunctionTexels);
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
