// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "ArcBuildingDemandProcessor.generated.h"

/**
 * Posts/updates ResourceDemand knowledge entries for halted building slots.
 * Increments demand counters on owning settlement's market fragment.
 * Runs every ~5 seconds.
 */
UCLASS()
class ARCECONOMY_API UArcBuildingDemandProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcBuildingDemandProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery BuildingQuery;
	float TimeSinceLastTick = 0.0f;
	float TickInterval = 5.0f;
};
