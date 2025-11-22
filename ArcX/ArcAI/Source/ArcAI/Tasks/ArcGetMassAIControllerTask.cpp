#include "ArcGetMassAIControllerTask.h"

#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

#include "GameFramework/Pawn.h"
#include "AIController.h"

FArcGetMassAIControllerTask::FArcGetMassAIControllerTask()
{
	bShouldCallTick = false;
}


bool FArcGetMassAIControllerTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorHandle);
	
	return true;
}

void FArcGetMassAIControllerTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FMassActorFragment>();
}

EStateTreeRunStatus FArcGetMassAIControllerTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* MassActor = MassStateTreeContext.GetExternalDataPtr(MassActorHandle);
	if (!MassActor)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!MassActor->Get())
	{
		return EStateTreeRunStatus::Failed;
	}

	const APawn* P = Cast<APawn>(MassActor->Get());
	if (!P)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Output = P->GetController<AAIController>();

	AAIController** ActorPtr = InstanceData.Result.GetMutablePtr<AAIController*>(Context);
	
	if (ActorPtr)
	{
		*ActorPtr = InstanceData.Output;
	}
	
	return EStateTreeRunStatus::Succeeded;
}