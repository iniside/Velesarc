// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ObserverProcessor.h"
#include "ArcMassItemAttachmentObserver.generated.h"

/**
 * UArcMassItemAttachmentTagRemoveObserver
 *
 * Observes FArcMassItemAttachmentTag Remove (entity destruction / tag removal).
 * On removal: tears down all active attachment state via
 * UArcMassItemAttachmentSubsystem::TeardownEntity and evicts the entity from
 * UArcMassItemEventSubsystem::EvictEntity.
 *
 * Mirrors UArcVisEntityDeinitObserver registration idiom.
 * bRequiresGameThreadExecution = true — spawned UObjects must be destroyed on GT.
 * Does NOT set bAutoRegisterWithProcessingPhases or ExecutionFlags.
 */
UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassItemAttachmentTagRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassItemAttachmentTagRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery{*this};
};
