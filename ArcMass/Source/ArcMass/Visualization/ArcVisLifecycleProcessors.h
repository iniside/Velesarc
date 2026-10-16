// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcVisLifecycleProcessors.generated.h"

// Vis Switch Processor — bridges canonical lifecycle phase changes to visual
// changes. For mesh entities: swaps ISM instance to the holder that matches
// the phase-override mesh. Subscribes to LifecyclePhaseChanged.

UCLASS()
class ARCMASS_API UArcVisLifecycleVisSwitchProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcVisLifecycleVisSwitchProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
