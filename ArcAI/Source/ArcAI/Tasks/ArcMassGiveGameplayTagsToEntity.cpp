#include "ArcMassGiveGameplayTagsToEntity.h"

#include "Abilities/ArcAbilityFunctions.h"
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
	Builder.AddReadWrite<FArcOwnedTagsFragment>();
}

EStateTreeRunStatus FArcMassGiveGameplayTagsToEntityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcOwnedTagsFragment* MassTags = MassCtx.GetExternalDataPtr(MassGameplayTagsHandle);
	if (!MassTags)
	{
		return EStateTreeRunStatus::Failed;
	}

	ArcAbilities::AddOwnedTags(MassCtx.GetEntityManager(), MassCtx.GetEntity(), InstanceData.Tags);
	
	return EStateTreeRunStatus::Succeeded;
}

void FArcMassGiveGameplayTagsToEntityTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (bRemoveOnExit)
	{
		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
		FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
		FArcOwnedTagsFragment* MassTags = MassCtx.GetExternalDataPtr(MassGameplayTagsHandle);
		if (!MassTags)
		{
			return;
		}

		ArcAbilities::RemoveOwnedTags(MassCtx.GetEntityManager(), MassCtx.GetEntity(), InstanceData.Tags);
	}
}

#if WITH_EDITOR
FText FArcMassGiveGameplayTagsToEntityTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "GiveTagsDesc", "Give Tags: {0}{1}"), FText::FromString(InstanceData->Tags.ToStringSimple()), bRemoveOnExit ? FText::FromString(TEXT(" (remove on exit)")) : FText::GetEmpty());
		}
	}
	return FText::GetEmpty();
}
#endif
