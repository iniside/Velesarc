// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcLinkableGuidObservers.generated.h"

/** Observer that registers entities with UArcLinkableGuidSubsystem when FArcLinkableGuidFragment is added. */
UCLASS()
class ARCMASS_API UArcLinkableGuidInitObserver : public UMassObserverProcessor
{
    GENERATED_BODY()

public:
    UArcLinkableGuidInitObserver();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery ObserverQuery;
};

/** Observer that unregisters entities from UArcLinkableGuidSubsystem when they are destroyed. */
UCLASS()
class ARCMASS_API UArcLinkableGuidDeinitObserver : public UMassObserverProcessor
{
    GENERATED_BODY()

public:
    UArcLinkableGuidDeinitObserver();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery ObserverQuery;
};
