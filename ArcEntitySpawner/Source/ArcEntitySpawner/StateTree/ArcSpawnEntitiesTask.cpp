// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSpawnEntitiesTask.h"

#include "ArcEntitySpawner/ArcEntitySpawnerSubsystem.h"
#include "ArcEntitySpawner/ArcEntitySpawnerTypes.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "MassSignalSubsystem.h"
#include "MassSpawnerSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSpawnEntitiesTask)

EStateTreeRunStatus FArcSpawnEntitiesTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
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

	// Validate entity config
	const UMassEntityConfigAsset* EntityConfig = InstanceData.EntityType.GetEntityConfig();
	if (!EntityConfig)
	{
		UE_LOG(LogArcEntitySpawner, Error, TEXT("FArcSpawnEntitiesTask: No EntityConfig configured."));
		InstanceData.bSuccess = false;
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);
	if (!EntityTemplate.IsValid())
	{
		UE_LOG(LogArcEntitySpawner, Error, TEXT("FArcSpawnEntitiesTask: EntityTemplate is invalid."));
		InstanceData.bSuccess = false;
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle SpawnerEntity = MassCtx.GetEntity();
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();

	// Extract locations from TQS results
	TArray<FVector> Locations;
	Locations.Reserve(InstanceData.SpawnLocations.Num());
	for (const FArcTQSTargetItem& Item : InstanceData.SpawnLocations)
	{
		Locations.Add(Item.Location);
	}

	const int32 NumLocations = Locations.Num();
	const int32 SpawnCount = InstanceData.Count;

	// Read spawner's PersistenceGuid for stamping on spawned entities
	FGuid SpawnerPersistenceGuid;
	if (const FArcMassPersistenceFragment* SpawnerPersistFrag =
		EntityManager.GetFragmentDataPtr<FArcMassPersistenceFragment>(SpawnerEntity))
	{
		SpawnerPersistenceGuid = SpawnerPersistFrag->PersistenceGuid;
	}

	// Spawn entities — hold CreationContext alive for fragment modification
	InstanceData.SpawnedEntities.Reset();
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
		SpawnerSystem->SpawnEntities(EntityTemplate.GetTemplateID(), SpawnCount,
			FConstStructView(), nullptr, InstanceData.SpawnedEntities);

	// Set transforms and optionally mark spawned-by while creation context is alive
	for (int32 i = 0; i < InstanceData.SpawnedEntities.Num(); ++i)
	{
		const FMassEntityHandle& SpawnedEntity = InstanceData.SpawnedEntities[i];

		if (FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(SpawnedEntity))
		{
			if (NumLocations > 0)
			{
				TransformFrag->GetMutableTransform().SetLocation(Locations[i % NumLocations]);
			}
		}

		// If entity config includes FArcSpawnedByFragment, set the spawner reference
		if (bMarkSpawnedEntities)
		{
			if (FArcSpawnedByFragment* SpawnedByFrag = EntityManager.GetFragmentDataPtr<FArcSpawnedByFragment>(SpawnedEntity))
			{
				SpawnedByFrag->SpawnerEntity = SpawnerEntity;
				SpawnedByFrag->SpawnerGuid = SpawnerPersistenceGuid;
			}
		}
	}

	// CreationContext released here → deferred commands flush → observers fire
	CreationContext.Reset();

	// Update spawner's tracking fragment
	if (FArcSpawnerFragment* SpawnerFrag = EntityManager.GetFragmentDataPtr<FArcSpawnerFragment>(SpawnerEntity))
	{
		SpawnerFrag->SpawnedEntities.Append(InstanceData.SpawnedEntities);

		// Track spawned entity GUIDs for persistence
		for (const FMassEntityHandle& Spawned : InstanceData.SpawnedEntities)
		{
			if (const FArcMassPersistenceFragment* PersistFrag =
				EntityManager.GetFragmentDataPtr<FArcMassPersistenceFragment>(Spawned))
			{
				if (PersistFrag->PersistenceGuid.IsValid())
				{
					SpawnerFrag->SpawnedEntityGuids.Add(PersistFrag->PersistenceGuid);
				}
			}
		}
	}

	// Register with subsystem
	if (SpawnerSubsystem)
	{
		SpawnerSubsystem->RegisterSpawnedEntities(SpawnerEntity, InstanceData.SpawnedEntities);
	}

	// Broadcast async message if channel is configured
	if (MessageChannel.IsValid() && SpawnerSubsystem)
	{
		FArcEntitiesSpawnedMessage Message;
		Message.SpawnedEntities = InstanceData.SpawnedEntities;
		Message.SpawnLocations = Locations;
		Message.SpawnerEntity = SpawnerEntity;
		Message.EntityConfig = EntityConfig;
		SpawnerSubsystem->BroadcastSpawnEvent(Message, MessageChannel);
	}

	InstanceData.bSuccess = true;

	// Signal the entity so the StateTree wakes up
	SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {SpawnerEntity});

	return EStateTreeRunStatus::Succeeded;
}
