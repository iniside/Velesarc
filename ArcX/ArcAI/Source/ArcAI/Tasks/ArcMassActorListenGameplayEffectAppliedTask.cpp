// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActorListenGameplayEffectAppliedTask.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassActorListenGameplayEffectAppliedTask::FArcMassActorListenGameplayEffectAppliedTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = true;
}

bool FArcMassActorListenGameplayEffectAppliedTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorFragment);

	return true;
}

void FArcMassActorListenGameplayEffectAppliedTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
}

EStateTreeRunStatus FArcMassActorListenGameplayEffectAppliedTask::EnterState(FStateTreeExecutionContext& Context
																			 , const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* ActorFragment = MassCtx.GetExternalDataPtr(MassActorFragment);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (InstanceData.OnAppliedToSelfDelegateHandle.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}
	if (!ActorFragment->Get())
	{
		InstanceData.OnAppliedToSelfDelegateHandle.Reset();
		
		return EStateTreeRunStatus::Running;	
	}
	
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ActorFragment->GetMutable());
	if (!ASI)
	{
		return EStateTreeRunStatus::Running;
	}
	
	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	InstanceData.OnAppliedToSelfDelegateHandle = ASC->OnGameplayEffectAppliedDelegateToSelf.AddWeakLambda(ActorFragment->GetMutable(), 
		[WeakCtx = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity = MassCtx.GetEntity()](UAbilitySystemComponent* InASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
			{
				FStateTreeStrongExecutionContext StrongContext = WeakCtx.MakeStrongExecutionContext();
				FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
				if (InstanceDataPtr)
				{
					FGameplayTagContainer AssetTags;
					Spec.GetAllAssetTags(AssetTags);
					
					if (!AssetTags.HasAll(InstanceDataPtr->RequiredTags))
					{
						return;
					}
					
					if (AssetTags.HasAny(InstanceDataPtr->DenyTags))
					{
						return;
					}
					
					InstanceDataPtr->Duration = Spec.GetDuration();
					StrongContext.BroadcastDelegate(InstanceDataPtr->OnEffectAppliedToSelf);
					SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
				}
				
			});
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassActorListenGameplayEffectAppliedTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return EStateTreeRunStatus::Running;
}
