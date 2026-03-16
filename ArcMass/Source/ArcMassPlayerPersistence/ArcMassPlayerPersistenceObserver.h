// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassObserverProcessor.h"
#include "ArcMassPlayerPersistenceObserver.generated.h"

/**
 * Observes FArcMassPlayerPersistenceTag addition on player entities.
 * Reads cached fragment data from UArcPlayerPersistenceSubsystem
 * and applies it via FArcMassFragmentSerializer::LoadEntityFragments.
 */
UCLASS()
class ARCMASSPLAYERPERSISTENCE_API UArcMassPlayerPersistenceObserver
	: public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassPlayerPersistenceObserver();

protected:
	virtual void ConfigureQueries(
		const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
