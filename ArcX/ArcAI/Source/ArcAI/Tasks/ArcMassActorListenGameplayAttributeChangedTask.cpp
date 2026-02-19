// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActorListenGameplayAttributeChangedTask.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassActorListenGameplayAttributeChangedTask::FArcMassActorListenGameplayAttributeChangedTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = true;
}

bool FArcMassActorListenGameplayAttributeChangedTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorFragment);

	return true;
}

void FArcMassActorListenGameplayAttributeChangedTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
}

EStateTreeRunStatus FArcMassActorListenGameplayAttributeChangedTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* ActorFragment = MassCtx.GetExternalDataPtr(MassActorFragment);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (InstanceData.OnAttributeChangedDelegateHandle.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}
	if (!ActorFragment->Get())
	{
		InstanceData.OnAttributeChangedDelegateHandle.Reset();
		
		return EStateTreeRunStatus::Running;	
	}
	
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ActorFragment->GetMutable());
	if (!ASI)
	{
		return EStateTreeRunStatus::Running;
	}
	
	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	InstanceData.OnAttributeChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(InstanceData.Attribute).AddWeakLambda(ActorFragment->GetMutable(), 
		[WeakCtx = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity = MassCtx.GetEntity()]
		(const FOnAttributeChangeData& ChangeData)
			{
				FStateTreeStrongExecutionContext StrongContext = WeakCtx.MakeStrongExecutionContext();
				FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
				if (InstanceDataPtr)
				{
					return;
				}
				
				InstanceDataPtr->NewValue = ChangeData.NewValue;
				InstanceDataPtr->OldValue = ChangeData.OldValue;
				InstanceDataPtr->Difference = ChangeData.NewValue - ChangeData.OldValue;
				bool bShouldBroadcast = false;
				switch (InstanceDataPtr->BroadcastCondition)
				{
					case EArcAttributeBroadcastCondition::Always:
						bShouldBroadcast = true;
						break;
					case EArcAttributeBroadcastCondition::NewValueGreater:
						bShouldBroadcast = InstanceDataPtr->NewValue > InstanceDataPtr->ConditionValue;
						break;
					case EArcAttributeBroadcastCondition::NewValueLess:
						bShouldBroadcast = InstanceDataPtr->NewValue < InstanceDataPtr->ConditionValue;
						break;
					case EArcAttributeBroadcastCondition::OldValueGreater:
						bShouldBroadcast = InstanceDataPtr->OldValue > InstanceDataPtr->ConditionValue;
						break;
					case EArcAttributeBroadcastCondition::OldValueLess:
						bShouldBroadcast = InstanceDataPtr->OldValue < InstanceDataPtr->ConditionValue;
						break;
					case EArcAttributeBroadcastCondition::DifferenceValueGreater:
						bShouldBroadcast = InstanceDataPtr->Difference > InstanceDataPtr->ConditionValue;
						break;
					case EArcAttributeBroadcastCondition::DifferenceValueLess:
						bShouldBroadcast = InstanceDataPtr->Difference < InstanceDataPtr->ConditionValue;
						break;
					default:
						break;
				}
				
				if (!bShouldBroadcast)
				{
					return;
				}
				
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnAttributeChanged);
				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
				
			});
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassActorListenGameplayAttributeChangedTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return FMassStateTreeTaskBase::Tick(Context, DeltaTime);
}
