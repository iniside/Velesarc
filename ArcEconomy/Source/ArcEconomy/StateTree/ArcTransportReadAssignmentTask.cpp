// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcTransportReadAssignmentTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "SmartObjectSubsystem.h"
#include "MassEntityView.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"

namespace ArcTransportReadAssignment
{
	void ResolveCandidateSlots(
		const FMassEntityManager& EntityManager,
		USmartObjectSubsystem& SOSubsystem,
		FMassEntityHandle BuildingHandle,
		const FGameplayTagQuery& SlotRequirements,
		FMassSmartObjectCandidateSlots& OutCandidateSlots,
		bool& bOutHasSlots)
	{
		OutCandidateSlots.Reset();
		bOutHasSlots = false;

		if (!BuildingHandle.IsValid())
		{
			return;
		}

		const FMassEntityView EntityView(EntityManager, BuildingHandle);
		const FArcSmartObjectOwnerFragment* OwnerFragment = EntityView.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>();
		if (!OwnerFragment || !OwnerFragment->SmartObjectHandle.IsValid())
		{
			return;
		}

		TArray<FSmartObjectSlotHandle> AllSlots;
		SOSubsystem.GetAllSlots(OwnerFragment->SmartObjectHandle, AllSlots);

		const bool bHasFilter = !SlotRequirements.IsEmpty();
		uint8 SlotCount = 0;

		for (const FSmartObjectSlotHandle& SlotHandle : AllSlots)
		{
			if (SlotCount >= FMassSmartObjectCandidateSlots::MaxNumCandidates)
			{
				break;
			}

			if (bHasFilter)
			{
				const FGameplayTagContainer& SlotTags = SOSubsystem.GetSlotTags(SlotHandle);
				if (!SlotRequirements.Matches(SlotTags))
				{
					continue;
				}
			}

			FSmartObjectCandidateSlot& Candidate = OutCandidateSlots.Slots[SlotCount];
			Candidate.Result = FSmartObjectRequestResult(OwnerFragment->SmartObjectHandle, SlotHandle);
			Candidate.Cost = 0.f;
			++SlotCount;
		}

		OutCandidateSlots.NumSlots = SlotCount;
		bOutHasSlots = (SlotCount > 0);
	}
}

bool FArcTransportReadAssignmentTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransporterFragmentHandle);
	return true;
}

EStateTreeRunStatus FArcTransportReadAssignmentTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FArcTransportReadAssignmentTaskInstanceData& InstanceData = Context.GetInstanceData(*this);
	const FArcTransporterFragment& Transporter = Context.GetExternalData(TransporterFragmentHandle);

	if (Transporter.TaskState == EArcTransporterTaskState::Idle || !Transporter.SourceBuildingHandle.IsValid())
	{
		InstanceData.bHasAssignment = false;
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.SourceBuildingHandle = Transporter.SourceBuildingHandle;
	InstanceData.DestinationBuildingHandle = Transporter.DestinationBuildingHandle;
	InstanceData.ItemDefinition = Transporter.TargetItemDefinition;
	InstanceData.Quantity = Transporter.TargetQuantity;
	InstanceData.bHasAssignment = true;

	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FMassEntityManager& EntityManager = MassContext.GetEntityManager();

	if (SOSubsystem)
	{
		ArcTransportReadAssignment::ResolveCandidateSlots(
			EntityManager, *SOSubsystem,
			InstanceData.SourceBuildingHandle, SourceSlotRequirements,
			InstanceData.SourceCandidateSlots, InstanceData.bHasSourceSlots);

		ArcTransportReadAssignment::ResolveCandidateSlots(
			EntityManager, *SOSubsystem,
			InstanceData.DestinationBuildingHandle, DestinationSlotRequirements,
			InstanceData.DestinationCandidateSlots, InstanceData.bHasDestinationSlots);
	}

	return EStateTreeRunStatus::Succeeded;
}
