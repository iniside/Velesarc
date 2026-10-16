// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcGatherSchedulerProcessor.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcBodyProcessor.h"
#include "Sim/ArcBodySlotSelection.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcItemInteractionFragment.h"
#include "Sim/ArcMatchProcessor.h"
#include "Sim/ArcWorldSimTags.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcGatherSchedulerProcessor)

namespace ArcGatherSchedulerPrivate
{
struct FGatherArea
{
	FMassEntityHandle Entity;
	FVector Location = FVector::ZeroVector;
	FGameplayTag AreaTag;
	FPrimaryAssetId Item;
	int32 AmountPerVisit = 0;
};

const FGatherArea* FindNearestArea(
	const TArray<FGatherArea>& Areas,
	const FGameplayTag AreaTag,
	const FVector& FromLocation)
{
	const FGatherArea* Nearest = nullptr;
	float BestDistanceSquared = 0.0f;
	for (const FGatherArea& Area : Areas)
	{
		if (Area.AreaTag != AreaTag)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(FromLocation, Area.Location);
		if (!Nearest || DistanceSquared < BestDistanceSquared)
		{
			Nearest = &Area;
			BestDistanceSquared = DistanceSquared;
		}
	}
	return Nearest;
}

int32 CountAllItems(const FArcMassItemStoreFragment& Store)
{
	int32 Count = 0;
	for (const FArcMassReplicatedItem& Entry : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Entry.ToItem();
		if (Data)
		{
			Count += static_cast<int32>(Data->GetStacks());
		}
	}
	return Count;
}

int32 CountItem(const FArcMassItemStoreFragment& Store, const FPrimaryAssetId& Item)
{
	int32 Count = 0;
	for (const FArcMassReplicatedItem& Entry : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Entry.ToItem();
		if (Data && Data->GetItemDefinitionId() == Item)
		{
			Count += static_cast<int32>(Data->GetStacks());
		}
	}
	return Count;
}

void QueueItemInteraction(
	FMassExecutionContext& Context,
	const FMassEntityHandle Entity,
	const uint32 Generation,
	const uint32 CommandSerial,
	const FPrimaryAssetId& Item,
	const int32 RequestedQuantity)
{
	Context.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[Entity, Generation, CommandSerial, Item, RequestedQuantity](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityValid(Entity))
			{
				return;
			}

			FStructView View = EntityManager.GetMutableSparseElementDataForEntity(
				FArcItemInteractionFragment::StaticStruct(), Entity);
			FArcItemInteractionFragment* Interaction = View.GetPtr<FArcItemInteractionFragment>();
			if (Interaction && Interaction->CommandSerial == CommandSerial)
			{
				return;
			}
			if (!Interaction)
			{
				Interaction = &EntityManager.AddSparseElementToEntity<FArcItemInteractionFragment>(Entity);
			}

			Interaction->Generation = Generation;
			Interaction->CommandSerial = CommandSerial;
			Interaction->Item = Item;
			Interaction->RequestedQuantity = RequestedQuantity;
			Interaction->ProcessedQuantity = 0;
		});
}

void QueueRemoveItemInteraction(
	FMassExecutionContext& Context,
	const FMassEntityHandle Entity,
	const uint32 Generation,
	const uint32 CommandSerial)
{
	Context.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>(
		[Entity, Generation, CommandSerial](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityValid(Entity))
			{
				return;
			}

			FStructView View = EntityManager.GetMutableSparseElementDataForEntity(
				FArcItemInteractionFragment::StaticStruct(), Entity);
			const FArcItemInteractionFragment* Interaction = View.GetPtr<FArcItemInteractionFragment>();
			if (Interaction
				&& Interaction->Generation == Generation
				&& Interaction->CommandSerial == CommandSerial)
			{
				EntityManager.RemoveSparseElementFromEntity<FArcItemInteractionFragment>(Entity);
			}
		});
}

void ResetToGather(FArcGatherAssignmentFragment& Assignment)
{
	Assignment.GatherArea = FMassEntityHandle();
	Assignment.Item = FPrimaryAssetId();
	Assignment.Quantity = 0;
	Assignment.Phase = EArcGatherAssignmentPhase::Gather;
}
}

UArcGatherSchedulerProcessor::UArcGatherSchedulerProcessor()
	: BodyQuery(*this)
	, AreaQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcMatchProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UArcBodyProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UArcGatherSchedulerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<USmartObjectSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	BodyQuery.Initialize(EntityManager);
	BodyQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.AddRequirement<FArcGatherAssignmentFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.AddRequirement<FArcCarryCapabilityFragment>(EMassFragmentAccess::ReadOnly);
	BodyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	BodyQuery.AddRequirement<FMassSmartObjectUserFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.AddSparseRequirement<FArcMassItemStoreFragment>(EMassFragmentPresence::All);
	BodyQuery.RegisterWithProcessor(*this);

	AreaQuery.Initialize(EntityManager);
	AreaQuery.AddConstSharedRequirement<FArcGatherAreaConfigFragment>(EMassFragmentPresence::All);
	AreaQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	AreaQuery.RegisterWithProcessor(*this);
}

void UArcGatherSchedulerProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	USmartObjectSubsystem* SmartObjectSubsystem = Context.GetMutableSubsystem<USmartObjectSubsystem>();
	UMassSignalSubsystem* SignalSubsystem = Context.GetMutableSubsystem<UMassSignalSubsystem>();
	if (!SmartObjectSubsystem || !SignalSubsystem)
	{
		return;
	}

	TArray<ArcGatherSchedulerPrivate::FGatherArea> Areas;
	AreaQuery.ForEachEntityChunk(Context, [&Areas](FMassExecutionContext& ChunkContext)
	{
		const FArcGatherAreaConfigFragment& Config =
			ChunkContext.GetConstSharedFragment<FArcGatherAreaConfigFragment>();
		const TConstArrayView<FTransformFragment> Transforms =
			ChunkContext.GetFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			ArcGatherSchedulerPrivate::FGatherArea Area;
			Area.Entity = ChunkContext.GetEntity(EntityIt);
			Area.Location = Transforms[EntityIt].GetTransform().GetLocation();
			Area.AreaTag = Config.AreaTag;
			Area.Item = Config.ItemDefinition;
			Area.AmountPerVisit = Config.AmountPerVisit;
			Areas.Add(Area);
		}
	});

	BodyQuery.ForEachEntityChunk(Context,
		[&EntityManager, SmartObjectSubsystem, SignalSubsystem, &Areas](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			TArrayView<FArcGatherAssignmentFragment> Assignments =
				ChunkContext.GetMutableFragmentView<FArcGatherAssignmentFragment>();
			const TConstArrayView<FArcCarryCapabilityFragment> Capabilities =
				ChunkContext.GetFragmentView<FArcCarryCapabilityFragment>();
			const TConstArrayView<FTransformFragment> Transforms =
				ChunkContext.GetFragmentView<FTransformFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcBodyFragment& Body = Bodies[EntityIt];
				FArcGatherAssignmentFragment& Assignment = Assignments[EntityIt];
				if (!Assignment.Workplace.IsValid())
				{
					continue;
				}

				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				FArcMassItemStoreFragment* BodyStore =
					ChunkContext.GetMutableSparseElement<FArcMassItemStoreFragment>(EntityIt);
				if (!BodyStore)
				{
					continue;
				}

				if (Assignment.ActiveCommandSerial != 0)
				{
					const uint32 CommandSerial = Assignment.ActiveCommandSerial;
					if (Body.Command.IsActive() && Body.Command.Serial == CommandSerial)
					{
						continue;
					}
					if (Body.LastCommandSerial != CommandSerial)
					{
						continue;
					}

					FStructView InteractionView = EntityManager.GetMutableSparseElementDataForEntity(
						FArcItemInteractionFragment::StaticStruct(), Entity);
					const FArcItemInteractionFragment* Interaction =
						InteractionView.GetPtr<FArcItemInteractionFragment>();
					const bool bMatchingInteraction = Interaction && Interaction->CommandSerial == CommandSerial;
					const EArcBodyCommandResult Result = Body.LastCommandResult;
					const FPrimaryAssetId RequestedItem = bMatchingInteraction
						? Interaction->Item
						: FPrimaryAssetId();
					const int32 ProcessedQuantity = bMatchingInteraction
						? Interaction->ProcessedQuantity
						: 0;
					if (bMatchingInteraction)
					{
						ArcGatherSchedulerPrivate::QueueRemoveItemInteraction(
							ChunkContext,
							Entity,
							Interaction->Generation,
							CommandSerial);
					}
					Assignment.ActiveCommandSerial = 0;

					if (Assignment.Phase == EArcGatherAssignmentPhase::Gather)
					{
						if (Result == EArcBodyCommandResult::Succeeded
							&& RequestedItem.IsValid()
							&& ProcessedQuantity > 0)
						{
							Assignment.Item = RequestedItem;
							Assignment.Quantity = ProcessedQuantity;
							Assignment.Phase = EArcGatherAssignmentPhase::Drop;
						}
					}
					else
					{
						const int32 Remaining = Assignment.Item.IsValid()
							? ArcGatherSchedulerPrivate::CountItem(*BodyStore, Assignment.Item)
							: 0;
						Assignment.Quantity = Remaining;
						if (Remaining == 0)
						{
							ArcGatherSchedulerPrivate::ResetToGather(Assignment);
						}
					}
					continue;
				}

				if (Body.State != EArcBodyState::Idle || Body.Command.IsActive())
				{
					continue;
				}

				FSmartObjectRequestResult RequestResult{FSmartObjectHandle()};
				FMassEntityHandle Target;
				FPrimaryAssetId Item;
				int32 RequestedQuantity = 0;
				FGameplayTag SlotTag;
				if (Assignment.Phase == EArcGatherAssignmentPhase::Gather)
				{
					if (!EntityManager.IsEntityValid(Assignment.Workplace))
					{
						continue;
					}
					const FMassEntityView WorkplaceView(EntityManager, Assignment.Workplace);
					const FArcGatheringConfigFragment* GatheringConfig =
						WorkplaceView.GetConstSharedFragmentDataPtr<FArcGatheringConfigFragment>();
					if (!GatheringConfig)
					{
						continue;
					}

					const ArcGatherSchedulerPrivate::FGatherArea* Area =
						ArcGatherSchedulerPrivate::FindNearestArea(
							Areas,
							GatheringConfig->AreaTag,
							Transforms[EntityIt].GetTransform().GetLocation());
					const int32 FreeCapacity = FMath::Max(
						0,
						Capabilities[EntityIt].Capacity - ArcGatherSchedulerPrivate::CountAllItems(*BodyStore));
					if (!Area || !Area->Item.IsValid() || FreeCapacity <= 0)
					{
						continue;
					}

					Target = Area->Entity;
					Item = Area->Item;
					RequestedQuantity = FMath::Min(Area->AmountPerVisit, FreeCapacity);
					SlotTag = ArcWorldSimTags::Slot_Gather;
				}
				else
				{
					const int32 Present = Assignment.Item.IsValid()
						? ArcGatherSchedulerPrivate::CountItem(*BodyStore, Assignment.Item)
						: 0;
					if (Present <= 0)
					{
						ArcGatherSchedulerPrivate::ResetToGather(Assignment);
						continue;
					}
					if (!EntityManager.IsEntityValid(Assignment.Workplace))
					{
						continue;
					}

					const FMassEntityView WorkplaceView(EntityManager, Assignment.Workplace);
					const FArcGatheringConfigFragment* GatheringConfig =
						WorkplaceView.GetConstSharedFragmentDataPtr<FArcGatheringConfigFragment>();
					const FArcBuildingOutputStore* OutputStore =
						WorkplaceView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>();
					if (!GatheringConfig || !OutputStore)
					{
						continue;
					}
					const int32 FreeOutputCapacity = FMath::Max(
						0,
						GatheringConfig->OutputCapacity - ArcGatherSchedulerPrivate::CountAllItems(*OutputStore));
					if (FreeOutputCapacity <= 0)
					{
						continue;
					}

					Target = Assignment.Workplace;
					Item = Assignment.Item;
					RequestedQuantity = FMath::Min(Present, FreeOutputCapacity);
					Assignment.Quantity = Present;
					SlotTag = ArcWorldSimTags::Slot_Drop;
				}

				if (RequestedQuantity <= 0
					|| !ArcWorldSim::FindAvailableSlot(
						EntityManager,
						*SmartObjectSubsystem,
						Target,
						SlotTag,
						RequestResult)
					|| !ArcWorldSim::TryAssignSlot(
						EntityManager,
						ChunkContext,
						*SmartObjectSubsystem,
						*SignalSubsystem,
						Entity,
						Body,
						Users[EntityIt],
						Target,
						RequestResult))
				{
					continue;
				}

				if (Assignment.Phase == EArcGatherAssignmentPhase::Gather)
				{
					Assignment.GatherArea = Target;
				}
				Assignment.ActiveCommandSerial = Body.Command.Serial;
				ArcGatherSchedulerPrivate::QueueItemInteraction(
					ChunkContext,
					Entity,
					ArcWorldSim::NextItemInteractionGeneration(),
					Body.Command.Serial,
					Item,
					RequestedQuantity);
			}
		});
}
