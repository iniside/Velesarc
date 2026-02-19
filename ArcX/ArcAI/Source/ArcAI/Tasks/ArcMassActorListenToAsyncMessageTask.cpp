// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActorListenToAsyncMessageTask.h"

#include "ArcAIPerceptionComponent.h"
#include "AsyncGameplayMessageSystem.h"
#include "AsyncMessageBindingComponent.h"
#include "AsyncMessageWorldSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassActorListenToAsyncMessageTask::FArcMassActorListenToAsyncMessageTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = true;
}

bool FArcMassActorListenToAsyncMessageTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorFragment);

	return true;
}

void FArcMassActorListenToAsyncMessageTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
}

EStateTreeRunStatus FArcMassActorListenToAsyncMessageTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassActorListenToAsyncMessageTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* ActorFragment = MassCtx.GetExternalDataPtr(MassActorFragment);
	
	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Running;
	}
	
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	TSharedPtr<FAsyncGameplayMessageSystem> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(World);
	
	if (!ActorFragment->Get())
	{
		MessageSystem->UnbindListener(InstanceData.BoundListenerHandle);
		
		return EStateTreeRunStatus::Running;	
	}

	if (InstanceData.BoundListenerHandle.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}
	
	

	UAsyncMessageBindingComponent* BindComp = ActorFragment->Get()->FindComponentByClass<UAsyncMessageBindingComponent>();
	
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	IAsyncMessageBindingEndpointInterface* EndpointInterface = Cast<IAsyncMessageBindingEndpointInterface>(BindComp);
	
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
			, EndpointInterface->GetEndpoint());
	
	return EStateTreeRunStatus::Running;
}
