// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcMatchProcessor.h"

#include "ArcMass/Debugger/ArcEconomyTrace.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "Operations/ArcMassItemOperations.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcBodyUtilityProcessor.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcEconomyNames.h"
#include "Sim/ArcSettlementLedgerSubsystem.h"
#include "Sim/ArcTransportJobFragment.h"
#include "Sim/ArcWorldSimStores.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMatchProcessor)

namespace ArcMatchProcessorPrivate
{
struct FStorageSlot
{
	FMassEntityHandle Entity;
	int32 FreeSpace = 0;
};

int32 SumStacks(const FArcMassItemStoreFragment& Store)
{
	int32 Total = 0;
	for (const FArcMassReplicatedItem& Item : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data)
		{
			Total += static_cast<int32>(Data->GetStacks());
		}
	}
	return Total;
}

bool FindBodyCargo(const FArcMassItemStoreFragment& Store, FPrimaryAssetId& OutItem, int32& OutQuantity)
{
	for (const FArcMassReplicatedItem& Item : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (!Data)
		{
			continue;
		}

		const FPrimaryAssetId DefinitionId = Data->GetItemDefinitionId();
		if (!DefinitionId.IsValid() || static_cast<int32>(Data->GetStacks()) <= 0)
		{
			continue;
		}

		OutItem = DefinitionId;
		OutQuantity = ArcMassItems::CountItemsByDefinition(Store, DefinitionId);
		return OutQuantity > 0;
	}
	return false;
}

FStorageSlot* FindStorageWithSpace(TArray<FStorageSlot>& Storages)
{
	for (FStorageSlot& Slot : Storages)
	{
		if (Slot.FreeSpace > 0)
		{
			return &Slot;
		}
	}
	return nullptr;
}

FArcHaveRow* FindHave(
	TArray<FArcHaveRow>& Rows,
	const TArray<FPrimaryAssetId>& AcceptableItems,
	const TSet<FMassEntityHandle>& ExcludedPlaces)
{
	for (FArcHaveRow& Row : Rows)
	{
		if (Row.Quantity <= 0 || !Row.Item.IsValid() || ExcludedPlaces.Contains(Row.Place))
		{
			continue;
		}
		if (AcceptableItems.Contains(Row.Item))
		{
			return &Row;
		}
	}
	return nullptr;
}

FArcNeedRow* FindNeedForItem(TArray<FArcNeedRow>& Needs, const FPrimaryAssetId& Item)
{
	for (FArcNeedRow& Need : Needs)
	{
		if (Need.Quantity > 0 && Need.Building.IsValid() && Need.AcceptableItems.Contains(Item))
		{
			return &Need;
		}
	}
	return nullptr;
}

void QueueTransportJob(
	FMassExecutionContext& Context,
	const FMassEntityHandle Entity,
	const uint32 Generation,
	const FMassEntityHandle Source,
	const FMassEntityHandle Sink,
	const FPrimaryAssetId& Item,
	const int32 Quantity,
	const EArcTransportJobPhase Phase)
{
	Context.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[Entity, Generation, Source, Sink, Item, Quantity, Phase](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityValid(Entity))
			{
				return;
			}

			FStructView View = EntityManager.GetMutableSparseElementDataForEntity(
				FArcTransportJobFragment::StaticStruct(), Entity);
			if (View.GetPtr<FArcTransportJobFragment>())
			{
				return;
			}

			FArcTransportJobFragment& Job =
				EntityManager.AddSparseElementToEntity<FArcTransportJobFragment>(Entity);
			Job.Generation = Generation;
			Job.Source = Source;
			Job.Sink = Sink;
			Job.Item = Item;
			Job.Quantity = Quantity;
			Job.Phase = Phase;
		});
}

void BookmarkCarrierAssign(
	const FMassEntityManager& EntityManager,
	const FMassEntityHandle Source,
	const FMassEntityHandle Sink,
	const FPrimaryAssetId& Item,
	const int32 Quantity)
{
	TRACE_ARC_ECONOMY_BOOKMARKF(
		TEXT("MATCH-JOB item=%s qty=%d source=%s sink=%s"),
		*ArcEconomyNames::ItemName(Item),
		Quantity,
		*ArcEconomyNames::BuildingName(EntityManager, Source),
		*ArcEconomyNames::BuildingName(EntityManager, Sink));
}
}

UArcMatchProcessor::UArcMatchProcessor()
	: CarrierQuery(*this)
	, StorageQuery(*this)
	, OutputBufferQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcBodyUtilityProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UArcMatchProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UArcSettlementLedgerSubsystem>(EMassFragmentAccess::ReadWrite);

	CarrierQuery.Initialize(EntityManager);
	CarrierQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadOnly);
	CarrierQuery.AddRequirement<FArcCarrierAssignmentFragment>(EMassFragmentAccess::ReadOnly);
	CarrierQuery.AddRequirement<FArcCarryCapabilityFragment>(EMassFragmentAccess::ReadOnly);
	CarrierQuery.RegisterWithProcessor(*this);

	StorageQuery.Initialize(EntityManager);
	StorageQuery.AddConstSharedRequirement<FArcStorageConfigFragment>(EMassFragmentPresence::All);
	StorageQuery.AddSparseRequirement<FArcBuildingOutputStore>(EMassFragmentPresence::All);
	StorageQuery.RegisterWithProcessor(*this);

	OutputBufferQuery.Initialize(EntityManager);
	OutputBufferQuery.AddSparseRequirement<FArcBuildingOutputStore>(EMassFragmentPresence::All);
	OutputBufferQuery.AddConstSharedRequirement<FArcStorageConfigFragment>(EMassFragmentPresence::None);
	OutputBufferQuery.RegisterWithProcessor(*this);
}

void UArcMatchProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcSettlementLedgerSubsystem* Ledger = Context.GetMutableSubsystem<UArcSettlementLedgerSubsystem>();
	if (!Ledger)
	{
		return;
	}

	TArray<FArcNeedRow> Needs = Ledger->Needs;
	TArray<FArcHaveRow> Haves = Ledger->Haves;
	TArray<FArcHaveRow> StorageHaves = Ledger->StorageHaves;
	TSet<FMassEntityHandle> CarrierBodies;
	TSet<FMassEntityHandle> PendingJobs;
	TMap<FMassEntityHandle, int32> DropReservationsBySink;

	CarrierQuery.ForEachEntityChunk(Context,
		[&EntityManager, &CarrierBodies, &DropReservationsBySink](FMassExecutionContext& ChunkContext)
		{
			const TConstArrayView<FArcCarrierAssignmentFragment> Assignments =
				ChunkContext.GetFragmentView<FArcCarrierAssignmentFragment>();
			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				if (!Assignments[EntityIt].bAssigned)
				{
					continue;
				}

				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				CarrierBodies.Add(Entity);
				const FMassEntityView EntityView(EntityManager, Entity);
				const FArcTransportJobFragment* Job =
					EntityView.GetSparseFragmentDataPtr<FArcTransportJobFragment>();
				const FArcMassItemStoreFragment* BodyStore =
					EntityView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
				if (Job && Job->Phase == EArcTransportJobPhase::Drop
					&& Job->Sink.IsValid() && BodyStore)
				{
					DropReservationsBySink.FindOrAdd(Job->Sink) +=
						ArcMassItems::CountItemsByDefinition(*BodyStore, Job->Item);
				}
			}
		});

	if (CarrierBodies.IsEmpty())
	{
		return;
	}

	TSet<FMassEntityHandle> OutputBuffers;
	OutputBufferQuery.ForEachEntityChunk(Context, [&OutputBuffers](FMassExecutionContext& ChunkContext)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			OutputBuffers.Add(ChunkContext.GetEntity(EntityIt));
		}
	});

	TArray<ArcMatchProcessorPrivate::FStorageSlot> Storages;
	StorageQuery.ForEachEntityChunk(Context,
		[&Storages, &DropReservationsBySink](FMassExecutionContext& ChunkContext)
		{
			const FArcStorageConfigFragment& Config =
				ChunkContext.GetConstSharedFragment<FArcStorageConfigFragment>();
			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				const FArcBuildingOutputStore* Store =
					ChunkContext.GetMutableSparseElement<FArcBuildingOutputStore>(EntityIt);
				if (!Store)
				{
					continue;
				}

				const int32* Reserved = DropReservationsBySink.Find(Entity);
				ArcMatchProcessorPrivate::FStorageSlot Slot;
				Slot.Entity = Entity;
				Slot.FreeSpace = FMath::Max(
					0,
					Config.Capacity - ArcMatchProcessorPrivate::SumStacks(*Store) - (Reserved ? *Reserved : 0));
				if (Slot.FreeSpace > 0)
				{
					Storages.Add(Slot);
				}
			}
		});

	CarrierQuery.ForEachEntityChunk(Context,
		[&EntityManager, &Needs, &Haves, &StorageHaves, &CarrierBodies, &PendingJobs, &OutputBuffers, &Storages](
			FMassExecutionContext& ChunkContext)
		{
			const TConstArrayView<FArcCarrierAssignmentFragment> Assignments =
				ChunkContext.GetFragmentView<FArcCarrierAssignmentFragment>();
			const TConstArrayView<FArcCarryCapabilityFragment> Capabilities =
				ChunkContext.GetFragmentView<FArcCarryCapabilityFragment>();
			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				if (!Assignments[EntityIt].bAssigned)
				{
					continue;
				}

				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				const FMassEntityView EntityView(EntityManager, Entity);
				if (PendingJobs.Contains(Entity)
					|| EntityView.GetSparseFragmentDataPtr<FArcTransportJobFragment>())
				{
					continue;
				}

				const FArcMassItemStoreFragment* BodyStore =
					EntityView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
				const int32 CarryCapacity = FMath::Max(0, Capabilities[EntityIt].Capacity);
				FPrimaryAssetId CargoItem;
				int32 CargoQuantity = 0;
				if (BodyStore && ArcMatchProcessorPrivate::FindBodyCargo(*BodyStore, CargoItem, CargoQuantity))
				{
					FMassEntityHandle Sink;
					int32 Quantity = 0;
					if (FArcNeedRow* Need = ArcMatchProcessorPrivate::FindNeedForItem(Needs, CargoItem))
					{
						Sink = Need->Building;
						Quantity = FMath::Min(CargoQuantity, Need->Quantity);
						Need->Quantity -= Quantity;
					}
					else if (ArcMatchProcessorPrivate::FStorageSlot* Storage =
						ArcMatchProcessorPrivate::FindStorageWithSpace(Storages))
					{
						Sink = Storage->Entity;
						Quantity = FMath::Min(CargoQuantity, Storage->FreeSpace);
						Storage->FreeSpace -= Quantity;
					}

					if (Sink.IsValid() && Quantity > 0)
					{
						ArcMatchProcessorPrivate::QueueTransportJob(
							ChunkContext,
							Entity,
							ArcWorldSim::NextTransportJobGeneration(),
							Entity,
							Sink,
							CargoItem,
							Quantity,
							EArcTransportJobPhase::Drop);
						PendingJobs.Add(Entity);
						ArcMatchProcessorPrivate::BookmarkCarrierAssign(
							EntityManager, Entity, Sink, CargoItem, Quantity);
					}
					continue;
				}

				if (CarryCapacity <= 0)
				{
					continue;
				}

				bool bAssigned = false;
				for (FArcNeedRow& Need : Needs)
				{
					if (Need.Quantity <= 0 || !Need.Building.IsValid())
					{
						continue;
					}

					FArcHaveRow* Have = ArcMatchProcessorPrivate::FindHave(
						Haves, Need.AcceptableItems, CarrierBodies);
					if (!Have)
					{
						Have = ArcMatchProcessorPrivate::FindHave(
							StorageHaves, Need.AcceptableItems, CarrierBodies);
					}
					if (!Have)
					{
						continue;
					}

					const int32 Quantity = FMath::Min3(Need.Quantity, Have->Quantity, CarryCapacity);
					if (Quantity <= 0)
					{
						continue;
					}

					ArcMatchProcessorPrivate::QueueTransportJob(
						ChunkContext,
						Entity,
						ArcWorldSim::NextTransportJobGeneration(),
						Have->Place,
						Need.Building,
						Have->Item,
						Quantity,
						EArcTransportJobPhase::Pickup);
					PendingJobs.Add(Entity);
					Need.Quantity -= Quantity;
					Have->Quantity -= Quantity;
					ArcMatchProcessorPrivate::BookmarkCarrierAssign(
						EntityManager, Have->Place, Need.Building, Have->Item, Quantity);
					bAssigned = true;
					break;
				}

				if (bAssigned)
				{
					continue;
				}

				ArcMatchProcessorPrivate::FStorageSlot* SurplusSink =
					ArcMatchProcessorPrivate::FindStorageWithSpace(Storages);
				if (!SurplusSink)
				{
					continue;
				}

				for (FArcHaveRow& Have : Haves)
				{
					if (Have.Quantity <= 0 || !Have.Item.IsValid()
						|| !OutputBuffers.Contains(Have.Place) || CarrierBodies.Contains(Have.Place))
					{
						continue;
					}

					const int32 Quantity = FMath::Min3(Have.Quantity, CarryCapacity, SurplusSink->FreeSpace);
					if (Quantity <= 0)
					{
						continue;
					}

					ArcMatchProcessorPrivate::QueueTransportJob(
						ChunkContext,
						Entity,
						ArcWorldSim::NextTransportJobGeneration(),
						Have.Place,
						SurplusSink->Entity,
						Have.Item,
						Quantity,
						EArcTransportJobPhase::Pickup);
					PendingJobs.Add(Entity);
					Have.Quantity -= Quantity;
					SurplusSink->FreeSpace -= Quantity;
					ArcMatchProcessorPrivate::BookmarkCarrierAssign(
						EntityManager, Have.Place, SurplusSink->Entity, Have.Item, Quantity);
					break;
				}
			}
		});
}
