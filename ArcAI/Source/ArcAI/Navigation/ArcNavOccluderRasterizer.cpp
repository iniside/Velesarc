// Copyright Lukasz Baran. All Rights Reserved.

#include "Navigation/ArcNavOccluderRasterizer.h"

#include "ArcMass/Spatial/ArcMassInfluenceMapping.h"
#include "Detour/DetourNavMesh.h"
#include "Engine/World.h"
#include "Mass/ExecutionContext.h"
#include "MassLogging.h"
#include "NavMesh/RecastHelpers.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcNavOccluderRasterizer)

namespace ArcNavOccluderPrivate
{
	static TAutoConsoleVariable<int32> CVarOccluderTilesPerTick(
		TEXT("arc.ai.NavOccluderTilesPerTick"),
		4,
		TEXT("Maximum number of influence tiles whose occluder bitplane is rasterized from the navmesh per tick."));

	/** ARecastNavMesh::GetPolysInBox returns at most 256 polys and reports no truncation (RecastNavMesh.cpp:2866). */
	constexpr int32 PolysInBoxCap = 256;
	constexpr int32 MaxSubdivisionDepth = 3;

	static void GatherPolys(const ARecastNavMesh& NavMeshActor, const FBox& Box, int32 Depth,
		bool& bLoggedTruncation, TSet<NavNodeRef>& OutPolyRefs)
	{
		TArray<FNavPoly> Polys;
		if (!NavMeshActor.GetPolysInBox(Box, Polys))
		{
			return;
		}

		if (Polys.Num() >= PolysInBoxCap && Depth >= MaxSubdivisionDepth)
		{
			// Subdivision bottomed out with the query still saturated, so polys were dropped and this
			// tile's occluder bitmap is incomplete.
			UE_CLOG(!bLoggedTruncation, LogMass, Warning,
				TEXT("ArcNavOccluderRasterizer: GetPolysInBox still saturated at depth %d; occluder edges are being dropped."),
				Depth);
			bLoggedTruncation = true;
		}

		if (Polys.Num() >= PolysInBoxCap && Depth < MaxSubdivisionDepth)
		{
			const FVector Center = Box.GetCenter();
			for (int32 Quadrant = 0; Quadrant < 4; Quadrant++)
			{
				FBox SubBox = Box;
				if ((Quadrant & 1) != 0)
				{
					SubBox.Min.X = Center.X;
				}
				else
				{
					SubBox.Max.X = Center.X;
				}
				if ((Quadrant & 2) != 0)
				{
					SubBox.Min.Y = Center.Y;
				}
				else
				{
					SubBox.Max.Y = Center.Y;
				}
				GatherPolys(NavMeshActor, SubBox, Depth + 1, bLoggedTruncation, OutPolyRefs);
			}
			return;
		}

		for (const FNavPoly& Poly : Polys)
		{
			if (Poly.Ref != 0)
			{
				OutPolyRefs.Add(Poly.Ref);
			}
		}
	}

	/** Mirrors dtNavMesh::getNeighbourTilesAt's side-to-offset table (DetourNavMesh.cpp:2353). */
	static FIntPoint OffsetTileCoordBySide(int32 TileX, int32 TileY, int32 Side)
	{
		switch (Side)
		{
			case 0: return FIntPoint(TileX + 1, TileY);
			case 1: return FIntPoint(TileX + 1, TileY + 1);
			case 2: return FIntPoint(TileX, TileY + 1);
			case 3: return FIntPoint(TileX - 1, TileY + 1);
			case 4: return FIntPoint(TileX - 1, TileY);
			case 5: return FIntPoint(TileX - 1, TileY - 1);
			case 6: return FIntPoint(TileX, TileY - 1);
			case 7: return FIntPoint(TileX + 1, TileY - 1);
			default: return FIntPoint(TileX, TileY);
		}
	}

	/** One occluding sub-span of a polygon edge, in world space. */
	struct FOccludingSpan
	{
		FVector Start;
		FVector End;
	};

	/** Mirrors dtSegInterval (DetourNavMeshQuery.cpp:4349): a link's coverage of the edge in 1/255 units. */
	struct FEdgeInterval
	{
		int32 TMin;
		int32 TMax;
	};

	/** Sorted insert mirroring insertInterval (DetourNavMeshQuery.cpp:4355), without the 16-interval cap. */
	static void InsertInterval(TArray<FEdgeInterval, TInlineAllocator<20>>& Intervals, int32 TMin, int32 TMax)
	{
		int32 Index = 0;
		while (Index < Intervals.Num() && TMax > Intervals[Index].TMin)
		{
			Index++;
		}
		Intervals.Insert(FEdgeInterval{ TMin, TMax }, Index);
	}

	static bool IsNeighbourTilePresent(const ARecastNavMesh& NavMeshActor, const FIntPoint& NeighbourCoord,
		TMap<FIntPoint, bool>& PresenceCache)
	{
		if (const bool* Cached = PresenceCache.Find(NeighbourCoord))
		{
			return *Cached;
		}

		TArray<FNavTileRef> NeighbourRefs;
		NavMeshActor.GetNavMeshTilesAt(NeighbourCoord.X, NeighbourCoord.Y, NeighbourRefs);
		const bool bPresent = NeighbourRefs.Num() > 0;
		PresenceCache.Add(NeighbourCoord, bPresent);
		return bPresent;
	}

	/**
	 * Emits the occluding sub-spans of one polygon edge, mirroring the interval machinery in
	 * dtNavMeshQuery::getPolyWallSegments (DetourNavMeshQuery.cpp:4408-4525): live links cover
	 * [bmin, bmax] of a tile-border edge and every uncovered gap between them is a wall. A border edge
	 * is routinely only partially portalled, so whole-edge classification would erase real geometry.
	 */
	static void GatherOccludingSpans(const dtNavMesh& NavMesh, const ARecastNavMesh& NavMeshActor,
		const dtMeshTile& MeshTile, const dtPoly& Poly, int32 EdgeIndex, int32 NextVertIndex,
		TMap<FIntPoint, bool>& PresenceCache, TArray<FOccludingSpan>& OutSpans)
	{
		FVector::FReal StartRecast[3];
		FVector::FReal EndRecast[3];
		MeshTile.verts[Poly.verts[EdgeIndex]].toWorld(MeshTile.header->bmin, StartRecast);
		MeshTile.verts[Poly.verts[NextVertIndex]].toWorld(MeshTile.header->bmin, EndRecast);
		// Recast2UnrealPoint is affine, so lerping after conversion equals the engine's dtVlerp before it.
		const FVector VJ = Recast2UnrealPoint(StartRecast);
		const FVector VI = Recast2UnrealPoint(EndRecast);

		const unsigned short Nei = Poly.neis[EdgeIndex];
		if ((Nei & DT_EXT_LINK) == 0)
		{
			if (Nei == 0)
			{
				OutSpans.Add(FOccludingSpan{ VJ, VI });
			}
			return;
		}

		TArray<FEdgeInterval, TInlineAllocator<20>> Intervals;
		for (unsigned int LinkIndex = Poly.firstLink; LinkIndex != DT_NULL_LINK; )
		{
			const dtLink& Link = NavMesh.getLink(&MeshTile, LinkIndex);
			LinkIndex = Link.next;
			if (Link.edge == EdgeIndex && Link.ref != 0)
			{
				InsertInterval(Intervals, int32(Link.bmin), int32(Link.bmax));
			}
		}

		// Sentinels, exactly as the engine adds them before walking the gaps.
		InsertInterval(Intervals, -1, 0);
		InsertInterval(Intervals, 255, 256);

		for (int32 Index = 1; Index < Intervals.Num(); Index++)
		{
			const int32 GapMin = Intervals[Index - 1].TMax;
			const int32 GapMax = Intervals[Index].TMin;
			if (GapMin == GapMax)
			{
				continue;
			}

			// A tile border with no live link over this span is a wall only where a neighbouring navmesh
			// tile exists. With bGenerateNavigationOnlyAroundNavigationInvokers the navmesh simply ends,
			// and that reads as transparent.
			const FIntPoint NeighbourCoord = OffsetTileCoordBySide(MeshTile.header->x, MeshTile.header->y, Nei & 0xff);
			if (!IsNeighbourTilePresent(NavMeshActor, NeighbourCoord, PresenceCache))
			{
				return;
			}

			const double TMin = double(GapMin) / 255.0;
			const double TMax = double(GapMax) / 255.0;
			OutSpans.Add(FOccludingSpan{ FMath::Lerp(VJ, VI, TMin), FMath::Lerp(VJ, VI, TMax) });
		}
	}

	/** Clips [InOutA, InOutB] to the [0, TileDim] square in tile-local cell coordinates. */
	static bool ClipToTile(FVector2D& InOutA, FVector2D& InOutB, double TileDim)
	{
		const FVector2D Delta = InOutB - InOutA;
		double TMin = 0.0;
		double TMax = 1.0;

		const double Point[2] = { InOutA.X, InOutA.Y };
		const double Direction[2] = { Delta.X, Delta.Y };

		for (int32 Axis = 0; Axis < 2; Axis++)
		{
			if (FMath::IsNearlyZero(Direction[Axis]))
			{
				if (Point[Axis] < 0.0 || Point[Axis] > TileDim)
				{
					return false;
				}
				continue;
			}

			double TNear = (0.0 - Point[Axis]) / Direction[Axis];
			double TFar = (TileDim - Point[Axis]) / Direction[Axis];
			if (TNear > TFar)
			{
				Swap(TNear, TFar);
			}
			TMin = FMath::Max(TMin, TNear);
			TMax = FMath::Min(TMax, TFar);
			if (TMin > TMax)
			{
				return false;
			}
		}

		const FVector2D ClippedA = InOutA + Delta * TMin;
		const FVector2D ClippedB = InOutA + Delta * TMax;
		InOutA = ClippedA;
		InOutB = ClippedB;
		return true;
	}

	/** Amanatides-Woo grid traversal, so the marked run is 4-connected and a shadowcast cannot slip through diagonally. */
	static void RasterizeSegment(FArcInfluenceTile& Tile, int32 TileDim, double CellSize,
		const FIntPoint& TileCoord, const FVector& Start, const FVector& End)
	{
		const double InvCellSize = 1.0 / CellSize;
		const double OriginX = double(TileCoord.X) * double(TileDim);
		const double OriginY = double(TileCoord.Y) * double(TileDim);

		FVector2D A(Start.X * InvCellSize - OriginX, Start.Y * InvCellSize - OriginY);
		FVector2D B(End.X * InvCellSize - OriginX, End.Y * InvCellSize - OriginY);
		if (!ClipToTile(A, B, double(TileDim)))
		{
			return;
		}

		int32 X = FMath::Clamp(FMath::FloorToInt32(A.X), 0, TileDim - 1);
		int32 Y = FMath::Clamp(FMath::FloorToInt32(A.Y), 0, TileDim - 1);
		const int32 EndX = FMath::Clamp(FMath::FloorToInt32(B.X), 0, TileDim - 1);
		const int32 EndY = FMath::Clamp(FMath::FloorToInt32(B.Y), 0, TileDim - 1);

		const double DeltaX = B.X - A.X;
		const double DeltaY = B.Y - A.Y;
		const int32 StepX = DeltaX > 0.0 ? 1 : (DeltaX < 0.0 ? -1 : 0);
		const int32 StepY = DeltaY > 0.0 ? 1 : (DeltaY < 0.0 ? -1 : 0);

		const double Huge = TNumericLimits<double>::Max();
		const double TDeltaX = StepX != 0 ? FMath::Abs(1.0 / DeltaX) : Huge;
		const double TDeltaY = StepY != 0 ? FMath::Abs(1.0 / DeltaY) : Huge;
		double TMaxX = StepX != 0 ? ((StepX > 0 ? double(X) + 1.0 : double(X)) - A.X) / DeltaX : Huge;
		double TMaxY = StepY != 0 ? ((StepY > 0 ? double(Y) + 1.0 : double(Y)) - A.Y) / DeltaY : Huge;
		TMaxX = FMath::Max(TMaxX, 0.0);
		TMaxY = FMath::Max(TMaxY, 0.0);

		const int32 MaxSteps = 2 * TileDim + 2;
		for (int32 Step = 0; Step < MaxSteps; Step++)
		{
			Tile.Occluders.SetBit(Y * TileDim + X);

			if (X == EndX && Y == EndY)
			{
				return;
			}

			if (TMaxX < TMaxY)
			{
				X += StepX;
				TMaxX += TDeltaX;
			}
			else
			{
				Y += StepY;
				TMaxY += TDeltaY;
			}

			if (X < 0 || X >= TileDim || Y < 0 || Y >= TileDim)
			{
				return;
			}
		}
	}

	static void RasterizeTile(const ARecastNavMesh& NavMeshActor, const dtNavMesh& NavMesh,
		const FArcInfluenceTiledGrid& Grid, const FIntPoint& TileCoord, FArcInfluenceTile& Tile,
		bool& bLoggedTruncation)
	{
		Tile.Occluders.ClearBits();

		const double TileWorldSize = Grid.GetTileWorldSize();
		const double MinX = double(TileCoord.X) * TileWorldSize;
		const double MinY = double(TileCoord.Y) * TileWorldSize;
		const FBox TileBox(
			FVector(MinX, MinY, -HALF_WORLD_MAX),
			FVector(MinX + TileWorldSize, MinY + TileWorldSize, HALF_WORLD_MAX));

		TSet<NavNodeRef> PolyRefs;
		GatherPolys(NavMeshActor, TileBox, 0, bLoggedTruncation, PolyRefs);

		const int32 TileDim = Grid.Settings.TileDim;
		const double CellSize = Grid.Settings.CellSize;

		// One presence answer per neighbouring nav tile, because GetNavMeshTilesAt allocates on every call.
		TMap<FIntPoint, bool> PresenceCache;
		TArray<FOccludingSpan> Spans;

		for (const NavNodeRef PolyRef : PolyRefs)
		{
			const dtMeshTile* MeshTile = nullptr;
			const dtPoly* Poly = nullptr;
			if (dtStatusFailed(NavMesh.getTileAndPolyByRef(PolyRef, &MeshTile, &Poly))
				|| MeshTile->header == nullptr
				|| Poly->getType() != DT_POLYTYPE_GROUND)
			{
				continue;
			}

			const int32 VertCount = int32(Poly->vertCount);
			for (int32 I = 0, J = VertCount - 1; I < VertCount; J = I++)
			{
				Spans.Reset();
				GatherOccludingSpans(NavMesh, NavMeshActor, *MeshTile, *Poly, J, I, PresenceCache, Spans);

				for (const FOccludingSpan& Span : Spans)
				{
					RasterizeSegment(Tile, TileDim, CellSize, TileCoord, Span.Start, Span.End);
				}
			}
		}
	}
} // namespace ArcNavOccluderPrivate

UArcNavOccluderRasterizerProcessor::UArcNavOccluderRasterizerProcessor()
{
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;

	ExecutionOrder.ExecuteAfter.Add(FName(TEXT("ArcInfluenceInvokerProcessor")));
}

void UArcNavOccluderRasterizerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	UArcInfluenceMappingSubsystem* Subsystem = World ? World->GetSubsystem<UArcInfluenceMappingSubsystem>() : nullptr;
	if (Subsystem == nullptr || Subsystem->GetGridCount() == 0)
	{
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	ARecastNavMesh* NavMeshActor = NavSys ? Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance()) : nullptr;
	const dtNavMesh* NavMesh = NavMeshActor ? NavMeshActor->GetRecastMesh() : nullptr;
	if (NavMesh == nullptr)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcNavOccluderRasterize);

	// The engine counter is monotonic from 0, so this inequality also catches the very first tick.
	const uint64 UpdateCount = NavMeshActor->GetTotalUpdatedTilesCount();
	const bool bNavMeshChanged = UpdateCount != LastNavMeshUpdateCount;
	LastNavMeshUpdateCount = UpdateCount;

	PendingDrains.SetNum(Subsystem->GetGridCount(), EAllowShrinking::No);

	int32 BudgetRemaining = FMath::Max(ArcNavOccluderPrivate::CVarOccluderTilesPerTick.GetValueOnGameThread(), 1);

	// Refill every grid before draining any of them. Folding this into the budget-gated loop
	// would let a saturated early grid starve a later grid's refill: bNavMeshChanged is false
	// on the following ticks, so that grid would keep a pre-churn occluder plane indefinitely.
	for (int32 GridIndex = 0; GridIndex < Subsystem->GetGridCount(); GridIndex++)
	{
		FArcInfluenceTiledGrid* Grid = Subsystem->GetTiledGrid(GridIndex);
		if (Grid == nullptr)
		{
			continue;
		}

		TArray<FIntPoint>& Pending = PendingDrains[GridIndex];

		if (bNavMeshChanged)
		{
			// OnNavMeshTilesUpdated is a virtual with nothing to bind to, so navmesh churn invalidates wholesale.
			Grid->Tiles.GenerateKeyArray(Pending);
			Grid->OccluderDirtyTiles.Reset();
		}
		else if (Pending.Num() == 0)
		{
			// Owning the drain list is what makes this safe: ReleaseTile swap-removes from
			// OccluderDirtyTiles, which would move an undrained tile past any positional cursor into it.
			Pending.Append(Grid->OccluderDirtyTiles);
			Grid->OccluderDirtyTiles.Reset();
		}
	}

	for (int32 GridIndex = 0; GridIndex < Subsystem->GetGridCount() && BudgetRemaining > 0; GridIndex++)
	{
		FArcInfluenceTiledGrid* Grid = Subsystem->GetTiledGrid(GridIndex);
		if (Grid == nullptr)
		{
			continue;
		}

		TArray<FIntPoint>& Pending = PendingDrains[GridIndex];

		while (BudgetRemaining > 0 && Pending.Num() > 0)
		{
			const FIntPoint TileCoord = Pending.Pop(EAllowShrinking::No);
			FArcInfluenceTile* Tile = Grid->FindTile(TileCoord);
			if (Tile == nullptr)
			{
				// Released between dirtying and draining; costs no budget.
				continue;
			}

			BudgetRemaining--;
			ArcNavOccluderPrivate::RasterizeTile(*NavMeshActor, *NavMesh, *Grid, TileCoord, *Tile,
				bLoggedPolyGatherTruncation);
		}
	}
}
