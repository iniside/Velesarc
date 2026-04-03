// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcIWActorPoolSubsystem.generated.h"

class UArcIWEntityComponent;

/**
 * Actor pool for ArcInstancedWorld hydrated actors.
 * Actors are reused across entity activations to avoid spawn/destroy overhead.
 * Each pooled actor is guaranteed to have a UArcIWEntityComponent.
 */
UCLASS()
class ARCINSTANCEDWORLD_API UArcIWActorPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Acquire an actor from the pool or spawn a new one.
	 * The returned actor will have a UArcIWEntityComponent with the entity handle set.
	 *
	 * @param ActorClass  Class to spawn/pool.
	 * @param Transform   World transform for the actor.
	 * @param EntityHandle  Mass entity this actor represents.
	 * @return The acquired actor, or nullptr on failure.
	 */
	AActor* AcquireActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform, FMassEntityHandle EntityHandle);

	/**
	 * Release an actor back to the pool.
	 * Clears the entity link, hides the actor, and disables collision/ticking.
	 *
	 * @param Actor  The actor to release.
	 */
	void ReleaseActor(AActor* Actor);

	/**
	 * Pre-warm the pool by spawning actors ahead of time.
	 *
	 * @param ActorClass  Class to pre-spawn.
	 * @param Count  Number of actors to add to the pool.
	 */
	void WarmPool(TSubclassOf<AActor> ActorClass, int32 Count);

private:
	/** Pooled actors grouped by class. */
	TMap<TSubclassOf<AActor>, TArray<TObjectPtr<AActor>>> PooledActors;

	/** Ensure the actor has a UArcIWEntityComponent. Returns the component. */
	UArcIWEntityComponent* EnsureEntityComponent(AActor* Actor);

	/** Prepare an actor for reuse: unhide, enable collision/ticking, set transform. */
	void ActivateActor(AActor* Actor, const FTransform& Transform);

	/** Deactivate an actor for pooling: hide, disable collision/ticking. */
	void DeactivateActor(AActor* Actor);
};
