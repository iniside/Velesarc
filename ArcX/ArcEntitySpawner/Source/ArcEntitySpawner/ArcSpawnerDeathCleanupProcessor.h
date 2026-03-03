// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcSpawnerDeathCleanupProcessor.generated.h"

/**
 * Signal processor that listens for LifecycleDead on spawner-owned entities.
 * Removes dead entities from spawner tracking and persistence.
 */
UCLASS()
class ARCENTITYSPAWNER_API UArcSpawnerDeathCleanupProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcSpawnerDeathCleanupProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner,
		const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(
		const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		FMassSignalNameLookup& EntitySignals) override;
};
