// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassCooldownTask.h"

#include "MassExecutionContext.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeFragments.h"
#include "StateTreeLinker.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"
#include "Engine/World.h"

FArcMassCooldownTask::FArcMassCooldownTask()
{
	bShouldCallTick = false;
}

bool FArcMassCooldownTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassCooldownHandle);
	Linker.LinkExternalData(MassGameplayTagHandle);
	
	return true;
}

void FArcMassCooldownTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FArcMassCooldownTaskFragment>();
	Builder.AddReadOnly<FArcMassGameplayTagContainerFragment>();
}

EStateTreeRunStatus FArcMassCooldownTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	FArcMassCooldownTaskFragment* MassCooldown = MassStateTreeContext.GetExternalDataPtr(MassCooldownHandle);
	FArcMassGameplayTagContainerFragment* MassGameplayTags = MassStateTreeContext.GetExternalDataPtr(MassGameplayTagHandle);
	
	if (!MassCooldown || !MassGameplayTags)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (MassCooldown->Cooldowns.ContainsByPredicate([Tag = InstanceData.CooldownTag](const FArcCooldownTaskItem& Item)
	{
		return Item.CooldownTag == Tag;
	}))
	{
		return EStateTreeRunStatus::Running;
	}
	
	MassCooldown->Cooldowns.Add({InstanceData.CooldownTag, InstanceData.Duration, 0.f});
	MassGameplayTags->Tags.AddTag(InstanceData.CooldownTag);
	
	FMassEntityManager& EM = MassStateTreeContext.GetEntityManager();
	EM.Defer().AddTag<FArcCooldownTaskTag>(MassStateTreeContext.GetEntity());
	
	return EStateTreeRunStatus::Running;
}

bool FArcMassIsOnCooldownCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassGameplayTagsHandle);
	
	return true;
}

void FArcMassIsOnCooldownCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FArcMassGameplayTagContainerFragment>();
}

bool FArcMassIsOnCooldownCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	FArcMassGameplayTagContainerFragment* MassGameplayTags = MassStateTreeContext.GetExternalDataPtr(MassGameplayTagsHandle);
	
	if (!MassGameplayTags)
	{
		return false;
	}
	
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const bool bHasTag = MassGameplayTags->Tags.HasTagExact(InstanceData.CooldownTag);
	return !bHasTag;
}

UArcMassCooldownTaskProcessor::UArcMassCooldownTaskProcessor()
	: EntityQuery_Conditional(*this)
{
}

void UArcMassCooldownTaskProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery_Conditional.AddRequirement<FArcMassCooldownTaskFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Conditional.AddRequirement<FArcMassGameplayTagContainerFragment>(EMassFragmentAccess::ReadWrite);
	
	EntityQuery_Conditional.AddTagRequirement<FArcCooldownTaskTag>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddSubsystemRequirement<UMassStateTreeSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Conditional.RegisterWithProcessor(*this);
}

void UArcMassCooldownTaskProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& MassEntityManager)
{
	Super::InitializeInternal(Owner, MassEntityManager);
	
	MassStateTreeSubsystem = UWorld::GetSubsystem<UMassStateTreeSubsystem>(Owner.GetWorld());
}

void UArcMassCooldownTaskProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!MassStateTreeSubsystem)
	{
		return;
	}
	
	EntityQuery_Conditional.ForEachEntityChunk(Context, [this](FMassExecutionContext& MyContext)
	{
		const TArrayView<FArcMassCooldownTaskFragment> CooldownList = MyContext.GetMutableFragmentView<FArcMassCooldownTaskFragment>();
		const TArrayView<FArcMassGameplayTagContainerFragment> GameplayTagsList = MyContext.GetMutableFragmentView<FArcMassGameplayTagContainerFragment>();
			
		const float WorldDeltaTime = MyContext.GetDeltaTimeSeconds();
			
		for (FMassExecutionContext::FEntityIterator EntityIt = MyContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = MyContext.GetEntity(EntityIt);
			
			FArcMassCooldownTaskFragment& CooldownTaskFragment = CooldownList[EntityIt];
			FArcMassGameplayTagContainerFragment& GameplayTagFragment = GameplayTagsList[EntityIt];
			for (int32 Idx = CooldownTaskFragment.Cooldowns.Num() - 1; Idx >= 0; --Idx)
			{
				FArcCooldownTaskItem& CooldownItem = CooldownTaskFragment.Cooldowns[Idx];
				CooldownItem.CurrentDuration += WorldDeltaTime;
				if (CooldownItem.CurrentDuration >= CooldownItem.CooldownDuration)
				{
					GameplayTagFragment.Tags.RemoveTag(CooldownItem.CooldownTag);
					CooldownTaskFragment.Cooldowns.RemoveAtSwap(Idx);
				}
			}
			
			if (CooldownTaskFragment.Cooldowns.IsEmpty())
			{
				MyContext.Defer().RemoveTag<FArcCooldownTaskTag>(Entity);
			}
		}
	});
}
