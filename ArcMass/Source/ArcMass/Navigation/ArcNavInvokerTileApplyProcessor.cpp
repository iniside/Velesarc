// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Navigation/ArcNavInvokerTileApplyProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/Navigation/NavigationInvokerPriority.h"
#include "ArcMass/Navigation/ArcMassNavInvokerTypes.h"
#include "ArcMass/Navigation/ArcMassNavInvokerSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcNavInvokerTileApplyProcessor)

UArcNavInvokerTileApplyProcessor::UArcNavInvokerTileApplyProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcNavInvokerTileApplyProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::NavInvokerCellChanged);
}

void UArcNavInvokerTileApplyProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FMassNavInvokerFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcNavInvokerTileApplyProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	UArcMassNavInvokerSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcMassNavInvokerSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcNavInvokerTileApply);

	TArray<FArcNavTileDiffEntry> Diffs = Subsystem->DrainDiffs();
	if (Diffs.Num() == 0)
	{
		return;
	}

	// Merge all tile changes across entities into batch operations.
	// Track per-tile refcount deltas and highest priority for gained tiles.
	struct FTileDeltaEntry
	{
		int32 Delta = 0;
		ENavigationInvokerPriority Priority = ENavigationInvokerPriority::Default;
	};
	TMap<FIntPoint, FTileDeltaEntry> TileDelta;

	for (const FArcNavTileDiffEntry& Diff : Diffs)
	{
		for (const FIntPoint& Tile : Diff.TileGains)
		{
			FTileDeltaEntry& Entry = TileDelta.FindOrAdd(Tile);
			Entry.Delta += 1;
			if (Diff.Priority > Entry.Priority)
			{
				Entry.Priority = Diff.Priority;
			}
		}
		for (const FIntPoint& Tile : Diff.TileLosses)
		{
			TileDelta.FindOrAdd(Tile).Delta -= 1;
		}
	}

	// Read current refcounts, compute which tiles are truly new or truly removed
	TArray<FNavMeshDirtyTileElement> TilesToRebuild;
	TArray<FIntPoint> TilesToRemove;
	TilesToRebuild.Reserve(TileDelta.Num());
	TilesToRemove.Reserve(TileDelta.Num());

	const TMap<FIntPoint, int32>& CurrentRefCounts = Subsystem->GetTileRefCounts();

	for (const TPair<FIntPoint, FTileDeltaEntry>& DeltaEntry : TileDelta)
	{
		if (DeltaEntry.Value.Delta == 0)
		{
			continue; // net-zero: tile gained by one entity, lost by another
		}

		const FIntPoint& Tile = DeltaEntry.Key;
		const int32 Delta = DeltaEntry.Value.Delta;

		const int32* CurrentCount = CurrentRefCounts.Find(Tile);
		const int32 OldCount = (CurrentCount != nullptr) ? *CurrentCount : 0;
		const int32 NewCount = OldCount + Delta;

		if (OldCount == 0 && NewCount > 0)
		{
			FNavMeshDirtyTileElement Element;
			Element.Coordinates = Tile;
			Element.InvokerDistanceSquared = 0.f;
			Element.InvokerPriority = DeltaEntry.Value.Priority;
			TilesToRebuild.Add(Element);
		}
		else if (OldCount > 0 && NewCount <= 0)
		{
			TilesToRemove.Add(Tile);
		}
	}

	// Apply refcount changes and update slot locations
	for (const FArcNavTileDiffEntry& Diff : Diffs)
	{
		for (const FIntPoint& Tile : Diff.TileGains)
		{
			Subsystem->IncrementTileRefCount(Tile);
		}
		for (const FIntPoint& Tile : Diff.TileLosses)
		{
			Subsystem->DecrementTileRefCount(Tile);
		}

		Subsystem->UpdateSlotLocation(Diff.SlotIndex, Diff.NewLocation);
	}

	// Single batched navmesh operations
	if (TilesToRebuild.Num() > 0 || TilesToRemove.Num() > 0)
	{
		Subsystem->ApplyBatchedTileChanges(TilesToRebuild, TilesToRemove);
	}
}
