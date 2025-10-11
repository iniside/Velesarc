#pragma once
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "MassActorSubsystem.h"

#include "ArcMassActorListenPerceptionTask.generated.h"
class UArcAIPerceptionComponent;

USTRUCT()
struct FArcMassActorListenPerceptionTaskInstanceData
{
	GENERATED_BODY()

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> Output;
	
	UPROPERTY()
	TWeakObjectPtr<UArcAIPerceptionComponent> PerceptionComponent;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;

	FDelegateHandle PerceptionChangedDelegate;
	FTimerHandle TimerHandle;

	TArray<AActor*> PerceivedActors;
};

USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Perception"))
struct FArcMassActorListenPerceptionTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorListenPerceptionTaskInstanceData;

public:
	FArcMassActorListenPerceptionTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;
};
