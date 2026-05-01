// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "ArcSettlementDiscoveryProcessor.generated.h"

/**
 * Periodic processor that discovers settlements for orphan buildings and NPCs
 * whose SettlementHandle is invalid (e.g. spawned before their settlement).
 * Runs every ~2 seconds in PrePhysics, before UArcGovernorProcessor.
 */
UCLASS()
class ARCECONOMY_API UArcSettlementDiscoveryProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcSettlementDiscoveryProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery OrphanBuildingQuery;
	FMassEntityQuery OrphanNPCQuery;

	/** Time accumulator for throttling. */
	float TimeSinceLastScan = 0.0f;

	/** Interval between scans in seconds. */
	UPROPERTY(EditAnywhere, Category = "Config")
	float ScanInterval = 2.0f;
};
