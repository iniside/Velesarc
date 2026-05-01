// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "ArcBuildingSupplyProcessor.generated.h"

/**
 * Posts ResourceSupply knowledge entries for buildings with output items.
 * Increments supply counters on owning settlement's market fragment.
 * Runs every ~5 seconds.
 */
UCLASS()
class ARCECONOMY_API UArcBuildingSupplyProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcBuildingSupplyProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery BuildingQuery;
	float TimeSinceLastTick = 0.0f;
	float TickInterval = 5.0f;
};
