#include "ArcGetMassActorTask.h"

#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FArcGetMassActorTask::FArcGetMassActorTask()
{
	bShouldCallTick = false;
}

bool FArcGetMassActorTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorHandle);
	
	return true;
}

void FArcGetMassActorTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FMassActorFragment>();
}

EStateTreeRunStatus FArcGetMassActorTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* MassActor = MassStateTreeContext.GetExternalDataPtr(MassActorHandle);
	if (!MassActor)
	{
		UE_LOG(LogStateTree, Warning, TEXT("FArcGetMassActorTask::EnterState: MassActor is null"));
		return EStateTreeRunStatus::Failed;
	}

	if (!MassActor->Get())
	{
		UE_LOG(LogStateTree, Warning, TEXT("FArcGetMassActorTask::EnterState: MassActor's Actor is null"));
		return EStateTreeRunStatus::Failed;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Output = const_cast<AActor*>(MassActor->Get());

	auto [ActorPtr] = InstanceData.Result.GetMutablePtrTuple<AActor*>(Context);
	
	if (ActorPtr)
	{
		*ActorPtr = const_cast<AActor*>(MassActor->Get());
	}
	
	UE_LOG(LogStateTree, Log, TEXT("FArcGetMassActorTask::EnterState: MassActor's Actor %s"), *GetNameSafe(InstanceData.Output));
	
	return EStateTreeRunStatus::Succeeded;
}