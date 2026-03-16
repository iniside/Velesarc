// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "ArcMassPersistenceProcessors.generated.h"

/**
 * Tracks source entity positions and updates the persistence subsystem's
 * active cell set. Runs every tick after physics.
 *
 * Source entities are identified by FArcMassPersistenceSourceTag.
 * Game code should add this tag to player entity configs.
 */
UCLASS()
class ARCMASS_API UArcMassPersistenceSourceTrackingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassPersistenceSourceTrackingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager,
		FMassExecutionContext& Context) override;

private:
	FMassEntityQuery SourceQuery;
};

/**
 * Tracks persistent entity positions and updates their CurrentCell.
 * When an entity moves to a new cell, notifies the subsystem.
 */
UCLASS()
class ARCMASS_API UArcMassPersistenceCellTrackingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassPersistenceCellTrackingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager,
		FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

/**
 * Observer: initializes persistence fragment when tag is added.
 * Sets PersistenceGuid if not already set (new entity vs loaded).
 */
UCLASS()
class ARCMASS_API UArcMassPersistenceInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassPersistenceInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager,
		FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
