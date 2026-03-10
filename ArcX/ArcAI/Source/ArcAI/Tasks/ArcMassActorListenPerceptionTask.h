#pragma once
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "MassActorSubsystem.h"
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

/**
 * Instant task that evaluates utility scores for all (Target, Action) combinations to select the best pairing.
 *
 * Iterates over each TargetActor and each ActionState, temporarily setting CurrentTargetActor so that
 * considerations in the action states can bind to it and score the target.
 * Outputs the SelectedTarget, SelectedActionState, and BestUtilityScore.
 * Fails if no combination meets the MinimumScoreThreshold.
 *
 * This is an instant task — it completes in EnterState with Succeeded or Failed.
 */
USTRUCT(meta = (DisplayName = "Utility Target Selection", ToolTip = "Instant task that evaluates utility scores for all (Target, Action) combinations. Outputs the best-scoring target and action state. Fails if no score meets the minimum threshold."))
struct FUtilityTargetSelectionTask :  public FMassStateTreeTaskBase
{
    GENERATED_BODY()
    
    using FInstanceDataType = FUtilityTargetSelectionTaskInstanceData;
    
    virtual const UStruct* GetInstanceDataType() const override 
    { 
        return FInstanceDataType::StaticStruct(); 
    }
    
    virtual EStateTreeRunStatus EnterState(
        FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

protected:
    float EvaluateStateUtilityForTarget(
        FStateTreeExecutionContext& Context,
        FInstanceDataType& InstanceData,
        AActor* TargetActor,
        FStateTreeStateHandle ActionStateHandle) const;
};

USTRUCT()
struct FTargetDistanceConsiderationInstanceData
{
	GENERATED_BODY()

	/** World location of the target being evaluated. Typically bound from the CurrentTargetActor's location. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FVector TargetLocation;

	/** World location of the owner entity. Used as the reference point for distance calculation. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FVector OwnerLocation;

	/** Maximum distance for normalization. At MaxDistance the score is 0; at distance 0 the score is 1. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MaxDistance;
};

/**
 * Consideration that scores targets based on proximity. Returns a normalized score from 0 to 1
 * where closer targets score higher (1 at distance 0, 0 at MaxDistance).
 * Typically used with FUtilityTargetSelectionTask to prefer nearer targets.
 */
USTRUCT(meta = (DisplayName = "Target Distance Consideration", ToolTip = "Consideration that scores targets by proximity. Returns 1.0 at distance 0, 0.0 at MaxDistance."))
struct FTargetDistanceConsideration : public FStateTreeConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTargetDistanceConsiderationInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	virtual float GetScore(FStateTreeExecutionContext& Context) const override
	{
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		// Calculate normalized score based on distance to target
		float Distance = FVector::Dist(InstanceData.OwnerLocation, InstanceData.TargetLocation);
		return FMath::GetMappedRangeValueClamped(
			FVector2D(0.f, InstanceData.MaxDistance),
			FVector2D(1.f, 0.f),
			Distance);
	}

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};