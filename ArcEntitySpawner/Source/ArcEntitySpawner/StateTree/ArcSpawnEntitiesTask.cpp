// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSpawnEntitiesTask.h"

#include "ArcEntitySpawner/ArcEntitySpawnerSubsystem.h"
#include "ArcEntitySpawner/ArcEntitySpawnerTypes.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMass/ArcMassFragments.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"

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

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	UArcEntitySpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UArcEntitySpawnerSubsystem>();
	if (!SignalSubsystem)
	{
		InstanceData.bSuccess = false;
		return EStateTreeRunStatus::Failed;
	}

	// Validate entity config
	const UMassEntityConfigAsset* EntityConfig = InstanceData.EntityType.GetEntityConfig();
	if (!EntityConfig)
	{
		UE_LOGF(LogArcEntitySpawner, Error,"FArcSpawnEntitiesTask: No EntityConfig configured.");
		InstanceData.bSuccess = false;
		return EStateTreeRunStatus::Failed;
	}

	UArcMassEntityConfigAsset* ArcCfg = Cast<UArcMassEntityConfigAsset>(const_cast<UMassEntityConfigAsset*>(EntityConfig));
	if (!ArcCfg)
	{
		UE_LOGF(LogArcEntitySpawner, Error,"FArcSpawnEntitiesTask: EntityConfig '%ls' is not a UArcMassEntityConfigAsset — skipping spawn.",
			*EntityConfig->GetName());
		InstanceData.bSuccess = false;
		return EStateTreeRunStatus::Failed;
	}

	UArcMassSpawnerSubsystem* ArcSpawner = World->GetSubsystem<UArcMassSpawnerSubsystem>();
	if (!ArcSpawner)
	{
		UE_LOGF(LogArcEntitySpawner, Error,"FArcSpawnEntitiesTask: No UArcMassSpawnerSubsystem available.");
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
	int32 SpawnCount = InstanceData.Count;

	if (NumLocations > 0 && SpawnCount > NumLocations)
	{
		UE_LOGF(LogArcEntitySpawner, Warning,"FArcSpawnEntitiesTask: Requested %d entities but only %d spawn locations available — clamping. "
				 "Configure the query to return enough results (e.g. TopN or AllPassing mode).",
			SpawnCount, NumLocations);
		SpawnCount = NumLocations;
	}

	// Read spawner's PersistenceGuid for stamping on spawned entities
	FGuid SpawnerPersistenceGuid;
	if (const FArcMassPersistenceFragment* SpawnerPersistFrag =
		EntityManager.GetFragmentDataPtr<FArcMassPersistenceFragment>(SpawnerEntity))
	{
		SpawnerPersistenceGuid = SpawnerPersistFrag->PersistenceGuid;
	}

	// Spawn entities — one per location so each gets the correct transform via FEntityBuilder.
	// After spawn, adjust Z by capsule half-height (readable only after entity exists).
	InstanceData.SpawnedEntities.Reset();
	for (int32 i = 0; i < SpawnCount; ++i)
	{
		const FVector SpawnLocationBase = (i < NumLocations) ? Locations[i] : FVector::ZeroVector;
		const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocationBase);
		const FMassEntityHandle SpawnedEntity = ArcSpawner->SpawnEntity(ArcCfg, SpawnTransform);

		if (!SpawnedEntity.IsValid())
		{
			continue;
		}

		InstanceData.SpawnedEntities.Add(SpawnedEntity);

		// Capsule Z-adjustment — half-height is only readable after entity creation
		if (i < NumLocations)
		{
			const FArcAgentCapsuleFragment* CapsuleFrag =
				EntityManager.GetFragmentDataPtr<FArcAgentCapsuleFragment>(SpawnedEntity);
			if (CapsuleFrag)
			{
				FTransformFragment* TransformFrag =
					EntityManager.GetFragmentDataPtr<FTransformFragment>(SpawnedEntity);
				if (TransformFrag)
				{
					FVector AdjustedLocation = SpawnLocationBase;
					AdjustedLocation.Z += CapsuleFrag->HalfHeight;
					TransformFrag->GetMutableTransform().SetLocation(AdjustedLocation);
				}
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
		Message.EntityConfig = const_cast<UMassEntityConfigAsset*>(EntityConfig);
		SpawnerSubsystem->BroadcastSpawnEvent(Message, MessageChannel);
	}

	InstanceData.bSuccess = true;

	// Signal the entity so the StateTree wakes up
	SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {SpawnerEntity});

	return EStateTreeRunStatus::Succeeded;
}
