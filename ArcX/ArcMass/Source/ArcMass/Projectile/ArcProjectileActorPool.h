// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcProjectileActorPool.generated.h"

UCLASS()
class ARCMASS_API UArcProjectileActorPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Acquire an actor from the pool or spawn a new one. */
	AActor* AcquireActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform);

	/** Release an actor back to the pool. Returns false if the actor cannot be pooled (caller should destroy). */
	bool ReleaseActor(AActor* Actor);

	/** Pre-spawn actors into the pool for later use. */
	void WarmPool(TSubclassOf<AActor> ActorClass, int32 Count);

protected:
	virtual void Deinitialize() override;

private:
	TMap<TSubclassOf<AActor>, TArray<TObjectPtr<AActor>>> PooledActors;
};
