// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassSignalProcessorBase.h"
#include "ArcIWActivateProcessor.generated.h"

// ---------------------------------------------------------------------------
// Activate Processor
// Subscribes to CellActivated signal -- adds composite ISM instances.
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcIWActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
