// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcDespawnEntitiesTask.h"

#include "ArcEntitySpawner/ArcEntitySpawnerSubsystem.h"
#include "ArcEntitySpawner/ArcEntitySpawnerTypes.h"
#include "MassSignalSubsystem.h"
#include "MassSpawnerSubsystem.h"
#include "MassStateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcDespawnEntitiesTask)

EStateTreeRunStatus FArcDespawnEntitiesTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		InstanceData.bSuccess = false;
		return EStateTreeRunStatus::Failed;
	}

	UMassSpawnerSubsystem* SpawnerSystem = World->GetSubsystem<UMassSpawnerSubsystem>();
	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	UArcEntitySpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UArcEntitySpawnerSubsystem>();
	if (!SpawnerSystem || !SignalSubsystem)
	{
		InstanceData.bSuccess = false;
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle SpawnerEntity = MassCtx.GetEntity();
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();

	// Determine which entities to despawn
	TArray<FMassEntityHandle> EntitiesToDestroy;

	if (InstanceData.bDespawnAll)
	{
		// Read from spawner's tracking fragment
		if (const FArcSpawnerFragment* SpawnerFrag = EntityManager.GetFragmentDataPtr<FArcSpawnerFragment>(SpawnerEntity))
		{
			EntitiesToDestroy = SpawnerFrag->SpawnedEntities;
		}
	}
	else
	{
		EntitiesToDestroy = InstanceData.EntitiesToDespawn;
	}

	if (EntitiesToDestroy.Num() > 0)
	{
		// Destroy entities
		SpawnerSystem->DestroyEntities(EntitiesToDestroy);

		// Update spawner fragment
		if (FArcSpawnerFragment* SpawnerFrag = EntityManager.GetFragmentDataPtr<FArcSpawnerFragment>(SpawnerEntity))
		{
			if (InstanceData.bDespawnAll)
			{
				SpawnerFrag->SpawnedEntities.Reset();
			}
			else
			{
				for (const FMassEntityHandle& Entity : EntitiesToDestroy)
				{
					SpawnerFrag->SpawnedEntities.RemoveSwap(Entity);
				}
			}
		}

		// Unregister from subsystem
		if (SpawnerSubsystem)
		{
			if (InstanceData.bDespawnAll)
			{
				SpawnerSubsystem->UnregisterAllForSpawner(SpawnerEntity);
			}
			else
			{
				SpawnerSubsystem->UnregisterSpawnedEntities(SpawnerEntity, EntitiesToDestroy);
			}
		}
	}

	InstanceData.bSuccess = true;

	// Signal the entity so the StateTree wakes up
	SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {SpawnerEntity});

	return EStateTreeRunStatus::Succeeded;
}
