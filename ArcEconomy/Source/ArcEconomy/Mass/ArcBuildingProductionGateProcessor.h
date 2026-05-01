// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "ArcBuildingProductionGateProcessor.generated.h"

/**
 * Manages ArcCraft queue entry lifecycle based on staffed state.
 * - Staffed: ensure queue entry exists (re-add from DesiredRecipe if missing)
 * - Not staffed: remove queue entry, track halt duration
 * Desired recipe persists on FArcBuildingWorkforceFragment between removals.
 */
UCLASS()
class ARCECONOMY_API UArcBuildingProductionGateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcBuildingProductionGateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery BuildingQuery;
};
