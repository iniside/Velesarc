// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcMassReplicationCaptureProcessor_TEST.h"

#include "Engine/World.h"
#include "Fragments/ArcMassReplicatedTag.h"
#include "MassExecutionContext.h"
#include "Replication/ArcMassEntityVessel.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"

UArcMassReplicationCaptureProcessor_TEST::UArcMassReplicationCaptureProcessor_TEST()
{
    bRequiresGameThreadExecution = true;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcMassReplicationCaptureProcessor_TEST::ConfigureQueries(
    const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.AddTagRequirement<FArcMassEntityReplicatedTag>(EMassFragmentPresence::All);
}

void UArcMassReplicationCaptureProcessor_TEST::Execute(
    FMassEntityManager& EntityManager,
    FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    UArcMassEntityReplicationSubsystem* Subsystem =
        World != nullptr ? World->GetSubsystem<UArcMassEntityReplicationSubsystem>() : nullptr;
    if (Subsystem == nullptr)
    {
        return;
    }

    EntityQuery.ForEachEntityChunk(Context, [Subsystem, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const TConstArrayView<FMassEntityHandle> Entities = ChunkContext.GetEntities();
        UE_LOG(LogTemp, Verbose, TEXT("ArcMassReplication[Capture_TEST]: chunk NumEntities=%d World=%s"),
            Entities.Num(), *Subsystem->GetWorld()->GetName());
        for (const FMassEntityHandle& Entity : Entities)
        {
            UArcMassEntityVessel* Vessel = Subsystem->FindVesselForEntity(Entity);
            UE_LOG(LogTemp, Verbose, TEXT("ArcMassReplication[Capture_TEST]: Entity=(Idx=%d,Ser=%d) Vessel=%s"),
                Entity.Index, Entity.SerialNumber,
                Vessel != nullptr ? *Vessel->GetName() : TEXT("<null>"));
            if (Vessel == nullptr)
            {
                continue;
            }
            Vessel->CaptureEntityStateForReplication(EntityManager);
        }
    });
}
