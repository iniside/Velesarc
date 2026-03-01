// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcAreaObservers.generated.h"

/**
 * Observer that fires when FArcAreaTag is added to a SmartObject entity.
 * Registers the area in UArcAreaSubsystem using its definition and location.
 */
UCLASS()
class ARCAREA_API UArcAreaAddObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcAreaAddObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

/**
 * Observer that fires when FArcAreaTag is removed from a SmartObject entity.
 * Unregisters the area from UArcAreaSubsystem.
 */
UCLASS()
class ARCAREA_API UArcAreaRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcAreaRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
