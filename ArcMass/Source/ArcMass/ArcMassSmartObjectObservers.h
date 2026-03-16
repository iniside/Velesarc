// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcMassSmartObjectObservers.generated.h"

/**
 * Observer that fires when FArcSmartObjectTag is added to an entity.
 * Creates a SmartObject using the entity's FArcSmartObjectDefinitionSharedFragment
 * and stores the handle in FArcSmartObjectOwnerFragment.
 */
UCLASS()
class ARCMASS_API UArcSmartObjectAddObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcSmartObjectAddObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

/**
 * Observer that fires when FArcSmartObjectTag is removed from an entity.
 * Destroys the SmartObject referenced by FArcSmartObjectOwnerFragment.
 */
UCLASS()
class ARCMASS_API UArcSmartObjectRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcSmartObjectRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
