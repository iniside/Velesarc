// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ArcNavInvokerCellDetectProcessor.generated.h"

/**
 * Parallel processor that detects nav invoker entities crossing coarse cell boundaries.
 * Computes tile diffs (gains/losses) using pure math on a worker thread, pushes
 * pre-computed results to UArcMassNavInvokerSubsystem, and signals dirty entities
 * for the game-thread apply processor.
 */
UCLASS()
class ARCMASS_API UArcNavInvokerCellDetectProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcNavInvokerCellDetectProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
