// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInfluenceInvoker.h"

#include "ArcMassInfluenceMapping.h"
#include "Engine/World.h"
#include "Mass/EntityFragments.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplateRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcInfluenceInvoker)

namespace ArcInfluenceInvokerPrivate
{
	static TAutoConsoleVariable<int32> CVarInvokerUpdatesPerTick(
		TEXT("arc.mass.InfluenceInvokerUpdatesPerTick"),
		16,
		TEXT("Maximum number of influence invokers whose tile footprint is recomputed per tick."));

	static bool IsTileWithin(const FArcInfluenceTiledGrid& Grid, const FVector& Position, float Radius, const FIntPoint& TileCoord)
	{
		const double TileSize = Grid.GetTileWorldSize();
		const double MinX = TileCoord.X * TileSize;
		const double MinY = TileCoord.Y * TileSize;
		const double DeltaX = Position.X - FMath::Clamp(Position.X, MinX, MinX + TileSize);
		const double DeltaY = Position.Y - FMath::Clamp(Position.Y, MinY, MinY + TileSize);
		return DeltaX * DeltaX + DeltaY * DeltaY <= double(Radius) * double(Radius);
	}

	static void ReleaseRef(FArcInfluenceTiledGrid& Grid, const FIntPoint& TileCoord)
	{
		FArcInfluenceTile* Tile = Grid.FindTile(TileCoord);
		if (Tile == nullptr)
		{
			return;
		}

		Tile->RefCount--;
		if (Tile->RefCount <= 0)
		{
			Grid.ReleaseTile(TileCoord);
		}
	}

	static void ReleaseAll(FArcInfluenceInvokerFragment& Fragment, FArcInfluenceTiledGrid& Grid)
	{
		for (const FIntPoint& TileCoord : Fragment.HeldTiles)
		{
			ReleaseRef(Grid, TileCoord);
		}
		Fragment.HeldTiles.Reset();
		Fragment.bFootprintValid = false;
	}

	/** Returns false when the tile cap refused a wanted tile, so the caller can leave the invoker pending. */
	static bool UpdateFootprint(FArcInfluenceInvokerFragment& Fragment, const FVector& Position, FArcInfluenceTiledGrid& Grid)
	{
		const float KeepRadius = Fragment.Radius + Fragment.ReleaseMargin;
		bool bComplete = true;

		TArray<FIntPoint, TInlineAllocator<9>> KeptTiles;
		for (const FIntPoint& TileCoord : Fragment.HeldTiles)
		{
			if (IsTileWithin(Grid, Position, KeepRadius, TileCoord))
			{
				KeptTiles.Add(TileCoord);
			}
			else
			{
				ReleaseRef(Grid, TileCoord);
			}
		}
		Fragment.HeldTiles = MoveTemp(KeptTiles);

		const FIntPoint MinTile = Grid.WorldToTile(FVector(Position.X - Fragment.Radius, Position.Y - Fragment.Radius, Position.Z));
		const FIntPoint MaxTile = Grid.WorldToTile(FVector(Position.X + Fragment.Radius, Position.Y + Fragment.Radius, Position.Z));

		for (int32 X = MinTile.X; X <= MaxTile.X; X++)
		{
			for (int32 Y = MinTile.Y; Y <= MaxTile.Y; Y++)
			{
				const FIntPoint TileCoord(X, Y);
				if (!IsTileWithin(Grid, Position, Fragment.Radius, TileCoord) || Fragment.HeldTiles.Contains(TileCoord))
				{
					continue;
				}

				// AllocateTile hands back a pointer into Grid.Tiles; it dies at the next AllocateTile/ReleaseTile.
				FArcInfluenceTile* Tile = Grid.AllocateTile(TileCoord);
				if (Tile == nullptr)
				{
					bComplete = false;
					continue;
				}

				Tile->RefCount++;
				Fragment.HeldTiles.Add(TileCoord);
			}
		}

		return bComplete;
	}

	static FArcInfluenceTiledGrid* ResolveGrid(UArcInfluenceMappingSubsystem& Subsystem, int32 GridIndex)
	{
		if (GridIndex < 0 || GridIndex >= Subsystem.GetGridCount())
		{
			return nullptr;
		}
		return Subsystem.GetTiledGrid(GridIndex);
	}
} // namespace ArcInfluenceInvokerPrivate

void UArcInfluenceInvokerTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FArcInfluenceInvokerFragment& Fragment = BuildContext.AddFragment_GetRef<FArcInfluenceInvokerFragment>();
	Fragment.Radius        = Radius;
	Fragment.ReleaseMargin = ReleaseMargin;
	Fragment.GridIndex     = GridIndex;
}

UArcInfluenceInvokerProcessor::UArcInfluenceInvokerProcessor()
	: EntityQuery{*this}
{
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
}

void UArcInfluenceInvokerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcInfluenceInvokerFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcInfluenceInvokerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	UArcInfluenceMappingSubsystem* Subsystem = World ? World->GetSubsystem<UArcInfluenceMappingSubsystem>() : nullptr;
	if (Subsystem == nullptr || Subsystem->GetGridCount() == 0)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcInfluenceInvokerUpdate);

	const int32 Budget = FMath::Max(ArcInfluenceInvokerPrivate::CVarInvokerUpdatesPerTick.GetValueOnGameThread(), 1);
	int32 BudgetRemaining = Budget;
	int32 SkipRemaining = PendingCursor;
	int32 PendingSeen = 0;

	bool& bLoggedMisconfigured = bLoggedNonTiledGrid;

	EntityQuery.ForEachEntityChunk(Context, [Subsystem, &BudgetRemaining, &SkipRemaining, &PendingSeen, &bLoggedMisconfigured](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcInfluenceInvokerFragment> Invokers = Ctx.GetMutableFragmentView<FArcInfluenceInvokerFragment>();
		TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcInfluenceInvokerFragment& Fragment = Invokers[EntityIt];

			FArcInfluenceTiledGrid* Grid = ArcInfluenceInvokerPrivate::ResolveGrid(*Subsystem, Fragment.GridIndex);
			if (Grid == nullptr)
			{
				UE_CLOGF(!bLoggedMisconfigured, LogMass, Warning,"ArcInfluenceInvoker: grid %d is missing or not tiled; its invokers allocate nothing.",
					Fragment.GridIndex);
				bLoggedMisconfigured = true;
				continue;
			}

			const FVector Position = Transforms[EntityIt].GetTransform().GetLocation();
			const FIntPoint Cell = Grid->WorldToCell(Position);
			if (Fragment.bFootprintValid && Cell == Fragment.LastCell)
			{
				continue;
			}

			PendingSeen++;
			if (SkipRemaining > 0)
			{
				SkipRemaining--;
				continue;
			}
			if (BudgetRemaining <= 0)
			{
				continue;
			}
			BudgetRemaining--;

			Fragment.LastCell = Cell;
			// A cap refusal leaves the invoker pending so it retries once capacity frees up.
			Fragment.bFootprintValid =
				ArcInfluenceInvokerPrivate::UpdateFootprint(Fragment, Position, *Grid);
		}
	});

	PendingCursor = BudgetRemaining > 0 ? 0 : PendingCursor + (Budget - BudgetRemaining);
	if (PendingCursor >= PendingSeen)
	{
		PendingCursor = 0;
	}
}

UArcInfluenceInvokerRemovalObserver::UArcInfluenceInvokerRemovalObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcInfluenceInvokerFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcInfluenceInvokerRemovalObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcInfluenceInvokerFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcInfluenceInvokerRemovalObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	UArcInfluenceMappingSubsystem* Subsystem = World ? World->GetSubsystem<UArcInfluenceMappingSubsystem>() : nullptr;
	if (Subsystem == nullptr || Subsystem->GetGridCount() == 0)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcInfluenceInvokerFragment> Invokers = Ctx.GetMutableFragmentView<FArcInfluenceInvokerFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcInfluenceInvokerFragment& Fragment = Invokers[EntityIt];

			FArcInfluenceTiledGrid* Grid = ArcInfluenceInvokerPrivate::ResolveGrid(*Subsystem, Fragment.GridIndex);
			if (Grid == nullptr)
			{
				continue;
			}

			ArcInfluenceInvokerPrivate::ReleaseAll(Fragment, *Grid);
		}
	});
}
