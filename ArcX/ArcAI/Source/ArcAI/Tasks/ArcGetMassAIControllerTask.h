#pragma once
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"
#include "StateTreePropertyRef.h"

#include "ArcGetMassAIControllerTask.generated.h"

class AAIController;

USTRUCT()
struct FArcGetMassAIControllerTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AAIController> Output;

	UPROPERTY(EditAnywhere, Category = Out, meta = (RefType = "/Script/AIModule.AIController"))
	FStateTreePropertyRef Result;
};

USTRUCT(DisplayName="Get Mass AI Controller")
struct FArcGetMassAIControllerTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGetMassAIControllerTaskInstanceData;

	FArcGetMassAIControllerTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};