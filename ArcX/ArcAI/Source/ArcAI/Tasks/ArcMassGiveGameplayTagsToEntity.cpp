#include "ArcMassGiveGameplayTagsToEntity.h"

#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassGiveGameplayTagsToEntityTask::FArcMassGiveGameplayTagsToEntityTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FArcMassGiveGameplayTagsToEntityTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassGameplayTagsHandle);

	return true;
}

void FArcMassGiveGameplayTagsToEntityTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FArcMassGameplayTagContainerFragment>();
}

EStateTreeRunStatus FArcMassGiveGameplayTagsToEntityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcMassGameplayTagContainerFragment* MassTags = MassCtx.GetExternalDataPtr(MassGameplayTagsHandle);
	if (!MassTags)
	{
		return EStateTreeRunStatus::Failed;
	}

	MassTags->Tags.AppendTags(InstanceData.Tags);
	
	return EStateTreeRunStatus::Succeeded;
}