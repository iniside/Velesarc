#include "ArcListenTargetAbilityActivatedTask.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"

FArcListenTargetAbilityActivatedTask::FArcListenTargetAbilityActivatedTask()
{
	bShouldCallTick = false;
}

bool FArcListenTargetAbilityActivatedTask::Link(FStateTreeLinker& Linker)
{
	return FMassStateTreeTaskBase::Link(Linker);
}

void FArcListenTargetAbilityActivatedTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	FMassStateTreeTaskBase::GetDependencies(Builder);
}

EStateTreeRunStatus FArcListenTargetAbilityActivatedTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InstanceData.InputTarget);
	if (!ASI)
	{
		return EStateTreeRunStatus::Failed;
	}

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	InstanceData.OnAbilityActivatedHandle =ASI->GetAbilitySystemComponent()->AbilityActivatedCallbacks.AddWeakLambda(InstanceData.InputTarget.Get(),
		[WeakContext = Context.MakeWeakExecutionContext(), Entity = MassCtx.GetEntity(), SignalSubsystem]
		(UGameplayAbility* InAbility)
		{
			const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			if (FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>())
			{
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnAbilityActivated);

				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
			}
		});
	
	return EStateTreeRunStatus::Running;
}

void FArcListenTargetAbilityActivatedTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InstanceData.InputTarget);
	if (!ASI)
	{
		return;
	}

	ASI->GetAbilitySystemComponent()->AbilityActivatedCallbacks.Remove(InstanceData.OnAbilityActivatedHandle);
}
