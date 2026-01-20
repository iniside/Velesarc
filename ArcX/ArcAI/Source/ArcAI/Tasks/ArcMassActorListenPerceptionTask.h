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

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> Output;
	
	UPROPERTY()
	TWeakObjectPtr<UArcAIPerceptionComponent> PerceptionComponent;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;

	FDelegateHandle PerceptionChangedDelegate;
	FTimerHandle TimerHandle;

	TArray<AActor*> PerceivedActors;
};

USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Perception"))
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

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;
};

USTRUCT()
struct FArcMassActorListenSightPerceptionTaskInstanceData
{
	GENERATED_BODY()

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, Category = Output)
	TArray<TObjectPtr<AActor>> Output;
	
	UPROPERTY()
	TWeakObjectPtr<UArcAIPerceptionComponent> PerceptionComponent;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;

	FDelegateHandle PerceptionChangedDelegate;
	FTimerHandle TimerHandle;
};

USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Perception"))
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
 * Evaluates utility for all (Target, Action) combinations. 
 * 
 * Considerations in action states can bind to CurrentTargetActor to access
 * the target being evaluated (e.g., CurrentTargetActor. Health, CurrentTargetActor.Location).
 */
USTRUCT(meta = (DisplayName = "Utility Target Selection"))
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
    
	/** List of potential target actors to evaluate */
	UPROPERTY(EditAnywhere, Category = "Input")
	FVector TargetLocation;
    
	/** List of action state handles to evaluate */
	UPROPERTY(EditAnywhere, Category = "Input")
	FVector OwnerLocation;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MaxDistance;
};

USTRUCT(meta = (DisplayName = "Target Distance Consideration"))
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
};