// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassSignalProcessorBase.h"
#include "ArcIWActorDeactivateProcessor.generated.h"

// ---------------------------------------------------------------------------
// Actor Deactivate Processor
// Subscribes to ActorCellDeactivated -- destroys actor, re-adds ISM.
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWActorDeactivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcIWActorDeactivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
