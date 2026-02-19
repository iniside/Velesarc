#pragma once
#include "MassStateTreeTypes.h"

#include "ArcMassActorRotateToFaceTargetTask.generated.h"

class ArcMassActorRotateToFaceTargetTask
{
public:
	
};

USTRUCT()
struct FArcMassActorRotateToFaceTargetTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> InputActor;
	
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> Target;
};

USTRUCT(DisplayName="Arc Mass Actor Rotate To Face Target Task", meta = (Category = "Arc|Common"))
struct FArcMassActorRotateToFaceTargetTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassActorRotateToFaceTargetTaskInstanceData;

	FArcMassActorRotateToFaceTargetTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	//TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};
