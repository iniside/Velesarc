#include "ArcMassUseGameplayAbilityTask.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassUseGameplayAbilityTask::FArcMassUseGameplayAbilityTask()
{
	bShouldCallTick = false;
}

bool FArcMassUseGameplayAbilityTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorHandle);
	
	return true;
}

void FArcMassUseGameplayAbilityTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FMassActorFragment>();
}

EStateTreeRunStatus FArcMassUseGameplayAbilityTask::EnterState(FStateTreeExecutionContext& Context
															   , const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* MassActor = MassCtx.GetExternalDataPtr(MassActorHandle);
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	
	if (!MassActor->Get())
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Actor = const_cast<AActor*>(MassActor->Get());
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);
	if (!ASI)
	{
		return EStateTreeRunStatus::Failed;
	}

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC)
	{
		return EStateTreeRunStatus::Failed;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.OnAbilityEndedHandle = ASC->AbilityEndedCallbacks.AddWeakLambda(Actor, [WeakContext = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity = MassCtx.GetEntity(), ASC](UGameplayAbility* Ability)
		{
			const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!InstanceDataPtr)
			{
				StrongContext.FinishTask(EStateTreeFinishTaskType::Failed);
				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
			}
			
			if (InstanceDataPtr->AbilityHandle == Ability->GetCurrentAbilitySpecHandle())
			{
				
				ASC->AbilityEndedCallbacks.Remove(InstanceDataPtr->OnAbilityEndedHandle);
				StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
			}
		});

	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(InstanceData.AbilityClassToActivate);
	if (!Spec)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	InstanceData.AbilityHandle = Spec->Handle;
	
	bool bSuccess = ASC->TryActivateAbility(Spec->Handle);
	if (!bSuccess)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	return EStateTreeRunStatus::Running;
}