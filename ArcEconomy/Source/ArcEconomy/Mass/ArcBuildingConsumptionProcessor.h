// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "ArcBuildingConsumptionProcessor.generated.h"

/**
 * Scans consumer buildings for stock deficits and posts demand knowledge entries.
 * Increments demand counters on owning settlement's market fragment.
 * Runs every ~5 seconds.
 */
UCLASS()
class ARCECONOMY_API UArcBuildingConsumptionProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcBuildingConsumptionProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ConsumerQuery;
	float TimeSinceLastTick = 0.0f;
	float TickInterval = 5.0f;
};
