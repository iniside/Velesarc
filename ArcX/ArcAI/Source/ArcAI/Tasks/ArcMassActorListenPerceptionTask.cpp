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
