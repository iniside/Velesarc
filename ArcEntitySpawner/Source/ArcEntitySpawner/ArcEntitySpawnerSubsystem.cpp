// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEntitySpawnerSubsystem.h"

#include "ArcEntitySpawner.h"
#include "ArcEntitySpawnerTypes.h"
#include "ArcMass/Persistence/ArcMassEntityPersistenceSubsystem.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "AsyncGameplayMessageSystem.h"
#include "AsyncMessageWorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEntitySpawnerSubsystem)

void UArcEntitySpawnerSubsystem::RegisterSpawnedEntities(FMassEntityHandle Spawner, TConstArrayView<FMassEntityHandle> Entities)
{
	TArray<FMassEntityHandle>& SpawnedArray = SpawnerToEntities.FindOrAdd(Spawner);
	SpawnedArray.Append(Entities.GetData(), Entities.Num());

	for (const FMassEntityHandle& Entity : Entities)
	{
		EntityToSpawner.Add(Entity, Spawner);
	}
}

void UArcEntitySpawnerSubsystem::UnregisterSpawnedEntities(FMassEntityHandle Spawner, TConstArrayView<FMassEntityHandle> Entities)
{
	if (TArray<FMassEntityHandle>* SpawnedArray = SpawnerToEntities.Find(Spawner))
	{
		for (const FMassEntityHandle& Entity : Entities)
		{
			SpawnedArray->RemoveSwap(Entity);
			EntityToSpawner.Remove(Entity);
		}

		if (SpawnedArray->IsEmpty())
		{
			SpawnerToEntities.Remove(Spawner);
		}
	}
}

void UArcEntitySpawnerSubsystem::UnregisterAllForSpawner(FMassEntityHandle Spawner)
{
	if (TArray<FMassEntityHandle>* SpawnedArray = SpawnerToEntities.Find(Spawner))
	{
		for (const FMassEntityHandle& Entity : *SpawnedArray)
		{
			EntityToSpawner.Remove(Entity);
		}
		SpawnerToEntities.Remove(Spawner);
	}
}

TArray<FMassEntityHandle> UArcEntitySpawnerSubsystem::GetSpawnedEntities(FMassEntityHandle Spawner) const
{
	if (const TArray<FMassEntityHandle>* SpawnedArray = SpawnerToEntities.Find(Spawner))
	{
		return *SpawnedArray;
	}
	return {};
}

FMassEntityHandle UArcEntitySpawnerSubsystem::GetSpawnerForEntity(FMassEntityHandle SpawnedEntity) const
{
	if (const FMassEntityHandle* Spawner = EntityToSpawner.Find(SpawnedEntity))
	{
		return *Spawner;
	}
	return FMassEntityHandle();
}

void UArcEntitySpawnerSubsystem::BroadcastSpawnEvent(const FArcEntitiesSpawnedMessage& Message, FGameplayTag Channel)
{
	if (!Channel.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSharedPtr<FAsyncGameplayMessageSystem> MessageSystem =
		UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(World);

	if (MessageSystem.IsValid())
	{
		FAsyncMessageId MessageId(Channel);
		MessageSystem->QueueMessageForBroadcast(MessageId, FConstStructView::Make(Message));
	}
}

void UArcEntitySpawnerSubsystem::RegisterActorSpawner(const FGuid& SpawnerGuid, AActor* SpawnerActor)
{
	if (SpawnerGuid.IsValid() && SpawnerActor)
	{
		SpawnerGuidToActor.Add(SpawnerGuid, SpawnerActor);
	}
}

void UArcEntitySpawnerSubsystem::UnregisterActorSpawner(const FGuid& SpawnerGuid)
{
	SpawnerGuidToActor.Remove(SpawnerGuid);
}

AActor* UArcEntitySpawnerSubsystem::GetActorSpawnerByGuid(const FGuid& SpawnerGuid) const
{
	if (const TWeakObjectPtr<AActor>* Found = SpawnerGuidToActor.Find(SpawnerGuid))
	{
		return Found->Get();
	}
	return nullptr;
}

void UArcEntitySpawnerSubsystem::OnSpawnedEntityDied(const FGuid& SpawnerGuid,
	const FGuid& EntityPersistenceGuid, FMassEntityHandle EntityHandle)
{
	// Remove from persistence
	if (UArcMassEntityPersistenceSubsystem* PersistSub =
		GetWorld()->GetSubsystem<UArcMassEntityPersistenceSubsystem>())
	{
		PersistSub->ActiveEntities.Remove(EntityPersistenceGuid);

		for (auto& Pair : PersistSub->CellEntityMap)
		{
			Pair.Value.Remove(EntityPersistenceGuid);
		}
	}

	// Notify actor-mode spawner
	if (AActor* SpawnerActor = GetActorSpawnerByGuid(SpawnerGuid))
	{
		if (AArcEntitySpawner* Spawner = Cast<AArcEntitySpawner>(SpawnerActor))
		{
			Spawner->OnSpawnedEntityDied(EntityPersistenceGuid, EntityHandle);
		}
	}
}
