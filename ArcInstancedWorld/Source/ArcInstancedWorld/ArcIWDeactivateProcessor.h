// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassSignalProcessorBase.h"
#include "ArcIWDeactivateProcessor.generated.h"

// ---------------------------------------------------------------------------
// Deactivate Processor
// Subscribes to CellDeactivated signal -- removes composite ISM instances.
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWDeactivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcIWDeactivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
