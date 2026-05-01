// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassSignalProcessorBase.h"
#include "ArcMassSkinnedMeshVisualizationProcessors.generated.h"

UCLASS()
class ARCMASS_API UArcVisSkinnedMeshEntityInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcVisSkinnedMeshEntityInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

UCLASS()
class ARCMASS_API UArcVisSkinnedMeshActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcVisSkinnedMeshActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

UCLASS()
class ARCMASS_API UArcVisSkinnedMeshDeactivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcVisSkinnedMeshDeactivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

UCLASS()
class ARCMASS_API UArcVisSkinnedMeshEntityDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcVisSkinnedMeshEntityDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
