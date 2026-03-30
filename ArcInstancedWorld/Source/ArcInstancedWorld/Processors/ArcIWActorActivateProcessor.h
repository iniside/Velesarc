// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassSignalProcessorBase.h"
#include "ArcIWActorActivateProcessor.generated.h"

// ---------------------------------------------------------------------------
// Actor Activate Processor
// Subscribes to ActorCellActivated -- removes ISM, spawns real actor.
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWActorActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcIWActorActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
