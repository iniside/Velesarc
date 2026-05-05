// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcMassReplicationSourceTrackingProcessor.h"
#include "Fragments/ArcMassReplicationSourceFragment.h"
#include "Fragments/ArcMassReplicationSourceTag.h"
#include "Subsystem/ArcMassEntityReplicationProxySubsystem.h"
#include "Spatial/ArcMassSpatialGrid.h"
#include "Mass/EntityFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Engine/World.h"

namespace ArcMassReplication
{
	extern const FName SourceCellChangedSignal = FName(TEXT("ArcMassReplicationSourceCellChanged"));
}

UArcMassReplicationSourceTrackingProcessor::UArcMassReplicationSourceTrackingProcessor()
{
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcMassReplicationSourceTrackingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	SourceQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	SourceQuery.AddRequirement<FArcMassReplicationSourceFragment>(EMassFragmentAccess::ReadWrite);
	SourceQuery.AddTagRequirement<FArcMassReplicationSourceTag>(EMassFragmentPresence::All);
}

void UArcMassReplicationSourceTrackingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMassEntityReplicationProxySubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationProxySubsystem>();
	if (!Subsystem)
	{
		return;
	}

	float CellSize = 10000.f;
	const TMap<UArcMassEntityReplicationProxySubsystem::FArchetypeKey, FArcMassSpatialGrid>& Grids = Subsystem->GetArchetypeGrids();
	for (const TPair<UArcMassEntityReplicationProxySubsystem::FArchetypeKey, FArcMassSpatialGrid>& Pair : Grids)
	{
		CellSize = Pair.Value.CellSize;
		break;
	}

	TArray<FMassEntityHandle> ChangedEntities;

	SourceQuery.ForEachEntityChunk(Context,
		[Subsystem, CellSize, &ChangedEntities](FMassExecutionContext& ChunkContext)
		{
			TConstArrayView<FTransformFragment> Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
			TArrayView<FArcMassReplicationSourceFragment> Sources = ChunkContext.GetMutableFragmentView<FArcMassReplicationSourceFragment>();

			for (int32 Idx = 0; Idx < ChunkContext.GetNumEntities(); ++Idx)
			{
				FArcMassReplicationSourceFragment& Source = Sources[Idx];
				if (Source.ConnectionId == 0)
				{
					continue;
				}

				FVector Position = Transforms[Idx].GetTransform().GetLocation();
				FIntVector2 NewCell(
					FMath::FloorToInt32(Position.X / CellSize),
					FMath::FloorToInt32(Position.Y / CellSize));

				Subsystem->UpdateSourceState(Source.ConnectionId, Position, NewCell);

				if (Source.CachedCell != NewCell)
				{
					Source.CachedCell = NewCell;
					ChangedEntities.Add(ChunkContext.GetEntity(Idx));
				}
			}
		});

	if (ChangedEntities.Num() > 0)
	{
		UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntities(ArcMassReplication::SourceCellChangedSignal, ChangedEntities);
		}
	}
}
