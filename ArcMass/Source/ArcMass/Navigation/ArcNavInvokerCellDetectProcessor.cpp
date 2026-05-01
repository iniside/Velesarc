// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Navigation/ArcNavInvokerCellDetectProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "ArcMass/Navigation/ArcMassNavInvokerTypes.h"
#include "ArcMass/Navigation/ArcMassNavInvokerSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcNavInvokerCellDetectProcessor)

UArcNavInvokerCellDetectProcessor::UArcNavInvokerCellDetectProcessor()
	: EntityQuery{*this}
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	bRequiresGameThreadExecution = false;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcNavInvokerCellDetectProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FMassNavInvokerFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassNavInvokerStaticTag>(EMassFragmentPresence::None);
}

void UArcNavInvokerCellDetectProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcMassNavInvokerSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcMassNavInvokerSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcNavInvokerCellDetect);

	const float CellSize = Subsystem->GetCellSize();
	const TSparseArray<FArcNavInvokerData>& Slots = Subsystem->GetSlots();

	TArray<FArcNavTileDiffEntry> LocalDiffs;
	TArray<FMassEntityHandle> DirtyEntities;

	EntityQuery.ForEachEntityChunk(Context, [CellSize, &Slots, &LocalDiffs, &DirtyEntities](FMassExecutionContext& Ctx)
	{
		TArrayView<FMassNavInvokerFragment> Invokers = Ctx.GetMutableFragmentView<FMassNavInvokerFragment>();
		TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassNavInvokerFragment& Fragment = Invokers[EntityIt];

			if (Fragment.InvokerIndex == INDEX_NONE)
			{
				continue;
			}

			const FVector NewLocation = Transforms[EntityIt].GetTransform().GetLocation();
			const FIntPoint NewCell = ArcNavInvoker::WorldToCell(NewLocation, CellSize);

			if (NewCell == Fragment.LastCell)
			{
				continue;
			}

			if (!Slots.IsValidIndex(Fragment.InvokerIndex))
			{
				continue;
			}

			const float TileDim = Fragment.CachedTileDim;
			if (TileDim <= 0.f)
			{
				continue;
			}

			const FVector NavmeshOrigin = Fragment.CachedNavmeshOrigin;
			const FArcNavInvokerData& OldData = Slots[Fragment.InvokerIndex];

			FArcNavTileDiffEntry Diff;
			Diff.SlotIndex = Fragment.InvokerIndex;
			Diff.NewLocation = NewLocation;
			Diff.Priority = Fragment.Priority;

			// Pass 1: tiles gained — in new footprint but not in old
			ArcNavInvoker::ForEachTileInRadius(
				NewLocation,
				Fragment.GenerationRadius,
				NavmeshOrigin,
				TileDim,
				[&OldData, &Diff, &NavmeshOrigin, TileDim](const FIntPoint& Tile)
				{
					if (!ArcNavInvoker::IsTileInRadius(OldData.Location, OldData.RadiusMin, Tile, NavmeshOrigin, TileDim))
					{
						Diff.TileGains.Add(Tile);
					}
				});

			// Pass 2: tiles lost — in old removal radius but outside new removal radius (hysteresis)
			ArcNavInvoker::ForEachTileInRadius(
				OldData.Location,
				OldData.RadiusMax,
				NavmeshOrigin,
				TileDim,
				[&NewLocation, &Fragment, &Diff, &NavmeshOrigin, TileDim](const FIntPoint& Tile)
				{
					if (!ArcNavInvoker::IsTileInRadius(NewLocation, Fragment.RemovalRadius, Tile, NavmeshOrigin, TileDim))
					{
						Diff.TileLosses.Add(Tile);
					}
				});

			if (Diff.TileGains.Num() > 0 || Diff.TileLosses.Num() > 0)
			{
				LocalDiffs.Add(MoveTemp(Diff));
				DirtyEntities.Add(Ctx.GetEntity(EntityIt));
			}

			Fragment.LastCell = NewCell;
		}
	});

	if (LocalDiffs.Num() > 0)
	{
		Subsystem->SwapInDiffs(MoveTemp(LocalDiffs));

		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Context.GetWorld());
		if (SignalSubsystem != nullptr)
		{
			SignalSubsystem->SignalEntitiesDeferred(Context, UE::ArcMass::Signals::NavInvokerCellChanged, DirtyEntities);
		}
	}
}
