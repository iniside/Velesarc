// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcTransportJobProcessor.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "Operations/ArcMassItemOperations.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcBodyProcessor.h"
#include "Sim/ArcBodySlotSelection.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcItemInteractionFragment.h"
#include "Sim/ArcMatchProcessor.h"
#include "Sim/ArcSettlementLedgerSubsystem.h"
#include "Sim/ArcTransportJobFragment.h"
#include "Sim/ArcWorldSimStores.h"
#include "Sim/ArcWorldSimTags.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTransportJobProcessor)

namespace ArcTransportJobProcessorPrivate
{
struct FStorageTarget
{
	FMassEntityHandle Entity;
	int32 FreeSpace = 0;
};

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
	return Item.IsValid() ? ArcMassItems::CountItemsByDefinition(Store, Item) : 0;
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
			if (Interaction && Interaction->Generation == Generation
				&& Interaction->CommandSerial == CommandSerial)
			{
				EntityManager.RemoveSparseElementFromEntity<FArcItemInteractionFragment>(Entity);
			}
		});
}

void QueueRemoveJob(
	FMassExecutionContext& Context,
	const FMassEntityHandle Entity,
	const uint32 Generation)
{
	Context.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>(
		[Entity, Generation](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityValid(Entity))
			{
				return;
			}

			FStructView View = EntityManager.GetMutableSparseElementDataForEntity(
				FArcTransportJobFragment::StaticStruct(), Entity);
			const FArcTransportJobFragment* Job = View.GetPtr<FArcTransportJobFragment>();
			if (Job && Job->Generation == Generation && Job->ActiveCommandSerial == 0)
			{
				EntityManager.RemoveSparseElementFromEntity<FArcTransportJobFragment>(Entity);
			}
		});
}

bool IsCompatibleSink(FMassEntityManager& EntityManager, const FMassEntityHandle Sink)
{
	if (!EntityManager.IsEntityValid(Sink))
	{
		return false;
	}

	const FMassEntityView SinkView(EntityManager, Sink);
	if (SinkView.GetSparseFragmentDataPtr<FArcBuildingInputStore>())
	{
		return true;
	}

	const FArcStorageConfigFragment* Config =
		SinkView.GetConstSharedFragmentDataPtr<FArcStorageConfigFragment>();
	const FArcBuildingOutputStore* Store =
		SinkView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>();
	return Config && Store && CountAllItems(*Store) < Config->Capacity;
}

FMassEntityHandle FindRetarget(
	FMassEntityManager& EntityManager,
	TArray<FArcNeedRow>& Needs,
	TArray<FStorageTarget>& Storages,
	const FPrimaryAssetId& Item,
	const int32 Quantity)
{
	for (FArcNeedRow& Need : Needs)
	{
		if (Need.Quantity <= 0 || !Need.Building.IsValid()
			|| !Need.AcceptableItems.Contains(Item)
			|| !IsCompatibleSink(EntityManager, Need.Building))
		{
			continue;
		}

		Need.Quantity = FMath::Max(0, Need.Quantity - Quantity);
		return Need.Building;
	}

	for (FStorageTarget& Storage : Storages)
	{
		if (Storage.FreeSpace <= 0 || !EntityManager.IsEntityValid(Storage.Entity))
		{
			continue;
		}

		Storage.FreeSpace = FMath::Max(0, Storage.FreeSpace - Quantity);
		return Storage.Entity;
	}

	return FMassEntityHandle();
}
}

UArcTransportJobProcessor::UArcTransportJobProcessor()
	: BodyQuery(*this)
	, StorageQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcMatchProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UArcBodyProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UArcTransportJobProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UArcSettlementLedgerSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<USmartObjectSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	BodyQuery.Initialize(EntityManager);
	BodyQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.AddRequirement<FArcCarrierAssignmentFragment>(EMassFragmentAccess::ReadOnly);
	BodyQuery.AddRequirement<FArcCarryCapabilityFragment>(EMassFragmentAccess::ReadOnly);
	BodyQuery.AddRequirement<FMassSmartObjectUserFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.AddSparseRequirement<FArcMassItemStoreFragment>(EMassFragmentPresence::All);
	BodyQuery.AddSparseRequirement<FArcTransportJobFragment>(EMassFragmentPresence::All);
	BodyQuery.RegisterWithProcessor(*this);

	StorageQuery.Initialize(EntityManager);
	StorageQuery.AddConstSharedRequirement<FArcStorageConfigFragment>(EMassFragmentPresence::All);
	StorageQuery.AddSparseRequirement<FArcBuildingOutputStore>(EMassFragmentPresence::All);
	StorageQuery.RegisterWithProcessor(*this);
}

void UArcTransportJobProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UArcSettlementLedgerSubsystem* Ledger = Context.GetMutableSubsystem<UArcSettlementLedgerSubsystem>();
	USmartObjectSubsystem* SmartObjectSubsystem = Context.GetMutableSubsystem<USmartObjectSubsystem>();
	UMassSignalSubsystem* SignalSubsystem = Context.GetMutableSubsystem<UMassSignalSubsystem>();
	if (!Ledger || !SmartObjectSubsystem || !SignalSubsystem)
	{
		return;
	}

	TArray<FArcNeedRow> Needs = Ledger->Needs;
	TMap<FMassEntityHandle, int32> DropReservationsBySink;
	BodyQuery.ForEachEntityChunk(Context, [&DropReservationsBySink](FMassExecutionContext& ChunkContext)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcMassItemStoreFragment* BodyStore =
				ChunkContext.GetMutableSparseElement<FArcMassItemStoreFragment>(EntityIt);
			const FArcTransportJobFragment* Job =
				ChunkContext.GetMutableSparseElement<FArcTransportJobFragment>(EntityIt);
			if (BodyStore && Job && Job->Phase == EArcTransportJobPhase::Drop && Job->Sink.IsValid())
			{
				DropReservationsBySink.FindOrAdd(Job->Sink) +=
					ArcTransportJobProcessorPrivate::CountItem(*BodyStore, Job->Item);
			}
		}
	});

	TArray<ArcTransportJobProcessorPrivate::FStorageTarget> Storages;
	StorageQuery.ForEachEntityChunk(Context,
		[&Storages, &DropReservationsBySink](FMassExecutionContext& ChunkContext)
	{
		const FArcStorageConfigFragment& Config =
			ChunkContext.GetConstSharedFragment<FArcStorageConfigFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcBuildingOutputStore* Store =
				ChunkContext.GetMutableSparseElement<FArcBuildingOutputStore>(EntityIt);
			if (!Store)
			{
				continue;
			}

			ArcTransportJobProcessorPrivate::FStorageTarget Target;
			Target.Entity = ChunkContext.GetEntity(EntityIt);
			const int32* Reserved = DropReservationsBySink.Find(Target.Entity);
			Target.FreeSpace = FMath::Max(
				0,
				Config.Capacity
					- ArcTransportJobProcessorPrivate::CountAllItems(*Store)
					- (Reserved ? *Reserved : 0));
			Storages.Add(Target);
		}
		});

	BodyQuery.ForEachEntityChunk(Context,
		[&EntityManager, SmartObjectSubsystem, SignalSubsystem, &Needs, &Storages](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			const TConstArrayView<FArcCarrierAssignmentFragment> Assignments =
				ChunkContext.GetFragmentView<FArcCarrierAssignmentFragment>();
			const TConstArrayView<FArcCarryCapabilityFragment> Capabilities =
				ChunkContext.GetFragmentView<FArcCarryCapabilityFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				if (!Assignments[EntityIt].bAssigned)
				{
					continue;
				}

				FArcBodyFragment& Body = Bodies[EntityIt];
				FArcMassItemStoreFragment* BodyStore =
					ChunkContext.GetMutableSparseElement<FArcMassItemStoreFragment>(EntityIt);
				FArcTransportJobFragment* Job =
					ChunkContext.GetMutableSparseElement<FArcTransportJobFragment>(EntityIt);
				if (!BodyStore || !Job || !Job->Item.IsValid() || Job->Quantity <= 0)
				{
					continue;
				}

				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				if (Job->ActiveCommandSerial != 0)
				{
					const uint32 CommandSerial = Job->ActiveCommandSerial;
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
					const bool bMatchingInteraction =
						Interaction && Interaction->CommandSerial == CommandSerial;
					const int32 ProcessedQuantity = bMatchingInteraction
						? Interaction->ProcessedQuantity
						: 0;
					if (bMatchingInteraction)
					{
						ArcTransportJobProcessorPrivate::QueueRemoveItemInteraction(
							ChunkContext,
							Entity,
							Interaction->Generation,
							CommandSerial);
					}
					Job->ActiveCommandSerial = 0;

					if (Job->Phase == EArcTransportJobPhase::Pickup)
					{
						if (Body.LastCommandResult == EArcBodyCommandResult::Succeeded
							&& ProcessedQuantity > 0)
						{
							Job->Quantity = ArcTransportJobProcessorPrivate::CountItem(*BodyStore, Job->Item);
							Job->Phase = EArcTransportJobPhase::Drop;
						}
					}
					else
					{
						const int32 Remaining =
							ArcTransportJobProcessorPrivate::CountItem(*BodyStore, Job->Item);
						Job->Quantity = Remaining;
						if (Remaining == 0)
						{
							ArcTransportJobProcessorPrivate::QueueRemoveJob(
								ChunkContext, Entity, Job->Generation);
						}
					}
					continue;
				}

				if (Body.State != EArcBodyState::Idle || Body.Command.IsActive())
				{
					continue;
				}

				FMassEntityHandle Target;
				FGameplayTag SlotTag;
				int32 RequestedQuantity = 0;
				if (Job->Phase == EArcTransportJobPhase::Pickup)
				{
					const int32 CargoCount = ArcTransportJobProcessorPrivate::CountItem(*BodyStore, Job->Item);
					if (CargoCount > 0)
					{
						Job->Quantity = CargoCount;
						Job->Phase = EArcTransportJobPhase::Drop;
						continue;
					}

					const int32 FreeCapacity = FMath::Max(
						0,
						Capabilities[EntityIt].Capacity
							- ArcTransportJobProcessorPrivate::CountAllItems(*BodyStore));
					if (FreeCapacity <= 0 || !EntityManager.IsEntityValid(Job->Source))
					{
						ArcTransportJobProcessorPrivate::QueueRemoveJob(
							ChunkContext, Entity, Job->Generation);
						continue;
					}

					const FMassEntityView SourceView(EntityManager, Job->Source);
					const FArcItemSourceConfigFragment* ItemSourceConfig =
						SourceView.GetConstSharedFragmentDataPtr<FArcItemSourceConfigFragment>();
					if (ItemSourceConfig)
					{
						const FPrimaryAssetId SourceItem = ItemSourceConfig->ItemDefinition;
						if (SourceItem != Job->Item)
						{
							ArcTransportJobProcessorPrivate::QueueRemoveJob(
								ChunkContext, Entity, Job->Generation);
							continue;
						}
						RequestedQuantity = FMath::Min3(Job->Quantity, ItemSourceConfig->AmountPerInteraction, FreeCapacity);
					}
					else
					{
						const FArcBuildingOutputStore* SourceStore =
							SourceView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>();
						const int32 Present = SourceStore
							? ArcTransportJobProcessorPrivate::CountItem(*SourceStore, Job->Item)
							: 0;
						if (Present <= 0)
						{
							ArcTransportJobProcessorPrivate::QueueRemoveJob(
								ChunkContext, Entity, Job->Generation);
							continue;
						}
						RequestedQuantity = FMath::Min3(Job->Quantity, Present, FreeCapacity);
					}

					Target = Job->Source;
					SlotTag = ArcWorldSimTags::Slot_Pickup;
				}
				else
				{
					const int32 CargoCount =
						ArcTransportJobProcessorPrivate::CountItem(*BodyStore, Job->Item);
					if (CargoCount <= 0)
					{
						Job->Quantity = 0;
						ArcTransportJobProcessorPrivate::QueueRemoveJob(
							ChunkContext, Entity, Job->Generation);
						continue;
					}

					Job->Quantity = CargoCount;
					if (!ArcTransportJobProcessorPrivate::IsCompatibleSink(EntityManager, Job->Sink))
					{
						Job->Sink = ArcTransportJobProcessorPrivate::FindRetarget(
							EntityManager, Needs, Storages, Job->Item, CargoCount);
						if (!Job->Sink.IsValid())
						{
							continue;
						}
					}

					Target = Job->Sink;
					RequestedQuantity = CargoCount;
					SlotTag = ArcWorldSimTags::Slot_Drop;
				}

				FSmartObjectRequestResult RequestResult{FSmartObjectHandle()};
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

				Job->ActiveCommandSerial = Body.Command.Serial;
				ArcTransportJobProcessorPrivate::QueueItemInteraction(
					ChunkContext,
					Entity,
					ArcWorldSim::NextItemInteractionGeneration(),
					Body.Command.Serial,
					Job->Item,
					RequestedQuantity);
			}
		});
}
