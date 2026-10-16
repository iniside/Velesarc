// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ObserverProcessor.h"
#include "Mass/Processor.h"

#include "ArcBodyProcessor.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcBodyProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcBodyProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCWORLDSIM_API UArcBodyCommandCleanupObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcBodyCommandCleanupObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
