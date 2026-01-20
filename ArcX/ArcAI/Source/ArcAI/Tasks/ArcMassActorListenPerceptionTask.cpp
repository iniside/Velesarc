#include "ArcMassActorListenPerceptionTask.h"

#include "ArcAIPerceptionComponent.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"

FArcMassActorListenPerceptionTask::FArcMassActorListenPerceptionTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = true;
}

bool FArcMassActorListenPerceptionTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorFragment);

	return true;
}

void FArcMassActorListenPerceptionTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
}

EStateTreeRunStatus FArcMassActorListenPerceptionTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassActorListenPerceptionTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* ActorFragment = MassCtx.GetExternalDataPtr(MassActorFragment);

	if (!ActorFragment->Get())
	{
		return EStateTreeRunStatus::Running;	
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	const AActor* AgentActor = ActorFragment->Get();

	if (!InstanceData.PerceptionComponent.IsValid())
	{
		InstanceData.PerceptionComponent = AgentActor->FindComponentByClass<UArcAIPerceptionComponent>();
	}
	
	if (!InstanceData.PerceptionComponent.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.PerceptionChangedDelegate.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	InstanceData.PerceptionComponent->NativeOnPerceptionChanged.AddWeakLambda(AgentActor
		, [this, SignalSubsystem, WeakContext = Context.MakeWeakExecutionContext(), Entity = MassCtx.GetEntity()]()
	{
			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
				
			UArcAIPerceptionComponent* AIPerception = InstanceDataPtr->PerceptionComponent.Get();
			TArray<AActor*> OutActors;
			AIPerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), OutActors);

			if (InstanceDataPtr->PerceivedActors.Num() != OutActors.Num())
			{
				InstanceDataPtr->PerceivedActors = OutActors;
				InstanceDataPtr->PerceivedActors = OutActors;
				if (OutActors.Num() > 0)
				{
					InstanceDataPtr->Output = OutActors[0];
				}
				else
				{
					InstanceDataPtr->Output = nullptr;
				}
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnResultChanged);
				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
			}
	});
	//FTimerDelegate Delegate = FTimerDelegate::CreateWeakLambda(AgentActor, [this, WeakContext = Context.MakeWeakExecutionContext()
	//	, SignalSubsystem, Entity = MassCtx.GetEntity()]()
	//{
	//	
	//});
	//Context.GetWorld()->GetTimerManager().SetTimer(InstanceData.TimerHandle, Delegate, 0.1, true);
	
	return EStateTreeRunStatus::Running;
}


FArcMassActorListenSightPerceptionTask::FArcMassActorListenSightPerceptionTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = true;
}

bool FArcMassActorListenSightPerceptionTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorFragment);

	return true;
}

void FArcMassActorListenSightPerceptionTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
}

EStateTreeRunStatus FArcMassActorListenSightPerceptionTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassActorListenSightPerceptionTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* ActorFragment = MassCtx.GetExternalDataPtr(MassActorFragment);

	if (!ActorFragment->Get())
	{
		return EStateTreeRunStatus::Running;	
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	const AActor* AgentActor = ActorFragment->Get();

	if (!InstanceData.PerceptionComponent.IsValid())
	{
		InstanceData.PerceptionComponent = AgentActor->FindComponentByClass<UArcAIPerceptionComponent>();
	}
	
	if (!InstanceData.PerceptionComponent.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.PerceptionChangedDelegate.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	InstanceData.PerceptionComponent->NativeOnPerceptionChanged.AddWeakLambda(AgentActor
		, [this, SignalSubsystem, WeakContext = Context.MakeWeakExecutionContext(), Entity = MassCtx.GetEntity()]()
	{
			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
				
			UArcAIPerceptionComponent* AIPerception = InstanceDataPtr->PerceptionComponent.Get();
			TArray<AActor*> OutActors;
			AIPerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), OutActors);

			
			{
				InstanceDataPtr->Output = OutActors;
				
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnResultChanged);
				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
			}
	});
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FUtilityTargetSelectionTask::EnterState(
    FStateTreeExecutionContext& Context, 
    const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
    
    if (InstanceData.TargetActors.IsEmpty() || InstanceData.ActionStates.IsEmpty())
    {
        return EStateTreeRunStatus::Failed;
    }
    
    float BestScore = -1.f;
    AActor* BestTarget = nullptr;
    FStateTreeStateHandle BestAction;
    
    for (AActor* TargetActor : InstanceData.TargetActors)
    {
        if (! IsValid(TargetActor))
        {
            continue;
        }
        
        for (const FStateTreeStateLink& ActionState : InstanceData.ActionStates)
        {
            if (!ActionState.StateHandle.IsValid())
            {
                continue;
            }
            
            const float Score = EvaluateStateUtilityForTarget(
                Context, InstanceData, TargetActor, ActionState.StateHandle);
            
            if (Score > BestScore)
            {
                BestScore = Score;
                BestTarget = TargetActor;
                BestAction = ActionState.StateHandle;
            }
        }
    }
    
    // Clear the evaluation target
    InstanceData.CurrentTargetActor = nullptr;
    
    // Store results
    InstanceData.SelectedTarget = BestTarget;
    InstanceData.SelectedActionState = BestAction;
    InstanceData.BestUtilityScore = BestScore;
    
    if (BestScore < InstanceData.MinimumScoreThreshold || ! BestTarget)
    {
        return EStateTreeRunStatus::Failed;
    }
    
	if (BestScore >= InstanceData.MinimumScoreThreshold && BestTarget)
	{
		UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
		// Store results
		InstanceData.SelectedTarget = BestTarget;
		InstanceData.SelectedActionState = BestAction;
        
		// Immediately request transition to the selected action state
		Context.RequestTransition(BestAction, EStateTreeTransitionPriority::Normal);
        
		FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
		
		return EStateTreeRunStatus::Succeeded;
	}
	
    return EStateTreeRunStatus:: Succeeded;
}

float FUtilityTargetSelectionTask:: EvaluateStateUtilityForTarget(
    FStateTreeExecutionContext& Context,
    FInstanceDataType& InstanceData,
    AActor* TargetActor,
    FStateTreeStateHandle ActionStateHandle) const
{
    const UStateTree* StateTree = Context.GetStateTree();
    const FCompactStateTreeState* ActionState = StateTree->GetStateFromHandle(ActionStateHandle);
    
    if (!ActionState || ActionState->UtilityConsiderationsNum == 0)
    {
        return 0.f;
    }
    
    // KEY:  Set the current target - considerations bind to this property!
    // Bindings like "CurrentTargetActor.Health" will now resolve to this actor
    InstanceData.CurrentTargetActor = TargetActor;
    
    // Get execution frame info
    const FStateTreeExecutionState& ExecState = Context.GetExecState();
    if (ExecState.ActiveFrames.IsEmpty())
    {
        return 0.f;
    }
    
    const FStateTreeExecutionFrame& CurrentFrame = ExecState.ActiveFrames[0];
    
    // Calculate memory requirements for evaluation scope
    using FMemoryRequirement = UE::StateTree:: InstanceData::FEvaluationScopeInstanceContainer::FMemoryRequirement;
    FMemoryRequirement MemoryRequirement;
    
    // Use existing utility evaluation - bindings are automatically copied 
    // because EvaluateUtilityWithValidation calls CopyBatchWithValidation internally
    const float Score = Context.EvaluateUtilityWithValidation(
        nullptr,
        CurrentFrame,
        ActionStateHandle,
        MemoryRequirement,
        ActionState->UtilityConsiderationsBegin,
        ActionState->UtilityConsiderationsNum,
        ActionState->Weight
    );
    
    return Score;
}