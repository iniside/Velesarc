#pragma once
#include "MassStateTreeTypes.h"

#include "ArcMassActorRotateToFaceTargetTask.generated.h"

class ArcMassActorRotateToFaceTargetTask
{
public:
	
};

/** Instance data for FArcMassActorRotateToFaceTargetTask. Holds the actor to rotate and the target to face. */
USTRUCT()
struct FArcMassActorRotateToFaceTargetTaskInstanceData
{
	GENERATED_BODY()

	/** The actor whose rotation will be set. Typically the entity's own actor. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> InputActor;

	/** The target actor to face. The InputActor will be rotated to look toward this actor's location. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> Target;
};

/**
 * Instant StateTree task that rotates the entity's actor to face a target actor.
 * EnterState sets the rotation immediately and returns Succeeded.
 * Can be combined with other tasks in the same state.
 */
USTRUCT(DisplayName="Arc Mass Actor Rotate To Face Target Task", meta = (Category = "Arc|Common", ToolTip = "Instant task that rotates the entity's actor to face a target actor and immediately succeeds."))
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

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	//TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};
