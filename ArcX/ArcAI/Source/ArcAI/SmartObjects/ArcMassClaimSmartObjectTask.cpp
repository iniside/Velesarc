// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassClaimSmartObjectTask.h"

#include "ArcCandidateSlotsFromSmartObjectItemFunction.h"
#include "MassAIBehaviorTypes.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectHandler.h"
#include "MassSmartObjectSettings.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"
#include "MassSignalSubsystem.h"

FArcMassClaimSmartObjectTask::FArcMassClaimSmartObjectTask()
{
	// This task should not react to Enter/ExitState when the state is reselected.
	bShouldStateChangeOnReselect = false;
}

bool FArcMassClaimSmartObjectTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectUserHandle);
	Linker.LinkExternalData(SmartObjectMRUSlotsHandle);
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	Linker.LinkExternalData(MassSignalSubsystemHandle);

	return true;
}

void FArcMassClaimSmartObjectTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite(SmartObjectUserHandle);
	Builder.AddReadWrite(SmartObjectMRUSlotsHandle);
	// @todo SmartObjectSubsystemHandle is being used in a RW fashion, but we need this task to be able to run in parallel with
	// everything else, so we need to ensure TMassExternalSubsystemTraits<USmartObjectSubsystem> is marked up for parallel access
	// and that this information is properly utilized during Mass processing graph creation
	Builder.AddReadOnly(SmartObjectSubsystemHandle);
	Builder.AddReadOnly(MassSignalSubsystemHandle);
}

EStateTreeRunStatus FArcMassClaimSmartObjectTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// Retrieve fragments and subsystems
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	
	if (!InstanceData.SmartObjectItem.SlotHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ClaimedSlot.Invalidate();
	InstanceData.ClaimedSlotTransform = FTransform::Identity;

	// Setup MassSmartObject handler and claim
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FMassSmartObjectHandler MassSmartObjectHandler(MassStateTreeContext.GetMassEntityExecutionContext(), SmartObjectSubsystem, SignalSubsystem);

	FMassSmartObjectCandidateSlots CandidateSlots;
	CandidateSlots.Slots[0].Result.SmartObjectHandle = InstanceData.SmartObjectItem.SmartObjectHandle;
	CandidateSlots.Slots[0].Result.SlotHandle = InstanceData.SmartObjectItem.SlotHandle;
	CandidateSlots.NumSlots = 1;
	
	InstanceData.ClaimedSlot = MassSmartObjectHandler.ClaimCandidate(MassStateTreeContext.GetEntity(), SOUser, CandidateSlots);

	// Treat claiming a slot as consuming all the candidate slots.
	// This is done here because of the limited ways we can communicate between FindSmartObject() and ClaimSmartObject().
	// InteractionCooldownEndTime is used by the FindSmartObject() to invalidate the candidates.
	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	if (!InstanceData.ClaimedSlot.IsValid())
	{
		MASSBEHAVIOR_LOG(Log, TEXT("Failed to claim smart object slot from %d candidates"), CandidateSlots.NumSlots);
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ClaimedSlotTransform = SmartObjectSubsystem.GetSlotTransform(InstanceData.ClaimedSlot).Get(FTransform::Identity);
	return EStateTreeRunStatus::Running;
}

void FArcMassClaimSmartObjectTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Succeeded or not, prevent interactions for a specified duration.
	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	if (InstanceData.ClaimedSlot.IsValid())
	{
		const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		FMassExecutionContext& MassContext = MassStateTreeContext.GetMassEntityExecutionContext();
		const FMassEntityHandle Entity = MassStateTreeContext.GetEntity();

		if (UE::Mass::SmartObject::FMRUSlotsFragment* ExistingMRUSlotsFragment = Context.GetExternalDataPtr(SmartObjectMRUSlotsHandle))
		{
			ExistingMRUSlotsFragment->Slots.Push(InstanceData.ClaimedSlot.SlotHandle);
		}
		else
		{
			const UMassSmartObjectSettings* MassSmartObjectSettings = GetDefault<UMassSmartObjectSettings>();
			if (MassSmartObjectSettings != nullptr
				&& MassSmartObjectSettings->MRUSlotsMaxCount > 0)
			{
				UE::Mass::SmartObject::FMRUSlotsFragment MRUSlotsFragment;
				MRUSlotsFragment.Slots.Push(InstanceData.ClaimedSlot.SlotHandle);
				MassContext.Defer().PushCommand<FMassCommandAddFragmentInstances>(Entity, MRUSlotsFragment);
			}
		}

		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler MassSmartObjectHandler(MassContext, SmartObjectSubsystem, SignalSubsystem);

		MassSmartObjectHandler.ReleaseSmartObject(Entity, SOUser, InstanceData.ClaimedSlot);
	}
	else
	{
		MASSBEHAVIOR_LOG(VeryVerbose, TEXT("Exiting state with an invalid ClaimHandle: nothing to do."));
	}
}

EStateTreeRunStatus FArcMassClaimSmartObjectTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Prevent FindSmartObject() to query new objects while claimed.
	// This is done here because of the limited ways we can communicate between FindSmartObject() and ClaimSmartObject().
	// InteractionCooldownEndTime is used by the FindSmartObject() to invalidate the candidates.
	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	// Check that the claimed slot is still valid, and if not, fail the task.
	// The slot can become invalid if the whole SO or slot becomes invalidated.
	// In this case, we don't consider the slot as used and don't update the MRU slots
	if (InstanceData.ClaimedSlot.IsValid())
	{
		const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		if (!SmartObjectSubsystem.IsClaimedSmartObjectValid(InstanceData.ClaimedSlot))
		{
			InstanceData.ClaimedSlot.Invalidate();
			InstanceData.ClaimedSlotTransform = FTransform::Identity;
		}
	}

	return InstanceData.ClaimedSlot.IsValid() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}
