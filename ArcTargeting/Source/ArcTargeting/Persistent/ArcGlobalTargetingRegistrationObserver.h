#pragma once

#include "Mass/ObserverProcessor.h"
#include "ArcGlobalTargetingRegistrationObserver.generated.h"

UCLASS()
class ARCTARGETING_API UArcGlobalTargetingAddObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcGlobalTargetingAddObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

UCLASS()
class ARCTARGETING_API UArcGlobalTargetingRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcGlobalTargetingRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
