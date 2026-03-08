// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"

#include "ArcProjectileVisualization.generated.h"

// ---------------------------------------------------------------------------
// Init Observer — spawns actor on FArcProjectileTag Add
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcProjectileActorInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcProjectileActorInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// Deinit Observer — destroys actor on FArcProjectileTag Remove
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcProjectileActorDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcProjectileActorDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
