#include "ArcMassRemoveGameplayTagsFromEntityTask.h"

#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassRemoveGameplayTagsFromEntityTask::FArcMassRemoveGameplayTagsFromEntityTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FArcMassRemoveGameplayTagsFromEntityTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassGameplayTagsHandle);

	return true;
}

void FArcMassRemoveGameplayTagsFromEntityTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FArcMassGameplayTagContainerFragment>();
}

EStateTreeRunStatus FArcMassRemoveGameplayTagsFromEntityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcMassGameplayTagContainerFragment* MassTags = MassCtx.GetExternalDataPtr(MassGameplayTagsHandle);
	if (!MassTags)
	{
		return EStateTreeRunStatus::Failed;
	}

	MassTags->Tags.RemoveTags(InstanceData.Tags);
	
	return EStateTreeRunStatus::Succeeded;
}