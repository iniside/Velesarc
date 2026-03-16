// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAssignToAreaTask.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntityView.h"
#include "ArcAreaSubsystem.h"
#include "ArcArea/Mass/ArcAreaAssignmentFragments.h"
#include "ArcKnowledge/Mass/ArcKnowledgeFragments.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"

EStateTreeRunStatus FArcMassAssignToAreaTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.bAssignmentSucceeded = false;
	InstanceData.CandidateSlots.Reset();

	if (!InstanceData.SlotHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcAreaSubsystem* AreaSubsystem = World->GetSubsystem<UArcAreaSubsystem>();
	if (!AreaSubsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle Entity = MassCtx.GetEntity();
	InstanceData.bAssignmentSucceeded = AreaSubsystem->AssignToSlot(InstanceData.SlotHandle, Entity);

	if (!InstanceData.bAssignmentSucceeded)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Resolve SmartObject candidate slots by matching entity tags against SO slot activity tags.
	const FArcAreaData* AreaData = AreaSubsystem->GetAreaData(InstanceData.SlotHandle.AreaHandle);
	if (!AreaData || !AreaData->SmartObjectHandle.IsValid())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const USmartObjectSubsystem* SOSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
	if (!SOSubsystem)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// Gather entity tags from owner fragments and knowledge entries.
	FGameplayTagContainer EntityTags;

	const FMassEntityView EntityView(MassCtx.GetEntityManager(), Entity);

	// 1) From archetype config: eligible roles (e.g., "Role.Blacksmith", "Role.Guard")
	if (const FArcAreaAssignmentConfigFragment* Config = EntityView.GetConstSharedFragmentDataPtr<FArcAreaAssignmentConfigFragment>())
	{
		EntityTags.AppendTags(Config->EligibleRoles);
	}

	// 2) From knowledge member: individual role tag
	if (const FArcKnowledgeMemberFragment* KnowledgeMember = EntityView.GetFragmentDataPtr<FArcKnowledgeMemberFragment>())
	{
		if (KnowledgeMember->Role.IsValid())
		{
			EntityTags.AddTag(KnowledgeMember->Role);
		}

		// 3) From registered knowledge entries: merge their tags
		if (const UArcKnowledgeSubsystem* KnowledgeSubsystem = World->GetSubsystem<UArcKnowledgeSubsystem>())
		{
			for (const FArcKnowledgeHandle& KnowledgeHandle : KnowledgeMember->RegisteredKnowledgeHandles)
			{
				if (const FArcKnowledgeEntry* Entry = KnowledgeSubsystem->GetKnowledgeEntry(KnowledgeHandle))
				{
					EntityTags.AppendTags(Entry->Tags);
				}
			}
		}
	}

	TArray<FSmartObjectSlotHandle> MatchedSlots;

	// Primary: match SO slots by activity tags using the entity's combined tags.
	if (!EntityTags.IsEmpty())
	{
		FSmartObjectRequestFilter Filter;
		Filter.ActivityRequirements = FGameplayTagQuery::MakeQuery_MatchAnyTags(EntityTags);
		SOSubsystem->FindSlots(AreaData->SmartObjectHandle, Filter, MatchedSlots);
	}

	// Fallback: if no tag match, find free slots with no activity tags.
	if (MatchedSlots.IsEmpty())
	{
		TArray<FSmartObjectSlotHandle> AllSlots;
		SOSubsystem->GetAllSlots(AreaData->SmartObjectHandle, AllSlots);

		for (const FSmartObjectSlotHandle& SlotHandle : AllSlots)
		{
			if (SOSubsystem->GetSlotState(SlotHandle) != ESmartObjectSlotState::Free)
			{
				continue;
			}

			bool bHasActivityTags = false;
			SOSubsystem->ReadSlotData(SlotHandle, [&bHasActivityTags](FConstSmartObjectSlotView SlotView)
			{
				FGameplayTagContainer ActivityTags;
				SlotView.GetActivityTags(ActivityTags);
				bHasActivityTags = !ActivityTags.IsEmpty();
			});

			if (!bHasActivityTags)
			{
				MatchedSlots.Add(SlotHandle);
			}
		}
	}

	// Populate candidate slots (up to MaxNumCandidates).
	const int32 NumCandidates = FMath::Min(MatchedSlots.Num(),
		static_cast<int32>(FMassSmartObjectCandidateSlots::MaxNumCandidates));

	for (int32 i = 0; i < NumCandidates; ++i)
	{
		FSmartObjectCandidateSlot& Candidate = InstanceData.CandidateSlots.Slots[InstanceData.CandidateSlots.NumSlots];
		Candidate.Result.SmartObjectHandle = AreaData->SmartObjectHandle;
		Candidate.Result.SlotHandle = MatchedSlots[i];
		Candidate.Cost = 0.f;
		InstanceData.CandidateSlots.NumSlots++;
	}

	return EStateTreeRunStatus::Succeeded;
}
