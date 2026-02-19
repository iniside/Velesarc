// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAddAbilityGameplayTagTask.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeLinker.h"

FArcAddAbilityGameplayTagTask::FArcAddAbilityGameplayTagTask()
{
	bShouldCallTick = false;
}

bool FArcAddAbilityGameplayTagTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorHandle);
	
	return true;
}

void FArcAddAbilityGameplayTagTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FMassActorFragment>();
}

EStateTreeRunStatus FArcAddAbilityGameplayTagTask::EnterState(FStateTreeExecutionContext& Context
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
	if (!ASC->HasAnyMatchingGameplayTags(InstanceData.TagsToAdd))
	{
		ASC->AddLooseGameplayTags(InstanceData.TagsToAdd);	
	}
	
	
	return EStateTreeRunStatus::Running;
}

void FArcAddAbilityGameplayTagTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* MassActor = MassCtx.GetExternalDataPtr(MassActorHandle);
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	
	if (!MassActor->Get())
	{
		return;
	}

	AActor* Actor = const_cast<AActor*>(MassActor->Get());
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);
	if (!ASI)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}
	
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ASC->RemoveLooseGameplayTags(InstanceData.TagsToAdd);
}
