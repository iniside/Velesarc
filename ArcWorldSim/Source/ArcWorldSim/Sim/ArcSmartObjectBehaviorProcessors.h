// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/Processor.h"

#include "ArcSmartObjectBehaviorProcessors.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcSmartObjectWorkBehaviorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcSmartObjectWorkBehaviorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCWORLDSIM_API UArcSmartObjectPickupBehaviorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcSmartObjectPickupBehaviorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCWORLDSIM_API UArcSmartObjectDropBehaviorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcSmartObjectDropBehaviorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCWORLDSIM_API UArcSmartObjectGatherBehaviorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcSmartObjectGatherBehaviorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCWORLDSIM_API UArcSmartObjectDrawBehaviorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcSmartObjectDrawBehaviorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCWORLDSIM_API UArcSmartObjectEatBehaviorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcSmartObjectEatBehaviorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCWORLDSIM_API UArcSmartObjectSleepBehaviorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcSmartObjectSleepBehaviorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
