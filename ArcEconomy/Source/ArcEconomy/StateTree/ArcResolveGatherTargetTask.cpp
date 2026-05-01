// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcResolveGatherTargetTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "SmartObjectSubsystem.h"
#include "MassEntityView.h"
#include "Mass/EntityFragments.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"

namespace ArcResolveGatherTarget
{
    void ResolveCandidateSlots(
        const FMassEntityManager& EntityManager,
        USmartObjectSubsystem& SOSubsystem,
        FMassEntityHandle EntityHandle,
        const FGameplayTagQuery& SlotRequirements,
        FMassSmartObjectCandidateSlots& OutCandidateSlots,
        bool& bOutHasSlots)
    {
        OutCandidateSlots.Reset();
        bOutHasSlots = false;

        if (!EntityHandle.IsValid())
        {
            return;
        }

        const FMassEntityView EntityView(EntityManager, EntityHandle);
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

bool FArcResolveGatherTargetTask::Link(FStateTreeLinker& Linker)
{
    Linker.LinkExternalData(GathererFragmentHandle);
    return true;
}

EStateTreeRunStatus FArcResolveGatherTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FArcResolveGatherTargetTaskInstanceData& InstanceData = Context.GetInstanceData(*this);
    const FArcGathererFragment& Gatherer = Context.GetExternalData(GathererFragmentHandle);

    if (!Gatherer.TargetResourceHandle.IsValid() || !Gatherer.AssignedBuildingHandle.IsValid())
    {
        InstanceData.bHasAssignment = false;
        return EStateTreeRunStatus::Failed;
    }

    InstanceData.TargetResourceHandle = Gatherer.TargetResourceHandle;
    InstanceData.bHasAssignment = true;

    // Resolve target location
    FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
    const FMassEntityManager& EntityManager = MassContext.GetEntityManager();

    if (EntityManager.IsEntityValid(Gatherer.TargetResourceHandle))
    {
        const FMassEntityView ResourceView(EntityManager, Gatherer.TargetResourceHandle);
        const FTransformFragment* TransformFrag = ResourceView.GetFragmentDataPtr<FTransformFragment>();
        if (TransformFrag)
        {
            InstanceData.TargetLocation = TransformFrag->GetTransform().GetLocation();
        }
    }

    // Resolve SmartObject slots
    USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();
    if (SOSubsystem)
    {
        ArcResolveGatherTarget::ResolveCandidateSlots(
            EntityManager, *SOSubsystem,
            Gatherer.TargetResourceHandle, ResourceSlotRequirements,
            InstanceData.ResourceCandidateSlots, InstanceData.bHasResourceSlots);

        ArcResolveGatherTarget::ResolveCandidateSlots(
            EntityManager, *SOSubsystem,
            Gatherer.AssignedBuildingHandle, BuildingSlotRequirements,
            InstanceData.BuildingCandidateSlots, InstanceData.bHasBuildingSlots);
    }

    return EStateTreeRunStatus::Succeeded;
}
