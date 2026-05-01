// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcBuildingProductionGateProcessor.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "Items/ArcItemSpec.h"
#include "MassExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "Mass/ArcAreaFragments.h"
#include "ArcAreaTypes.h"

UArcBuildingProductionGateProcessor::UArcBuildingProductionGateProcessor()
	: BuildingQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcBuildingProductionGateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	BuildingQuery.AddRequirement<FArcBuildingFragment>(EMassFragmentAccess::ReadOnly);
	BuildingQuery.AddRequirement<FArcBuildingWorkforceFragment>(EMassFragmentAccess::ReadWrite);
	BuildingQuery.AddRequirement<FArcCraftQueueFragment>(EMassFragmentAccess::ReadWrite);
	BuildingQuery.AddRequirement<FArcCraftInputFragment>(EMassFragmentAccess::ReadOnly);
	BuildingQuery.AddRequirement<FArcAreaFragment>(EMassFragmentAccess::ReadOnly);
	BuildingQuery.AddTagRequirement<FArcCraftAutoTickTag>(EMassFragmentPresence::All);
	BuildingQuery.AddSharedRequirement<FArcBuildingEconomyConfig>(EMassFragmentAccess::ReadOnly);
}

void UArcBuildingProductionGateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcAreaSubsystem* AreaSubsystem = Context.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!AreaSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcBuildingProductionGate);

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	BuildingQuery.ForEachEntityChunk(Context, [DeltaTime, AreaSubsystem](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcAreaFragment> Areas = Ctx.GetFragmentView<FArcAreaFragment>();
		const TConstArrayView<FArcBuildingFragment> Buildings = Ctx.GetFragmentView<FArcBuildingFragment>();
		TArrayView<FArcBuildingWorkforceFragment> Workforces = Ctx.GetMutableFragmentView<FArcBuildingWorkforceFragment>();
		TArrayView<FArcCraftQueueFragment> Queues = Ctx.GetMutableFragmentView<FArcCraftQueueFragment>();
		const TConstArrayView<FArcCraftInputFragment> Inputs = Ctx.GetFragmentView<FArcCraftInputFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();
		const FArcBuildingEconomyConfig& EconomyConfig = Ctx.GetSharedFragment<FArcBuildingEconomyConfig>();

		// Gathering buildings don't use craft queues — skip entirely.
		if (EconomyConfig.IsGatheringBuilding())
		{
			return;
		}

		const FGameplayTagContainer& WorkerActivityTags = EconomyConfig.WorkerActivityTags;

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const FArcAreaHandle AreaHandle = Areas[EntityIndex].AreaHandle;
			const FArcAreaData* AreaData = AreaSubsystem->GetAreaData(AreaHandle);
			FArcBuildingWorkforceFragment& Workforce = Workforces[EntityIndex];
			FArcCraftQueueFragment& Queue = Queues[EntityIndex];
			const FArcBuildingFragment& Building = Buildings[EntityIndex];
			const bool bOutputBufferFull = Building.CurrentOutputCount >= EconomyConfig.OutputBufferSize;

			// Count total active (staffed) area slots for this building
			int32 TotalActiveSlots = 0;
			if (AreaData)
			{
				const bool bHasWorkerFilter = !WorkerActivityTags.IsEmpty();
				for (int32 SlotIdx = 0; SlotIdx < AreaData->Slots.Num(); ++SlotIdx)
				{
					const FArcAreaSlotHandle SlotHandle(AreaHandle, SlotIdx);
					if (AreaSubsystem->GetSlotState(SlotHandle) != EArcAreaSlotState::Active)
					{
						continue;
					}
					if (bHasWorkerFilter && !AreaData->Slots[SlotIdx].ActivityTags.HasAll(WorkerActivityTags))
					{
						continue;
					}
					++TotalActiveSlots;
				}
			}

			int32 WorkersNeeded = 0;
			for (int32 SlotIndex = 0; SlotIndex < Workforce.Slots.Num(); ++SlotIndex)
			{
				FArcBuildingSlotData& Slot = Workforce.Slots[SlotIndex];

				if (!Slot.DesiredRecipe)
				{
					continue;
				}

				WorkersNeeded += Slot.RequiredWorkerCount;
				const bool bStaffed = TotalActiveSlots >= WorkersNeeded;

				bool bEntryExists = false;
				bool bEntryActive = false;
				for (const FArcCraftQueueEntry& Entry : Queue.Entries)
				{
					if (Entry.Recipe == Slot.DesiredRecipe && Entry.Priority == SlotIndex)
					{
						bEntryExists = true;
						bEntryActive = (Entry.State == EArcCraftQueueEntryState::Active);
						break;
					}
				}

				// Active entry means craft is in progress — reset halt and skip checks
				if (bEntryActive)
				{
					Slot.HaltDuration = 0.0f;
					continue;
				}

				bool bHasInputs = false;
				if (Slot.DesiredRecipe)
				{
					TArray<FArcItemSpec> ItemsCopy = Inputs[EntityIndex].InputItems;
					bHasInputs = FArcCraftItemSource::MatchAndConsumeFromSpecs(
						ItemsCopy, Slot.DesiredRecipe, false);
				}
				const bool bCanProduce = bStaffed && bHasInputs && !bOutputBufferFull;

				if (bCanProduce && !bEntryExists)
				{
					FArcCraftQueueEntry NewEntry;
					NewEntry.Recipe = Slot.DesiredRecipe;
					NewEntry.Amount = 1;
					NewEntry.CompletedAmount = 0;
					NewEntry.Priority = SlotIndex;
					NewEntry.ElapsedTickTime = 0.0f;
					NewEntry.State = EArcCraftQueueEntryState::Pending;
					Queue.Entries.Add(NewEntry);
					Slot.HaltDuration = 0.0f;
				}
				else if (!bCanProduce && bEntryExists)
				{
					// Only remove Pending entries — Active entries are owned by CraftTick
					Queue.Entries.RemoveAll([&Slot, SlotIndex](const FArcCraftQueueEntry& Entry)
					{
						return Entry.Recipe == Slot.DesiredRecipe
							&& Entry.Priority == SlotIndex
							&& Entry.State == EArcCraftQueueEntryState::Pending;
					});
					if (Queue.ActiveEntryIndex >= Queue.Entries.Num())
					{
						Queue.ActiveEntryIndex = INDEX_NONE;
					}
				}

				if (!bCanProduce)
				{
					Slot.HaltDuration += DeltaTime;
				}
			}
		}
	});
}
