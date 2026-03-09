// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcProjectileActorPool.h"

#include "MassActorPoolableInterface.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcProjectileActorPool)

AActor* UArcProjectileActorPoolSubsystem::AcquireActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform)
{
	if (!ActorClass)
	{
		return nullptr;
	}

	// Try to retrieve from pool
	if (TArray<TObjectPtr<AActor>>* Pool = PooledActors.Find(ActorClass))
	{
		while (Pool->Num() > 0)
		{
			AActor* PooledActor = Pool->Pop(EAllowShrinking::No);
			if (IsValid(PooledActor))
			{
				PooledActor->SetActorHiddenInGame(false);
				PooledActor->SetActorEnableCollision(false);
				PooledActor->SetActorTickEnabled(false);
				PooledActor->SetActorTransform(Transform, false, nullptr, ETeleportType::ResetPhysics);

				if (PooledActor->Implements<UMassActorPoolableInterface>())
				{
					IMassActorPoolableInterface::Execute_PrepareForGame(PooledActor);
				}

				return PooledActor;
			}
		}
	}

	// Pool empty — spawn new
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return World->SpawnActor<AActor>(ActorClass, Transform, SpawnParams);
}

bool UArcProjectileActorPoolSubsystem::ReleaseActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	if (Actor->Implements<UMassActorPoolableInterface>())
	{
		if (!IMassActorPoolableInterface::Execute_CanBePooled(Actor))
		{
			return false;
		}

		IMassActorPoolableInterface::Execute_PrepareForPooling(Actor);
	}

	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);

	TArray<TObjectPtr<AActor>>& Pool = PooledActors.FindOrAdd(Actor->GetClass());
	Pool.Add(Actor);

	return true;
}

void UArcProjectileActorPoolSubsystem::WarmPool(TSubclassOf<AActor> ActorClass, int32 Count)
{
	if (!ActorClass || Count <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<TObjectPtr<AActor>>& Pool = PooledActors.FindOrAdd(ActorClass);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < Count; ++i)
	{
		AActor* Actor = World->SpawnActor<AActor>(ActorClass, FTransform::Identity, SpawnParams);
		if (Actor)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			Actor->SetActorTickEnabled(false);

			if (Actor->Implements<UMassActorPoolableInterface>())
			{
				IMassActorPoolableInterface::Execute_PrepareForPooling(Actor);
			}

			Pool.Add(Actor);
		}
	}
}

void UArcProjectileActorPoolSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();

	for (auto& [ActorClass, Pool] : PooledActors)
	{
		for (AActor* Actor : Pool)
		{
			if (IsValid(Actor) && World)
			{
				World->DestroyActor(Actor);
			}
		}
	}

	PooledActors.Empty();

	Super::Deinitialize();
}
