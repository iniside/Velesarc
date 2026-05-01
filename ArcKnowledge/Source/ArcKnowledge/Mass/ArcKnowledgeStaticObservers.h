// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcKnowledgeStaticObservers.generated.h"

/**
 * Observer that fires when FArcKnowledgeStaticTag is added to an entity.
 * Inserts the entity into the subsystem's static knowledge R-tree.
 */
UCLASS()
class ARCKNOWLEDGE_API UArcKnowledgeStaticAddObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcKnowledgeStaticAddObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

/**
 * Observer that fires when FArcKnowledgeStaticTag is removed from an entity.
 * Removes the entity from the subsystem's static knowledge R-tree.
 */
UCLASS()
class ARCKNOWLEDGE_API UArcKnowledgeStaticRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcKnowledgeStaticRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
