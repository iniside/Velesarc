// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"

#include "ArcConditionTickProcessors.generated.h"

// ---------------------------------------------------------------------------
// Per-condition tick processors.
//
// Each condition has its own tick processor handling decay, overload timers,
// burnout, and hysteresis. Identical logic, different fragment types.
// ---------------------------------------------------------------------------

#define ARC_DECLARE_CONDITION_TICK_PROCESSOR(Name) \
	UCLASS() \
	class ARCCONDITIONEFFECTS_API UArc##Name##ConditionTickProcessor : public UMassProcessor \
	{ \
		GENERATED_BODY() \
	public: \
		UArc##Name##ConditionTickProcessor(); \
	protected: \
		virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override; \
		virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override; \
	private: \
		FMassEntityQuery EntityQuery; \
	};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcBurningConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcBurningConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcBleedingConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcBleedingConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcChilledConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcChilledConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcShockedConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcShockedConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcPoisonedConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcPoisonedConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcDiseasedConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcDiseasedConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcWeakenedConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcWeakenedConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcOiledConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcOiledConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcWetConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcWetConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcCorrodedConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcCorrodedConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcBlindedConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcBlindedConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcSuffocatingConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcSuffocatingConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS()
class ARCCONDITIONEFFECTS_API UArcExhaustedConditionTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcExhaustedConditionTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

#undef ARC_DECLARE_CONDITION_TICK_PROCESSOR
