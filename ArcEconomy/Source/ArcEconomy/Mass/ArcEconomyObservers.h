// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassEntityQuery.h"
#include "ArcEconomyObservers.generated.h"

/** Registers buildings in UArcSettlementSubsystem on FArcBuildingFragment Add. */
UCLASS()
class ARCECONOMY_API UArcBuildingAddObserver : public UMassObserverProcessor
{
	GENERATED_BODY()
public:
	UArcBuildingAddObserver();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ObserverQuery;
};

/** Unregisters buildings from UArcSettlementSubsystem and cleans up knowledge entries on FArcBuildingFragment Remove. */
UCLASS()
class ARCECONOMY_API UArcBuildingRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()
public:
	UArcBuildingRemoveObserver();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ObserverQuery;
};

/** Registers NPCs in UArcSettlementSubsystem on FArcEconomyNPCFragment Add. */
UCLASS()
class ARCECONOMY_API UArcNPCAddObserver : public UMassObserverProcessor
{
	GENERATED_BODY()
public:
	UArcNPCAddObserver();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ObserverQuery;
};

/** Unregisters NPCs from UArcSettlementSubsystem on FArcEconomyNPCFragment Remove. */
UCLASS()
class ARCECONOMY_API UArcNPCRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()
public:
	UArcNPCRemoveObserver();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ObserverQuery;
};
