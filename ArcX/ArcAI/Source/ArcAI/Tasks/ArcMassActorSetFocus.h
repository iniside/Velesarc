#pragma once
#include "ArcMassStateTreeRunEnvQueryTask.h"

#include "ArcMassActorSetFocus.generated.h"

class AAIController;
class AActor;

USTRUCT()
struct FArcMassActorSetFocusTaskInstanceData
{
	GENERATED_BODY()

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AAIController> AIController;

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> FocusTarget;
};

USTRUCT(meta = (DisplayName = "Arc Mass Actor Set Focus", Category = "Arc|Common"))
struct FArcMassActorSetFocusTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorSetFocusTaskInstanceData;

public:
	FArcMassActorSetFocusTask();
	
protected:
	//virtual bool Link(FStateTreeLinker& Linker) override;
	//virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
