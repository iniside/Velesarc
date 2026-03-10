#pragma once
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"
#include "StateTreePropertyRef.h"

#include "ArcGetMassActorTask.generated.h"

/** Instance data for FArcGetMassActorTask. Holds the resolved actor output. */
USTRUCT()
struct FArcGetMassActorTaskInstanceData
{
	GENERATED_BODY()

	/** The actor retrieved from the entity's FMassActorFragment. Available after EnterState succeeds. */
	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> Output;

	/** Property reference that receives the resolved actor. Must be bound to an AActor* property in the StateTree. */
	UPROPERTY(EditAnywhere, Category = Out, meta = (RefType = "/Script/Engine.Actor"))
	FStateTreePropertyRef Result;
};

/**
 * Instant task that retrieves the AActor associated with the current Mass entity
 * from its FMassActorFragment and outputs it. Succeeds immediately if the actor
 * is valid, fails otherwise.
 */
USTRUCT(DisplayName="Get Mass Actor", meta = (Category = "Arc|Common", ToolTip = "Instant task. Retrieves the AActor from the entity's FMassActorFragment and outputs it."))
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

	/** Handle to the entity's actor fragment, used to resolve the AActor pointer. */
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};