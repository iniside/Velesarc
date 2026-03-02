#pragma once
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"
#include "StateTreePropertyRef.h"

#include "ArcGetMassActorTask.generated.h"

USTRUCT()
struct FArcGetMassActorTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> Output;

	UPROPERTY(EditAnywhere, Category = Out, meta = (RefType = "/Script/Engine.Actor"))
	FStateTreePropertyRef Result;
};

USTRUCT(DisplayName="Get Mass Actor", meta = (Category = "Arc|Common"))
struct FArcGetMassActorTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

public:
	
	using FInstanceDataType = FArcGetMassActorTaskInstanceData;

	FArcGetMassActorTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};