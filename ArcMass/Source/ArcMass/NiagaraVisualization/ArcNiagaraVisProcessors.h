// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "ArcNiagaraVisProcessors.generated.h"

// ---------------------------------------------------------------------------
// Init observer — creates controller + render state on tag add
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcNiagaraVisInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcNiagaraVisInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// Deinit observer — destroys render state + controller on tag remove
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcNiagaraVisDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcNiagaraVisDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// Per-frame processor — pushes dynamic render data to proxy
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcNiagaraVisDynamicDataProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcNiagaraVisDynamicDataProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
