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

	// Spawn the Mass entity. The init observer (FArcVisEntityTag Add) fires when CreationContext
	// is released, so we set the transform first, then release to ensure the observer sees it.
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	TArray<FMassEntityHandle> SpawnedEntities;
	{
		TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
			SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, SpawnedEntities);

		if (SpawnedEntities.IsEmpty())
		{
			return;
		}

		EntityHandle = SpawnedEntities[0];
		bOwnsEntity = true;

		// Set the transform before the observer fires so it uses the correct position
		FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(EntityHandle);
		if (TransformFragment)
		{
			TransformFragment->SetTransform(GetOwner()->GetActorTransform());
		}
	} // CreationContext released → init observer fires here

	// Store a reference to this pre-placed actor so the activate/deactivate processors
	// can reuse it instead of spawning/destroying.
	AActor* Owner = GetOwner();
	EntityManager.AddFragmentToEntity(EntityHandle, FArcVisPrePlacedActorFragment::StaticStruct(),
		[Owner](void* FragmentMemory, const UScriptStruct& FragmentType)
		{
			auto& Fragment = *static_cast<FArcVisPrePlacedActorFragment*>(FragmentMemory);
			Fragment.PrePlacedActor = Owner;
		});

	// The init observer has already run and created an ISM instance or spawned a new actor.
	// Clean up whatever it created — the pre-placed actor lifecycle is managed by the cell system.
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
				ActorFragment->ResetAndUpdateHandleMap();
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

		// Determine representation based on cell activity
		if (VisSubsystem->IsActiveCellCoord(Rep->GridCoords))
		{
			// Cell is active — bind the pre-placed actor now
			Rep->bIsActorRepresentation = true;
			Rep->CurrentISMMesh = nullptr;

			FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(EntityHandle);
			if (ActorFragment)
			{
				ActorFragment->SetAndUpdateHandleMap(EntityHandle, GetOwner(), false);
			}
		}
		else
		{
			// Cell is inactive — hide the pre-placed actor, use ISM representation
			Rep->bIsActorRepresentation = false;
			Rep->CurrentISMMesh = nullptr;
			GetOwner()->SetActorHiddenInGame(true);
			GetOwner()->SetActorEnableCollision(false);

			const FArcVisConfigFragment* Config = EntityManager.GetConstSharedFragmentDataPtr<FArcVisConfigFragment>(EntityHandle);
			if (Config)
			{
				UStaticMesh* MeshToUse = Config->StaticMesh;
				if (MeshToUse)
				{
					Rep->ISMInstanceId = VisSubsystem->AddISMInstance(
						Rep->GridCoords,
						Config->ISMManagerClass,
						MeshToUse,
						Config->MaterialOverrides,
						Config->bCastShadows,
						GetOwner()->GetActorTransform(),
						EntityHandle);
				}
				Rep->CurrentISMMesh = MeshToUse;
			}
		}
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
