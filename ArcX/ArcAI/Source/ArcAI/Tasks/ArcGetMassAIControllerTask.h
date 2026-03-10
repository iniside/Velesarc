#pragma once
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"
#include "StateTreePropertyRef.h"

#include "ArcGetMassAIControllerTask.generated.h"

class AAIController;

/** Instance data for FArcGetMassAIControllerTask. Holds the output AIController reference. */
USTRUCT()
struct FArcGetMassAIControllerTaskInstanceData
{
	GENERATED_BODY()

	/** The retrieved AAIController. Bind this output to provide the controller to other tasks in the state tree. */
	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AAIController> Output;

	/** Property ref output for the AAIController. Alternative to Output for property ref-based binding. */
	UPROPERTY(EditAnywhere, Category = Out, meta = (RefType = "/Script/AIModule.AIController"))
	FStateTreePropertyRef Result;
};

/**
 * Instant StateTree task that retrieves the AAIController from the entity's actor (via FMassActorFragment).
 * EnterState looks up the actor's AIController and writes it to the Output/Result, then immediately
 * returns Succeeded. Use this to provide an AIController binding for downstream tasks that need one
 * (e.g., MoveTo, SetFocus). Can be combined with other tasks in the same state.
 */
USTRUCT(DisplayName="Get Mass AI Controller", meta = (Category = "Arc|Common", ToolTip = "Instant task that retrieves the AAIController from the entity's actor and outputs it."))
struct FArcGetMassAIControllerTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGetMassAIControllerTaskInstanceData;

	FArcGetMassAIControllerTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	/** Handle to the entity's actor fragment, used to retrieve the AActor and find its AIController. */
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};