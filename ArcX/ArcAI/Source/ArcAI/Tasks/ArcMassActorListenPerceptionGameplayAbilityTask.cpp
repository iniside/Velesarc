// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActorListenPerceptionGameplayAbilityTask.h"

#include "ArcAIPerceptionComponent.h"
#include "AsyncGameplayMessageSystem.h"
#include "AsyncMessageBindingComponent.h"
#include "AsyncMessageWorldSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassActorListenPerceptionGameplayAbilityTask::FArcMassActorListenPerceptionGameplayAbilityTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = true;
}

bool FArcMassActorListenPerceptionGameplayAbilityTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorFragment);

	return true;
}

void FArcMassActorListenPerceptionGameplayAbilityTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
}

EStateTreeRunStatus FArcMassActorListenPerceptionGameplayAbilityTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassActorListenPerceptionGameplayAbilityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* ActorFragment = MassCtx.GetExternalDataPtr(MassActorFragment);

	if (!ActorFragment->Get())
	{
		return EStateTreeRunStatus::Running;	
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UAsyncMessageBindingComponent* BindComp = ActorFragment->Get()->FindComponentByClass<UAsyncMessageBindingComponent>();
	TSharedPtr<FAsyncGameplayMessageSystem> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(World);
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	IAsyncMessageBindingEndpointInterface* EnpointInerface = Cast<IAsyncMessageBindingEndpointInterface>(BindComp);
	
	InstanceData.BoundListenerHandle = MessageSystem->BindListener(
		InstanceData.MessageToListenFor,
		[this, SignalSubsystem, WeakContext = Context.MakeWeakExecutionContext(), Entity = MassCtx.GetEntity()](const FAsyncMessage& Message)
			{
				FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
				FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
				if (InstanceDataPtr)
				{
					if (InstanceDataPtr->OutputInstanced.GetScriptStruct() == Message.GetPayloadView().GetScriptStruct())
					{
						InstanceDataPtr->OutputInstanced.Reset();
						InstanceDataPtr->OutputInstanced.InitializeAsScriptStruct( Message.GetPayloadView().GetScriptStruct(), Message.GetPayloadView().GetMemory());
					
						StrongContext.BroadcastDelegate(InstanceDataPtr->OnResultChanged);
						SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
					}
				}
			}
			, FAsyncMessageBindingOptions()
			, EnpointInerface->GetEndpoint());
	
	return EStateTreeRunStatus::Running;
}
