#pragma once
#include "ArcMassStateTreeRunEnvQueryTask.h"

#include "ArcMassActorSetFocus.generated.h"

class AAIController;
class AActor;

/** Instance data for FArcMassActorSetFocusTask. Holds the AIController and focus target actor. */
USTRUCT()
struct FArcMassActorSetFocusTaskInstanceData
{
	GENERATED_BODY()

	/** The AIController whose focus will be set. Must be bound from the entity's actor. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AAIController> AIController;

	/** The actor to focus on. The AIController will continuously look at this actor while the state is active. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> FocusTarget;
};

/**
 * Latent StateTree task that sets AI focus on a target actor for the duration of the state.
 * EnterState sets the initial focus and returns Running. Tick continuously updates the focus
 * to keep tracking the target. ExitState clears the focus. This task runs for the entire
 * lifetime of its state and should be used as the primary task or alongside movement tasks.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Actor Set Focus", Category = "Arc|Common", ToolTip = "Latent task that sets and maintains AI focus on a target actor. Clears focus on state exit."))
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

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
