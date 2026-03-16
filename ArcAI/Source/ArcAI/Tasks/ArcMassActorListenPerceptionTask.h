#pragma once
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"
#include "StateTreeConsiderationBase.h"

#include "ArcMassActorListenPerceptionTask.generated.h"
class UArcAIPerceptionComponent;

USTRUCT()
struct FArcMassActorListenPerceptionTaskInstanceData
{
	GENERATED_BODY()

	/** Optional duration in seconds. If > 0, the task auto-completes after this time. If 0 or negative, runs indefinitely until a transition fires. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;

	/** Output: the first perceived actor detected by the perception system. Updated when perception changes. */
	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> Output;

	/** Internal: cached reference to the entity's perception component. */
	UPROPERTY()
	TWeakObjectPtr<UArcAIPerceptionComponent> PerceptionComponent;

	/** Delegate dispatcher that fires when the perception result changes. Use this to trigger state tree transitions. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;

	/** Internal handle for the perception changed delegate. */
	FDelegateHandle PerceptionChangedDelegate;
	/** Internal timer handle for the optional duration timeout. */
	FTimerHandle TimerHandle;

	/** Internal: current list of all perceived actors. */
	TArray<AActor*> PerceivedActors;
};

/**
 * Latent task that listens for perception changes on the entity's UArcAIPerceptionComponent.
 *
 * On EnterState, retrieves the entity's actor via FMassActorFragment, finds its ArcAIPerceptionComponent,
 * and subscribes to perception change events. Returns Running.
 * When perception changes (new actors detected/lost), outputs the first perceived actor
 * and fires OnResultChanged, signaling the entity.
 * Optionally auto-completes after Duration seconds.
 *
 * This is a latent event listener — it should be the primary or only task in its state,
 * and state transitions should be driven by the OnResultChanged delegate.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Perception", ToolTip = "Latent task that listens for perception changes on the entity's actor. Outputs the first perceived actor and fires OnResultChanged. Should be the primary task in its state."))
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

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;
};

USTRUCT()
struct FArcMassActorListenSightPerceptionTaskInstanceData
{
	GENERATED_BODY()

	/** Optional duration in seconds. If > 0, the task auto-completes after this time. If 0 or negative, runs indefinitely until a transition fires. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;

	/** Output: array of all actors currently perceived via sight sense. Updated when perception changes. */
	UPROPERTY(EditAnywhere, Category = Output)
	TArray<TObjectPtr<AActor>> Output;

	/** Internal: cached reference to the entity's perception component. */
	UPROPERTY()
	TWeakObjectPtr<UArcAIPerceptionComponent> PerceptionComponent;

	/** Delegate dispatcher that fires when sight perception results change. Use this to trigger state tree transitions. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;

	/** Internal handle for the perception changed delegate. */
	FDelegateHandle PerceptionChangedDelegate;
	/** Internal timer handle for the optional duration timeout. */
	FTimerHandle TimerHandle;
};

/**
 * Latent task that listens specifically for sight perception changes on the entity's UArcAIPerceptionComponent.
 *
 * Similar to FArcMassActorListenPerceptionTask, but outputs an array of all actors seen via sight sense
 * rather than just the first perceived actor.
 *
 * This is a latent event listener — it should be the primary or only task in its state,
 * and state transitions should be driven by the OnResultChanged delegate.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Sight Perception", ToolTip = "Latent task that listens for sight perception changes. Outputs array of all seen actors. Should be the primary task in its state."))
struct FArcMassActorListenSightPerceptionTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorListenSightPerceptionTaskInstanceData;

public:
	FArcMassActorListenSightPerceptionTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;
};

USTRUCT()
struct FUtilityTargetSelectionTaskInstanceData
{
    GENERATED_BODY()
    
    /** List of potential target actors to evaluate */
    UPROPERTY(EditAnywhere, Category = "Input")
    TArray<TObjectPtr<AActor>> TargetActors;
    
    /** List of action state handles to evaluate */
    UPROPERTY(EditAnywhere, Category = "Parameter")
    TArray<FStateTreeStateLink> ActionStates;
    
    /** Current target being evaluated - considerations bind to this!  */
    UPROPERTY(EditAnywhere, Category = "Output")
    TObjectPtr<AActor> CurrentTargetActor = nullptr;
    
    /** Output:  Selected target */
    UPROPERTY(EditAnywhere, Category = "Output")
    TObjectPtr<AActor> SelectedTarget = nullptr;
    
    /** Output: Selected action state */
    UPROPERTY(EditAnywhere, Category = "Output")
    FStateTreeStateHandle SelectedActionState;
    
    /** Output: Best utility score achieved */
    UPROPERTY(EditAnywhere, Category = "Output")
    float BestUtilityScore = 0.f;
    
    /** Minimum score threshold */
    UPROPERTY(EditAnywhere, Category = "Parameters")
    float MinimumScoreThreshold = 0.01f;
};