#include "ArcMassRemoveGameplayTagsFromEntityTask.h"

#include "Abilities/ArcAbilityFunctions.h"
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
	Builder.AddReadWrite<FArcOwnedTagsFragment>();
}

EStateTreeRunStatus FArcMassRemoveGameplayTagsFromEntityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcOwnedTagsFragment* MassTags = MassCtx.GetExternalDataPtr(MassGameplayTagsHandle);
	if (!MassTags)
	{
		return EStateTreeRunStatus::Failed;
	}

	ArcAbilities::RemoveOwnedTags(MassCtx.GetEntityManager(), MassCtx.GetEntity(), InstanceData.Tags);

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FArcMassRemoveGameplayTagsFromEntityTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "RemoveTagsDesc", "Remove Tags: {0}"), FText::FromString(InstanceData->Tags.ToStringSimple()));
		}
	}
	return FText::GetEmpty();
}
#endif