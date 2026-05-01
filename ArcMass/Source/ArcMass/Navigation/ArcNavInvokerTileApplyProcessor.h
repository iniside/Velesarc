// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcNavInvokerTileApplyProcessor.generated.h"

/**
 * Signal-based processor that consumes pre-computed tile diffs from the parallel
 * cell-detect processor and applies them to the navmesh in a single batched pass.
 * Only fires when entities have crossed cell boundaries (zero cost on quiet frames).
 */
UCLASS()
class ARCMASS_API UArcNavInvokerTileApplyProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcNavInvokerTileApplyProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
