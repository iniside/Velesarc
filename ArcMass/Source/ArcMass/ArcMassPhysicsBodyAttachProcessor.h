// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcMassPhysicsBodyAttachProcessor.generated.h"

/**
 * Signal-driven processor that attaches entity handles to standalone FBodyInstance
 * objects via Chaos user data. Subscribes to PhysicsLinkRequested.
 * Handles FArcMassPhysicsBodyFragment (componentless bodies).
 *
 * If a body's Chaos actor is not yet valid (async creation pending),
 * re-signals the entity for retry on the next frame.
 */
UCLASS()
class ARCMASS_API UArcMassPhysicsBodyAttachProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMassPhysicsBodyAttachProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
