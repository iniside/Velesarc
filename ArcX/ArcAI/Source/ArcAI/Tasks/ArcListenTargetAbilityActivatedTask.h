#pragma once
#include "ArcMassStateTreeRunEnvQueryTask.h"

#include "ArcListenTargetAbilityActivatedTask.generated.h"

class AActor;

USTRUCT()
struct FArcListenTargetAbilityActivatedTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> InputTarget;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnAbilityActivated;

	FDelegateHandle OnAbilityActivatedHandle;
};

USTRUCT(DisplayName="Arc Listen Target Ability Activated", meta = (Category = "Arc|Events" ))
struct FArcListenTargetAbilityActivatedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcListenTargetAbilityActivatedTaskInstanceData;

	FArcListenTargetAbilityActivatedTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
};

USTRUCT()
struct FArcMassStateTreeEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;
};