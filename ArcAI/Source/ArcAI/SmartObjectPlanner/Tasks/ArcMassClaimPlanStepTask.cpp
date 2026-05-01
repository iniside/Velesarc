// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassClaimPlanStepTask.h"

#include "ArcAILogs.h"
#include "ArcKnowledgeSubsystem.h"
#include "MassAIBehaviorTypes.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectHandler.h"
#include "MassSmartObjectSettings.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"

FArcMassClaimPlanStepTask::FArcMassClaimPlanStepTask()
{
	bShouldStateChangeOnReselect = false;
	bShouldCallTick = true;
}

bool FArcMassClaimPlanStepTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectUserHandle);
	Linker.LinkExternalData(SmartObjectMRUSlotsHandle);
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	Linker.LinkExternalData(MassSignalSubsystemHandle);
	return true;
}

void FArcMassClaimPlanStepTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite(SmartObjectUserHandle);
	Builder.AddReadWrite(SmartObjectMRUSlotsHandle);
	Builder.AddReadOnly(SmartObjectSubsystemHandle);
	Builder.AddReadOnly(MassSignalSubsystemHandle);
}

EStateTreeRunStatus FArcMassClaimPlanStepTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.bClaimSucceeded = false;
	InstanceData.SmartObjectClaimHandle.Invalidate();
	InstanceData.ClaimedAdvertisementHandle = FArcKnowledgeHandle();

	const FMassEntityHandle Entity = MassCtx.GetEntity();

	// --- Knowledge path ---
	if (InstanceData.KnowledgeHandle.IsValid())
	{
		UWorld* World = MassCtx.GetWorld();
		if (!World)
		{
			return EStateTreeRunStatus::Failed;
		}

		UArcKnowledgeSubsystem* KnowledgeSubsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
		if (!KnowledgeSubsystem)
		{
			return EStateTreeRunStatus::Failed;
		}

		InstanceData.bClaimSucceeded = KnowledgeSubsystem->ClaimAdvertisement(InstanceData.KnowledgeHandle, Entity);
		if (InstanceData.bClaimSucceeded)
		{
			InstanceData.ClaimedAdvertisementHandle = InstanceData.KnowledgeHandle;
		}

		return InstanceData.bClaimSucceeded ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
	}

	// --- SmartObject path ---
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

	const FMassSmartObjectHandler MassSmartObjectHandler(MassCtx.GetMassEntityExecutionContext(), SmartObjectSubsystem, SignalSubsystem);

	InstanceData.SmartObjectClaimHandle = MassSmartObjectHandler.ClaimCandidate(Entity, SOUser, InstanceData.CandidateSlots);

	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	if (!InstanceData.SmartObjectClaimHandle.IsValid())
	{
		UE_LOG(LogGoalPlannerStep, Log, TEXT("ClaimPlanStep: Failed to claim SO slot from %d candidates"), InstanceData.CandidateSlots.NumSlots);
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bClaimSucceeded = true;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassClaimPlanStepTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Knowledge claims don't need tick validation
	if (InstanceData.ClaimedAdvertisementHandle.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	// SO claims can become invalid if the SO is destroyed
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	if (InstanceData.SmartObjectClaimHandle.IsValid())
	{
		const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		if (!SmartObjectSubsystem.IsClaimedSmartObjectValid(InstanceData.SmartObjectClaimHandle))
		{
			InstanceData.SmartObjectClaimHandle.Invalidate();
		}
	}

	return InstanceData.SmartObjectClaimHandle.IsValid() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

void FArcMassClaimPlanStepTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// --- Knowledge path cleanup ---
	if (InstanceData.ClaimedAdvertisementHandle.IsValid())
	{
		UWorld* World = MassCtx.GetWorld();
		UArcKnowledgeSubsystem* KnowledgeSubsystem = World ? World->GetSubsystem<UArcKnowledgeSubsystem>() : nullptr;
		if (KnowledgeSubsystem)
		{
			KnowledgeSubsystem->CancelAdvertisement(InstanceData.ClaimedAdvertisementHandle);
		}
		return;
	}

	// --- SmartObject path cleanup ---
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	if (InstanceData.SmartObjectClaimHandle.IsValid())
	{
		FMassExecutionContext& MassExecutionContext = MassCtx.GetMassEntityExecutionContext();
		const FMassEntityHandle Entity = MassCtx.GetEntity();

		if (UE::Mass::SmartObject::FMRUSlotsFragment* ExistingMRUSlotsFragment = Context.GetExternalDataPtr(SmartObjectMRUSlotsHandle))
		{
			ExistingMRUSlotsFragment->Slots.Push(InstanceData.SmartObjectClaimHandle.SlotHandle);
		}
		else
		{
			const UMassSmartObjectSettings* MassSmartObjectSettings = GetDefault<UMassSmartObjectSettings>();
			if (MassSmartObjectSettings != nullptr && MassSmartObjectSettings->MRUSlotsMaxCount > 0)
			{
				UE::Mass::SmartObject::FMRUSlotsFragment MRUSlotsFragment;
				MRUSlotsFragment.Slots.Push(InstanceData.SmartObjectClaimHandle.SlotHandle);
				MassExecutionContext.Defer().PushCommand<FMassCommandAddFragmentInstances>(Entity, MRUSlotsFragment);
			}
		}

		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler MassSmartObjectHandler(MassExecutionContext, SmartObjectSubsystem, SignalSubsystem);
		MassSmartObjectHandler.ReleaseSmartObject(Entity, SOUser, InstanceData.SmartObjectClaimHandle);
	}
}

#if WITH_EDITOR
FText FArcMassClaimPlanStepTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcAI", "ClaimPlanStepDesc", "Claim Plan Step (SO or Knowledge)");
}
#endif
