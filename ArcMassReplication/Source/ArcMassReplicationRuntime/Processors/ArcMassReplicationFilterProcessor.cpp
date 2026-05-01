// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcMassReplicationFilterProcessor.h"
#include "Fragments/ArcMassReplicationSourceTag.h"
#include "Fragments/ArcMassReplicationSourceFragment.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"
#include "Spatial/ArcMassSpatialGrid.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassSignalTypes.h"
#include "Engine/World.h"

namespace ArcMassReplication
{
	extern const FName SourceCellChangedSignal;
}

UArcMassReplicationFilterProcessor::UArcMassReplicationFilterProcessor()
{
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcMassReplicationFilterProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	if (const UWorld* World = GetWorld())
	{
		if (UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>())
		{
			SubscribeToSignal(*SignalSubsystem, ArcMassReplication::SourceCellChangedSignal);
		}
	}
}

void UArcMassReplicationFilterProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassReplicationSourceFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FArcMassReplicationSourceTag>(EMassFragmentPresence::All);
}

void UArcMassReplicationFilterProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMassEntityReplicationSubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	const int32 EnterRadius = FMath::FloorToInt32(Subsystem->EnterRadiusCells);
	const int32 ExitRadius = FMath::FloorToInt32(Subsystem->ExitRadiusCells);

	TArray<uint32> SourceConnections = Subsystem->GetAllSourceConnectionIds();

	for (uint32 ConnectionId : SourceConnections)
	{
		const UArcMassEntityReplicationSubsystem::FSourceState* SourceState = Subsystem->GetSourceState(ConnectionId);
		if (!SourceState)
		{
			continue;
		}

		FIntVector2 SourceCell = SourceState->Cell;

		const TMap<UArcMassEntityReplicationSubsystem::FArchetypeKey, FArcMassSpatialGrid>& Grids = Subsystem->GetArchetypeGrids();

		for (const TPair<UArcMassEntityReplicationSubsystem::FArchetypeKey, FArcMassSpatialGrid>& GridPair : Grids)
		{
			const FArcMassSpatialGrid& Grid = GridPair.Value;

			for (int32 DX = -ExitRadius; DX <= ExitRadius; ++DX)
			{
				for (int32 DY = -ExitRadius; DY <= ExitRadius; ++DY)
				{
					FIntVector2 Cell(SourceCell.X + DX, SourceCell.Y + DY);
					const int32 ChebyshevDist = FMath::Max(FMath::Abs(DX), FMath::Abs(DY));

					const bool bIsCurrentlyRelevant = Subsystem->IsCellRelevantForConnection(ConnectionId, Cell);
					const bool bHasEntities = Grid.CellToEntities.Contains(Cell);

					if (!bHasEntities)
					{
						if (bIsCurrentlyRelevant)
						{
							Subsystem->SetCellIrrelevantForConnection(ConnectionId, Cell);
						}
						continue;
					}

					if (!bIsCurrentlyRelevant && ChebyshevDist <= EnterRadius)
					{
						Subsystem->SetCellRelevantForConnection(ConnectionId, Cell);
					}
					else if (bIsCurrentlyRelevant && ChebyshevDist > ExitRadius)
					{
						Subsystem->SetCellIrrelevantForConnection(ConnectionId, Cell);
					}
				}
			}
		}
	}
}
