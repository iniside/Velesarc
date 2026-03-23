// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWActorPoolSubsystem.h"
#include "ArcIWEntityComponent.h"
#include "Engine/World.h"

AActor* UArcIWActorPoolSubsystem::AcquireActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform, FMassEntityHandle EntityHandle)
{
	if (!ActorClass)
	{
		return nullptr;
	}

	AActor* Actor = nullptr;

	// Try to reuse a pooled actor
	TArray<TObjectPtr<AActor>>* Pool = PooledActors.Find(ActorClass);
	if (Pool && Pool->Num() > 0)
	{
		Actor = Pool->Pop();
		ActivateActor(Actor, Transform);
	}
	else
	{
		// Spawn new actor
		UWorld* World = GetWorld();
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Actor = World->SpawnActor<AActor>(ActorClass, Transform, SpawnParams);
	}

	if (!Actor)
	{
		return nullptr;
	}

	// Ensure entity component exists and set the handle
	UArcIWEntityComponent* EntityComp = EnsureEntityComponent(Actor);
	EntityComp->SetEntityHandle(EntityHandle);

	return Actor;
}

void UArcIWActorPoolSubsystem::ReleaseActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	// Clear entity link
	UArcIWEntityComponent* EntityComp = Actor->FindComponentByClass<UArcIWEntityComponent>();
	if (EntityComp)
	{
		EntityComp->ClearEntityHandle();
	}

	DeactivateActor(Actor);

	TSubclassOf<AActor> ActorClass = Actor->GetClass();
	PooledActors.FindOrAdd(ActorClass).Add(Actor);
}

void UArcIWActorPoolSubsystem::WarmPool(TSubclassOf<AActor> ActorClass, int32 Count)
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
	Pool.Reserve(Pool.Num() + Count);

	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(ActorClass, FTransform::Identity, SpawnParams);
		if (Actor)
		{
			EnsureEntityComponent(Actor);
			DeactivateActor(Actor);
			Pool.Add(Actor);
		}
	}
}

UArcIWEntityComponent* UArcIWActorPoolSubsystem::EnsureEntityComponent(AActor* Actor)
{
	UArcIWEntityComponent* Comp = Actor->FindComponentByClass<UArcIWEntityComponent>();
	if (!Comp)
	{
		Comp = NewObject<UArcIWEntityComponent>(Actor);
		Comp->RegisterComponent();
		Actor->AddInstanceComponent(Comp);
	}
	return Comp;
}

void UArcIWActorPoolSubsystem::ActivateActor(AActor* Actor, const FTransform& Transform)
{
	Actor->SetActorTransform(Transform);
	Actor->SetActorHiddenInGame(false);
	Actor->SetActorEnableCollision(true);
	Actor->SetActorTickEnabled(true);
}

void UArcIWActorPoolSubsystem::DeactivateActor(AActor* Actor)
{
	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);
}
