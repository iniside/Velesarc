// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcKnowledgeObservers.generated.h"

/**
 * Observer that fires when FArcKnowledgeMemberTag is added to an entity.
 * Registers the entity's capabilities as knowledge entries in its settlement.
 */
UCLASS()
class ARCKNOWLEDGE_API UArcKnowledgeMemberAddObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcKnowledgeMemberAddObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

/**
 * Observer that fires when FArcKnowledgeMemberTag is removed from an entity.
 * Removes the entity's registered knowledge entries from its settlement.
 */
UCLASS()
class ARCKNOWLEDGE_API UArcKnowledgeMemberRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcKnowledgeMemberRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
