// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcResolveBuildingInfoTask.h"
#include "Mass/ArcAreaAssignmentFragments.h"
#include "ArcAreaSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRuntime.h"

bool FArcResolveBuildingInfoTask::Link(FStateTreeLinker& Linker)
{
    Linker.LinkExternalData(AssignmentFragmentHandle);
    return true;
}

EStateTreeRunStatus FArcResolveBuildingInfoTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
    FArcResolveBuildingInfoTaskInstanceData& InstanceData = Context.GetInstanceData(*this);

    const FArcAreaAssignmentFragment& Assignment = Context.GetExternalData(AssignmentFragmentHandle);

    if (!Assignment.IsAssigned())
    {
        return EStateTreeRunStatus::Failed;
    }

    UArcAreaSubsystem* AreaSubsystem = Context.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
    if (!AreaSubsystem)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FArcAreaData* AreaData = AreaSubsystem->GetAreaData(Assignment.SlotHandle.AreaHandle);
    if (!AreaData)
    {
        return EStateTreeRunStatus::Failed;
    }

    InstanceData.BuildingHandle = AreaData->OwnerEntity;
    InstanceData.AreaHandle = AreaData->Handle;
    InstanceData.SlotHandle = Assignment.SlotHandle;
    InstanceData.SlotLocation = AreaData->Location;

    USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();
    if (SOSubsystem && AreaData->SmartObjectHandle.IsValid())
    {
        TArray<FSmartObjectSlotHandle> AllSlots;
        SOSubsystem->GetAllSlots(AreaData->SmartObjectHandle, AllSlots);

        const bool bHasFilter = !SlotRequirements.IsEmpty();

        InstanceData.CandidateSlots.Reset();
        uint8 CandidateCount = 0;
        for (const FSmartObjectSlotHandle& SlotH : AllSlots)
        {
            if (CandidateCount >= FMassSmartObjectCandidateSlots::MaxNumCandidates)
            {
                break;
            }

            if (bHasFilter)
            {
            	const FGameplayTagContainer& SlotTags = SOSubsystem->GetSlotTags(SlotH);
            	if (!SlotRequirements.Matches(SlotTags))
            	{
            		continue;
            	}
            }

            FSmartObjectCandidateSlot& Candidate = InstanceData.CandidateSlots.Slots[CandidateCount];
            Candidate.Result = FSmartObjectRequestResult(AreaData->SmartObjectHandle, SlotH);
            Candidate.Cost = 0.f;
            ++CandidateCount;
        }
        InstanceData.CandidateSlots.NumSlots = CandidateCount;
    }

    return EStateTreeRunStatus::Succeeded;
}
