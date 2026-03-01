// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcAreaAssignmentObserver.generated.h"

/**
 * Observer that fires when FArcAreaAssignmentTag is removed from an NPC entity.
 * Cleans up the entity's area assignment in the subsystem (handles "NPC dies while assigned").
 */
UCLASS()
class ARCAREA_API UArcAreaAssignmentRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcAreaAssignmentRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
