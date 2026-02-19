#pragma once
#include "MassEntityElementTypes.h"
#include "MassProcessor.h"
#include "MassStateTreeTypes.h"

#include "ArcMassWaitTask.generated.h"

USTRUCT()
struct FArcMassWaitTaskFragment : public FMassFragment
{
	GENERATED_BODY()

public:
	float Duration = 0;
	float Time = 0;
};

USTRUCT()
struct FArcMassWaitTaskTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FArcMassWaitTaskInstanceData
{
	GENERATED_BODY()

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;

	float Time = 0;
};

/**
 * Stop, and stand on current navmesh location
 */
USTRUCT(meta = (DisplayName = "Arc Mass Wait", Category = "Arc|Common"))
struct FArcMassWaitTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassWaitTaskInstanceData;

public:
	FArcMassWaitTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FArcMassWaitTaskFragment> MassWaitFragment;
};

class UMassSignalSubsystem;

UCLASS(MinimalAPI)
class UArcMassWaitTaskProcessor : public UMassProcessor
{
	GENERATED_BODY()

protected:
	UArcMassWaitTaskProcessor();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& ) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery_Conditional;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem = nullptr;
};