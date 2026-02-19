// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActorListenGameplayTagCountChangedTask.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassActorListenGameplayTagCountChangedTask::FArcMassActorListenGameplayTagCountChangedTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = true;
}

bool FArcMassActorListenGameplayTagCountChangedTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorFragment);

	return true;
}

void FArcMassActorListenGameplayTagCountChangedTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
}

EStateTreeRunStatus FArcMassActorListenGameplayTagCountChangedTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* ActorFragment = MassCtx.GetExternalDataPtr(MassActorFragment);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (InstanceData.OnGameplayEffectCountChangedDelegateHandle.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}
	if (!ActorFragment->Get())
	{
		InstanceData.OnGameplayEffectCountChangedDelegateHandle.Reset();
		
		return EStateTreeRunStatus::Running;	
	}
	
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ActorFragment->GetMutable());
	if (!ASI)
	{
		return EStateTreeRunStatus::Running;
	}
	
	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	InstanceData.OnGameplayEffectCountChangedDelegateHandle = ASC->RegisterGameplayTagEvent(InstanceData.Tag, InstanceData.EventType).AddWeakLambda(ActorFragment->GetMutable(), 
		[WeakCtx = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity = MassCtx.GetEntity()]
		(const FGameplayTag Tag, int32 NewCount)
			{
				FStateTreeStrongExecutionContext StrongContext = WeakCtx.MakeStrongExecutionContext();
				FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
				if (!InstanceDataPtr)
				{
					return;	
				}
				InstanceDataPtr->NewCount = NewCount;
				if (NewCount >= InstanceDataPtr->MinimumCount)
				{
					StrongContext.BroadcastDelegate(InstanceDataPtr->OnGameplayTagCountChanged);
					SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});	
				}
				
				
			});
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassActorListenGameplayTagCountChangedTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return EStateTreeRunStatus::Running;
}
