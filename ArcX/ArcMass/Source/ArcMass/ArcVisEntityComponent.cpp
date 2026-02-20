// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisEntityComponent.h"

#include "ArcMassEntityVisualization.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "Engine/World.h"

UArcVisEntityComponent::UArcVisEntityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArcVisEntityComponent::BeginPlay()
{
	Super::BeginPlay();

	// If we have an entity config and no entity handle yet, this is a pre-placed actor
	if (EntityConfig && !EntityHandle.IsValid())
	{
		RegisterPrePlacedActor();
	}
}

void UArcVisEntityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bOwnsEntity)
	{
		UnregisterPrePlacedActor();
	}

	Super::EndPlay(EndPlayReason);
}

void UArcVisEntityComponent::NotifyVisActorCreated(FMassEntityHandle InEntityHandle)
{
	EntityHandle = InEntityHandle;
	bOwnsEntity = false;
	OnVisActorCreated.Broadcast(GetOwner());
}

void UArcVisEntityComponent::NotifyVisActorPreDestroy()
{
	OnVisActorPreDestroy.Broadcast(GetOwner());
	EntityHandle = FMassEntityHandle();
}

void UArcVisEntityComponent::RegisterPrePlacedActor()
{
	UWorld* World = GetWorld();
	if (!World || !EntityConfig)
	{
		return;
	}

	UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
	if (!SpawnerSubsystem)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* VisSubsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!VisSubsystem)
	{
		return;
	}

	const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);
	if (!EntityTemplate.IsValid())
	{
		return;
	}

	// Spawn the Mass entity
	TArray<FMassEntityHandle> SpawnedEntities;
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
		SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, SpawnedEntities);

	if (SpawnedEntities.IsEmpty())
	{
		return;
	}

	EntityHandle = SpawnedEntities[0];
	bOwnsEntity = true;

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	// Set the transform to match the actor
	FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(EntityHandle);
	if (TransformFragment)
	{
		TransformFragment->SetTransform(GetOwner()->GetActorTransform());
	}

	// The init observer will have already run and created an ISM instance or spawned a new actor.
	// We need to fix that — this pre-placed actor IS the actor representation.
	FArcVisRepresentationFragment* Rep = EntityManager.GetFragmentDataPtr<FArcVisRepresentationFragment>(EntityHandle);
	if (Rep)
	{
		if (Rep->bIsActorRepresentation)
		{
			// Observer spawned an actor — destroy it, we already have one
			FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(EntityHandle);
			if (ActorFragment)
			{
				if (AActor* SpawnedActor = ActorFragment->GetMutable())
				{
					if (SpawnedActor != GetOwner())
					{
						SpawnedActor->Destroy();
					}
				}
			}
		}
		else
		{
			// Observer created ISM instance — remove it
			const FArcVisConfigFragment* Config = EntityManager.GetConstSharedFragmentDataPtr<FArcVisConfigFragment>(EntityHandle);
			if (Config)
			{
				UStaticMesh* MeshToRemove = Rep->CurrentISMMesh ? Rep->CurrentISMMesh.Get() : Config->StaticMesh.Get();
				if (MeshToRemove && Rep->ISMInstanceId != INDEX_NONE)
				{
					VisSubsystem->RemoveISMInstance(Rep->GridCoords, Config->ISMManagerClass, MeshToRemove, Rep->ISMInstanceId, EntityManager);
					Rep->ISMInstanceId = INDEX_NONE;
				}
			}
		}

		// Set to actor representation using the pre-placed actor
		Rep->bIsActorRepresentation = true;
		Rep->CurrentISMMesh = nullptr;
	}

	// Bind this actor to the entity via FMassActorFragment
	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(EntityHandle);
	if (ActorFragment)
	{
		ActorFragment->SetAndUpdateHandleMap(EntityHandle, GetOwner(), false);
	}
}

void UArcVisEntityComponent::UnregisterPrePlacedActor()
{
	if (!EntityHandle.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
	if (SpawnerSubsystem)
	{
		SpawnerSubsystem->DestroyEntities(MakeArrayView(&EntityHandle, 1));
	}

	EntityHandle = FMassEntityHandle();
	bOwnsEntity = false;
}
