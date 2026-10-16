// Copyright Lukasz Baran. All Rights Reserved.

#include "Perception/ArcVisibilityChannelProcessor.h"

#include "ArcMass/Spatial/ArcMassInfluenceMapping.h"
#include "Engine/World.h"
#include "Mass/EntityFragments.h"
#include "Mass/ExecutionContext.h"
#include "Perception/ArcMassSightPerception.h"
#include "Teams/ArcMassTeamFragment.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcVisibilityChannelProcessor)

namespace ArcVisibilityChannelPrivate
{
	static TAutoConsoleVariable<int32> CVarPerceiversPerTick(
		TEXT("arc.ai.VisibilityPerceiversPerTick"),
		8,
		TEXT("Maximum number of perceivers shadowcast into the tiled influence grids per tick. The whole perceiver set is one rebuild cycle."));

	/** Octant transforms: world offset = (Dx*XX + Dy*XY, Dx*YX + Dy*YY). Exactly one of XX/YX is non-zero per octant, so a row run is axis-aligned. */
	static const int32 OctantMult[4][8] =
	{
		{ 1,  0,  0, -1, -1,  0,  0,  1 },
		{ 0,  1,  1,  0,  0, -1, -1,  0 },
		{ 0,  1, -1,  0,  0, -1,  1,  0 },
		{ 1,  0,  0,  1, -1,  0,  0, -1 }
	};

	/** Sets bits [First, Last] of a plane, whole words at a time. */
	static void SetBitRange(FArcInfluenceBitplane& Plane, int32 First, int32 Last)
	{
		const int32 FirstWord = First >> 6;
		const int32 LastWord = Last >> 6;
		const uint64 HeadMask = ~uint64(0) << (First & 63);
		const uint64 TailMask = ~uint64(0) >> (63 - (Last & 63));

		if (FirstWord == LastWord)
		{
			Plane.OrWord(FirstWord, HeadMask & TailMask);
			return;
		}

		Plane.OrWord(FirstWord, HeadMask);
		for (int32 Word = FirstWord + 1; Word < LastWord; Word++)
		{
			Plane.OrWord(Word, ~uint64(0));
		}
		Plane.OrWord(LastWord, TailMask);
	}

	/** Remembers the last tile a cell resolved to, so a run of cells inside one tile costs a single map lookup. */
	struct FTileCursor
	{
		FArcInfluenceTiledGrid* Grid = nullptr;
		FArcInfluenceTile* Tile = nullptr;
		FIntPoint TileCoord = FIntPoint::ZeroValue;
		bool bResolved = false;

		FArcInfluenceTile* Resolve(const FIntPoint& InTileCoord)
		{
			if (!bResolved || InTileCoord != TileCoord)
			{
				TileCoord = InTileCoord;
				Tile = Grid->FindTile(InTileCoord);
				bResolved = true;
			}
			return Tile;
		}
	};

	/** Cells marked so far in the row being scanned, in octant-local coordinates. */
	struct FRowRun
	{
		int32 Dy = 0;
		int32 XX = 0;
		int32 XY = 0;
		int32 YX = 0;
		int32 YY = 0;
		int32 Dx0 = 0;
		int32 Dx1 = 0;
		bool bActive = false;
	};

	struct FShadowCaster
	{
		FShadowCaster(FArcInfluenceTiledGrid& InGrid, const FArcVisibilityCaster& InCaster)
			: Grid(InGrid)
			, Caster(InCaster)
			, TileDim(InGrid.Settings.TileDim)
			, CellSize(InGrid.Settings.CellSize)
			, RangeSq(InCaster.Range * InCaster.Range)
			, OriginCell(InGrid.WorldToCell(InCaster.Origin))
			, MaxDepth(FMath::CeilToInt32(InCaster.Range / InGrid.Settings.CellSize))
			, bFullCircle(InCaster.HalfAngleRadians >= PI - UE_KINDA_SMALL_NUMBER || InCaster.Forward.IsNearlyZero())
		{
			ReadCursor.Grid = &InGrid;
			WriteCursor.Grid = &InGrid;
		}

		void Run()
		{
			WriteHorizontalRun(OriginCell.X, OriginCell.X, OriginCell.Y);

			for (int32 Octant = 0; Octant < 8; Octant++)
			{
				if (OctantOverlapsShape(Octant))
				{
					CastLight(Octant, 1, 1.0, 0.0);
				}
			}
		}

	private:
		FArcInfluenceTiledGrid& Grid;
		const FArcVisibilityCaster& Caster;
		int32 TileDim;
		double CellSize;
		double RangeSq;
		FIntPoint OriginCell;
		int32 MaxDepth;
		bool bFullCircle;
		FTileCursor ReadCursor;
		FTileCursor WriteCursor;

		/** An octant spans a 45-degree wedge, so it can be skipped when its axis is further from the view direction than the two half-angles combined. */
		bool OctantOverlapsShape(int32 Octant) const
		{
			if (bFullCircle)
			{
				return true;
			}

			const double CombinedHalfAngle = Caster.HalfAngleRadians + PI / 8.0;
			if (CombinedHalfAngle >= PI)
			{
				return true;
			}

			const int32 XX = OctantMult[0][Octant];
			const int32 XY = OctantMult[1][Octant];
			const int32 YX = OctantMult[2][Octant];
			const int32 YY = OctantMult[3][Octant];

			const FVector2D WedgeNear(double(-XY), double(-YY));
			const FVector2D WedgeFar = FVector2D(double(-XX - XY), double(-YX - YY)).GetSafeNormal();
			const FVector2D WedgeAxis = (WedgeNear + WedgeFar).GetSafeNormal();

			return FVector2D::DotProduct(WedgeAxis, Caster.Forward) >= FMath::Cos(CombinedHalfAngle);
		}

		bool IsInShape(int32 CellX, int32 CellY) const
		{
			const double DeltaX = (double(CellX) + 0.5) * CellSize - Caster.Origin.X;
			const double DeltaY = (double(CellY) + 0.5) * CellSize - Caster.Origin.Y;
			const double DistSq = DeltaX * DeltaX + DeltaY * DeltaY;
			if (DistSq > RangeSq)
			{
				return false;
			}
			if (bFullCircle)
			{
				return true;
			}

			// Dot >= Cos * Length, compared squared to keep the square root out of the inner loop.
			const double Dot = DeltaX * Caster.Forward.X + DeltaY * Caster.Forward.Y;
			const double CosSqTerm = Caster.CosHalfAngle * Caster.CosHalfAngle * DistSq;
			if (Dot >= 0.0)
			{
				return Caster.CosHalfAngle <= 0.0 || Dot * Dot >= CosSqTerm;
			}
			return Caster.CosHalfAngle < 0.0 && Dot * Dot <= CosSqTerm;
		}

		bool IsOccluder(int32 CellX, int32 CellY)
		{
			const FIntPoint TileCoord = Grid.CellToTile(FIntPoint(CellX, CellY));
			const FArcInfluenceTile* Tile = ReadCursor.Resolve(TileCoord);
			if (Tile == nullptr)
			{
				// D8: an unallocated tile is transparent, so the cast crosses it instead of stopping at an invisible wall.
				return false;
			}

			const int32 LocalX = CellX - TileCoord.X * TileDim;
			const int32 LocalY = CellY - TileCoord.Y * TileDim;
			return Tile->Occluders.GetBit(LocalY * TileDim + LocalX);
		}

		void WriteHorizontalRun(int32 FromX, int32 ToX, int32 CellY)
		{
			int32 X = FromX;
			while (X <= ToX)
			{
				const FIntPoint TileCoord = Grid.CellToTile(FIntPoint(X, CellY));
				const int32 TileMinX = TileCoord.X * TileDim;
				const int32 SegmentEnd = FMath::Min(ToX, TileMinX + TileDim - 1);

				if (FArcInfluenceTile* Tile = WriteCursor.Resolve(TileCoord))
				{
					const int32 RowBase = (CellY - TileCoord.Y * TileDim) * TileDim;
					SetBitRange(Tile->FindOrAddBackPlane(Caster.TeamId),
						RowBase + (X - TileMinX), RowBase + (SegmentEnd - TileMinX));
				}

				X = SegmentEnd + 1;
			}
		}

		void WriteVerticalRun(int32 CellX, int32 FromY, int32 ToY)
		{
			int32 Y = FromY;
			while (Y <= ToY)
			{
				const FIntPoint TileCoord = Grid.CellToTile(FIntPoint(CellX, Y));
				const int32 TileMinY = TileCoord.Y * TileDim;
				const int32 SegmentEnd = FMath::Min(ToY, TileMinY + TileDim - 1);

				if (FArcInfluenceTile* Tile = WriteCursor.Resolve(TileCoord))
				{
					FArcInfluenceBitplane& Plane = Tile->FindOrAddBackPlane(Caster.TeamId);
					const int32 LocalX = CellX - TileCoord.X * TileDim;
					for (int32 LocalY = Y - TileMinY; LocalY <= SegmentEnd - TileMinY; LocalY++)
					{
						Plane.SetBit(LocalY * TileDim + LocalX);
					}
				}

				Y = SegmentEnd + 1;
			}
		}

		void FlushRun(FRowRun& Row)
		{
			if (!Row.bActive)
			{
				return;
			}
			Row.bActive = false;

			const int32 X0 = OriginCell.X + Row.Dx0 * Row.XX + Row.Dy * Row.XY;
			const int32 Y0 = OriginCell.Y + Row.Dx0 * Row.YX + Row.Dy * Row.YY;
			const int32 X1 = OriginCell.X + Row.Dx1 * Row.XX + Row.Dy * Row.XY;
			const int32 Y1 = OriginCell.Y + Row.Dx1 * Row.YX + Row.Dy * Row.YY;

			if (Y0 == Y1)
			{
				WriteHorizontalRun(FMath::Min(X0, X1), FMath::Max(X0, X1), Y0);
			}
			else
			{
				WriteVerticalRun(X0, FMath::Min(Y0, Y1), FMath::Max(Y0, Y1));
			}
		}

		void MarkCell(FRowRun& Row, int32 Dx, int32 CellX, int32 CellY)
		{
			if (!IsInShape(CellX, CellY))
			{
				FlushRun(Row);
				return;
			}

			if (Row.bActive && Dx == Row.Dx1 + 1)
			{
				Row.Dx1 = Dx;
				return;
			}

			FlushRun(Row);
			Row.bActive = true;
			Row.Dx0 = Dx;
			Row.Dx1 = Dx;
		}

		void CastLight(int32 Octant, int32 StartRow, double Start, double End)
		{
			if (Start < End)
			{
				return;
			}

			FRowRun Row;
			Row.XX = OctantMult[0][Octant];
			Row.XY = OctantMult[1][Octant];
			Row.YX = OctantMult[2][Octant];
			Row.YY = OctantMult[3][Octant];

			double NewStart = 0.0;

			for (int32 Depth = StartRow; Depth <= MaxDepth; Depth++)
			{
				const int32 Dy = -Depth;
				Row.Dy = Dy;
				Row.bActive = false;
				bool bBlocked = false;

				for (int32 Dx = -Depth; Dx <= 0; Dx++)
				{
					const double LeftSlope = (double(Dx) - 0.5) / (double(Dy) + 0.5);
					const double RightSlope = (double(Dx) + 0.5) / (double(Dy) - 0.5);

					if (Start < RightSlope)
					{
						continue;
					}
					if (End > LeftSlope)
					{
						break;
					}

					const int32 CellX = OriginCell.X + Dx * Row.XX + Dy * Row.XY;
					const int32 CellY = OriginCell.Y + Dx * Row.YX + Dy * Row.YY;
					const bool bWall = IsOccluder(CellX, CellY);

					if (bBlocked)
					{
						if (bWall)
						{
							// Every cell of a wall run shows its face, not just the one that started it.
							// Non-wall cells here sit behind the wall and stay unmarked until the
							// un-block re-test below admits them.
							MarkCell(Row, Dx, CellX, CellY);
							NewStart = RightSlope;
							continue;
						}

						bBlocked = false;
						Start = NewStart;
						if (Start < RightSlope)
						{
							continue;
						}
					}
					else if (bWall)
					{
						bBlocked = true;
						// The wall face itself is seen; what it hides is left to the narrower scan spawned here.
						MarkCell(Row, Dx, CellX, CellY);
						FlushRun(Row);
						if (Depth < MaxDepth)
						{
							CastLight(Octant, Depth + 1, Start, LeftSlope);
						}
						NewStart = RightSlope;
						continue;
					}

					MarkCell(Row, Dx, CellX, CellY);
				}

				FlushRun(Row);

				if (bBlocked)
				{
					break;
				}
			}
		}
	};

	static void CollectCasters(FMassExecutionContext& Context, FMassEntityQuery& Query, TArray<FArcVisibilityCaster>& OutCasters)
	{
		Query.ForEachEntityChunk(Context, [&OutCasters](FMassExecutionContext& Ctx)
		{
			const FArcPerceptionSightSenseConfigFragment& Config = Ctx.GetConstSharedFragment<FArcPerceptionSightSenseConfigFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FArcMassTeamFragment> Teams = Ctx.GetFragmentView<FArcMassTeamFragment>();

			const bool bIsCone = Config.ShapeType == EArcPerceptionShapeType::Cone;
			const double HalfAngleRadians = bIsCone ? FMath::DegreesToRadians(double(Config.ConeHalfAngleDegrees)) : PI;
			const double Range = bIsCone ? double(Config.ConeLength) : double(Config.Radius);
			if (Range <= 0.0)
			{
				return;
			}

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FTransform& Transform = Transforms[EntityIt].GetTransform();
				const FVector Forward = Transform.GetRotation().GetForwardVector();

				FArcVisibilityCaster& Caster = OutCasters.AddDefaulted_GetRef();
				Caster.Origin = Transform.GetLocation();
				// A perceiver looking straight up projects onto the grid as a full disc, which a zero Forward selects.
				Caster.Forward = FVector2D(Forward.X, Forward.Y).GetSafeNormal();
				Caster.Range = Range;
				Caster.HalfAngleRadians = HalfAngleRadians;
				Caster.CosHalfAngle = FMath::Cos(HalfAngleRadians);
				Caster.TeamId = Teams[EntityIt].TeamId;
			}
		});
	}
} // namespace ArcVisibilityChannelPrivate

UArcVisibilityChannelProcessor::UArcVisibilityChannelProcessor()
	: PerceiverQuery{*this}
{
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;

	ExecutionOrder.ExecuteAfter.Add(FName(TEXT("ArcNavOccluderRasterizerProcessor")));
	// Without this edge Mass may run an NPC's first path request of a new region against an empty channel.
	ExecutionOrder.ExecuteBefore.Add(FName(TEXT("ArcTacticsUtilityProcessor")));
}

void UArcVisibilityChannelProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	PerceiverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PerceiverQuery.AddRequirement<FArcMassTeamFragment>(EMassFragmentAccess::ReadOnly);
	PerceiverQuery.AddConstSharedRequirement<FArcPerceptionSightSenseConfigFragment>(EMassFragmentPresence::All);
}

void UArcVisibilityChannelProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	UArcInfluenceMappingSubsystem* Subsystem = World ? World->GetSubsystem<UArcInfluenceMappingSubsystem>() : nullptr;
	if (Subsystem == nullptr || Subsystem->GetGridCount() == 0)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisibilityChannelRebuild);

	const int32 GridCount = Subsystem->GetGridCount();

	if (!bCycleActive)
	{
		Casters.Reset();
		ArcVisibilityChannelPrivate::CollectCasters(Context, PerceiverQuery, Casters);
		CasterCursor = 0;
		bCycleActive = true;

		for (int32 GridIndex = 0; GridIndex < GridCount; GridIndex++)
		{
			FArcInfluenceTiledGrid* Grid = Subsystem->GetTiledGrid(GridIndex);
			if (Grid == nullptr)
			{
				continue;
			}
			for (TPair<FIntPoint, FArcInfluenceTile>& Pair : Grid->Tiles)
			{
				Pair.Value.ClearBack();
			}
		}
	}

	const int32 Budget = FMath::Max(ArcVisibilityChannelPrivate::CVarPerceiversPerTick.GetValueOnGameThread(), 1);
	const int32 BatchEnd = FMath::Min(CasterCursor + Budget, Casters.Num());

	for (; CasterCursor < BatchEnd; CasterCursor++)
	{
		for (int32 GridIndex = 0; GridIndex < GridCount; GridIndex++)
		{
			FArcInfluenceTiledGrid* Grid = Subsystem->GetTiledGrid(GridIndex);
			if (Grid == nullptr)
			{
				continue;
			}

			ArcVisibilityChannelPrivate::FShadowCaster ShadowCaster(*Grid, Casters[CasterCursor]);
			ShadowCaster.Run();
		}
	}

	if (CasterCursor < Casters.Num())
	{
		return;
	}

	// Nothing is published until every perceiver of this cycle has been cast, so a reader never sees half a rebuild.
	for (int32 GridIndex = 0; GridIndex < GridCount; GridIndex++)
	{
		FArcInfluenceTiledGrid* Grid = Subsystem->GetTiledGrid(GridIndex);
		if (Grid == nullptr)
		{
			continue;
		}
		for (TPair<FIntPoint, FArcInfluenceTile>& Pair : Grid->Tiles)
		{
			Pair.Value.PublishBack();
		}
	}

	Casters.Reset();
	bCycleActive = false;
}
