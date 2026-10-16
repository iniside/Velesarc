// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassCooldownTask.h"

#include "Abilities/ArcAbilityFunctions.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Engine/World.h"
#include "ArcMass/ArcMassDecayingTags.h"

FArcMassCooldownTask::FArcMassCooldownTask()
{
	bShouldCallTick = false;
}

bool FArcMassCooldownTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassGameplayTagHandle);

	return true;
}

void FArcMassCooldownTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FArcOwnedTagsFragment>();
}

EStateTreeRunStatus FArcMassCooldownTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	FArcOwnedTagsFragment* MassGameplayTags = MassStateTreeContext.GetExternalDataPtr(MassGameplayTagHandle);
	if (!MassGameplayTags)
	{
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcMassDecayingTagSubsystem* DecaySys = World->GetSubsystem<UArcMassDecayingTagSubsystem>();
	if (!DecaySys)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (MassGameplayTags->GetTags().HasTagExact(InstanceData.CooldownTag))
	{
		// Already cooling — let the existing decay run to completion.
		return EStateTreeRunStatus::Running;
	}

	DecaySys->RequestAddDecayingTag(MassStateTreeContext.GetEntity(), InstanceData.CooldownTag, InstanceData.Duration);
	FMassEntityManager& EM = MassStateTreeContext.GetEntityManager();
	ArcAbilities::AddOwnedTags(EM, MassStateTreeContext.GetEntity(), FGameplayTagContainer(InstanceData.CooldownTag));
	EM.Defer().AddTag<FArcCooldownTaskTag>(MassStateTreeContext.GetEntity());

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FArcMassCooldownTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "CooldownTaskDesc", "Cooldown {0} for {1}s"), FText::FromString(InstanceData->CooldownTag.ToString()), FText::AsNumber(InstanceData->Duration));
		}
	}
	return NSLOCTEXT("ArcAI", "CooldownTaskDescDefault", "Cooldown");
}
#endif

bool FArcMassIsOnCooldownCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassGameplayTagsHandle);

	return true;
}

void FArcMassIsOnCooldownCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FArcOwnedTagsFragment>();
}

bool FArcMassIsOnCooldownCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	FArcOwnedTagsFragment* MassGameplayTags = MassStateTreeContext.GetExternalDataPtr(MassGameplayTagsHandle);

	if (!MassGameplayTags)
	{
		return false;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const bool bHasTag = MassGameplayTags->GetTags().HasTagExact(InstanceData.CooldownTag);
	return !bHasTag;
}

#if WITH_EDITOR
FText FArcMassIsOnCooldownCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "IsOnCooldownDesc", "On Cooldown: {0}"),
				FText::FromString(InstanceData->CooldownTag.ToString()));
		}
	}
	return FText::GetEmpty();
}
#endif
