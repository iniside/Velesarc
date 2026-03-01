// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "MassSignalProcessorBase.h"
#include "ArcMobileVisualizationProcessors.generated.h"

// ---------------------------------------------------------------------------
// 1. Source Cell Tracking — tracks LOD source movement, signals affected entities.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisSourceTrackingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMobileVisSourceTrackingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

// ---------------------------------------------------------------------------
// 2. Entity Cell Update — tracks entity movement, updates grid, signals cell changes.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisEntityCellUpdateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMobileVisEntityCellUpdateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

// ---------------------------------------------------------------------------
// 3. Entity Cell Changed — evaluates LOD after cell change, emits transition signals.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisEntityCellChangedProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMobileVisEntityCellChangedProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// 4. Upgrade to Actor — ISM/None -> Actor
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisUpgradeToActorProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMobileVisUpgradeToActorProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// 5. Downgrade to ISM — Actor -> ISM
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisDowngradeToISMProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMobileVisDowngradeToISMProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// 6. Upgrade to ISM — None -> ISM
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisUpgradeToISMProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMobileVisUpgradeToISMProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// 7. Downgrade to None — ISM -> None
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisDowngradeToNoneProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMobileVisDowngradeToNoneProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// 8. ISM Transform Update — batch-updates ISM transforms for moving entities in ISM mode.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisISMTransformUpdateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMobileVisISMTransformUpdateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

// ---------------------------------------------------------------------------
// 9. Entity Init Observer — FArcMobileVisEntityTag::Add
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisEntityInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMobileVisEntityInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// 10. Entity Deinit Observer — FArcMobileVisEntityTag::Remove
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisEntityDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMobileVisEntityDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// 11. Source Deinit Observer — FArcMobileVisSourceTag::Remove
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisSourceDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMobileVisSourceDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
