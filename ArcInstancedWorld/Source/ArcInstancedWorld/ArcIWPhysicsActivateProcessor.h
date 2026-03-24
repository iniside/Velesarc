// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassSignalProcessorBase.h"
#include "ArcIWPhysicsActivateProcessor.generated.h"

// ---------------------------------------------------------------------------
// Physics Activate Processor
// Subscribes to PhysicsCellActivated signal -- creates physics bodies for
// entities that already have ISM mesh instances (MeshOnly state required).
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWPhysicsActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcIWPhysicsActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
