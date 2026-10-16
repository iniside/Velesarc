// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcLedgerProcessor.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcMass/Debugger/ArcEconomyTrace.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "GameplayTagContainer.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonTypes.h"
#include "MassSubsystemAccess.h"
#include "Operations/ArcMassItemOperations.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcGatheringProcessor.h"
#include "Sim/ArcProductionProcessor.h"
#include "Sim/ArcProductionSlots.h"
#include "Sim/ArcSettlementLedgerSubsystem.h"
#include "Sim/ArcTransportJobFragment.h"
#include "Sim/ArcWorldSimStores.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcLedgerProcessor)

namespace ArcLedgerProcessorPrivate
{
struct FInFlightToSink
{
	FMassEntityHandle Sink;
	FPrimaryAssetId Item;
	int32 Quantity = 0;
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

void AccumulateStacksByDefinition(const FArcMassItemStoreFragment& Store, TMap<FPrimaryAssetId, int32>& OutStacks)
{
	for (const FArcMassReplicatedItem& Item : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (!Data)
		{
			continue;
		}

		const FPrimaryAssetId DefinitionId = Data->GetItemDefinitionId();
		if (!DefinitionId.IsValid())
		{
			continue;
		}

		OutStacks.FindOrAdd(DefinitionId) += static_cast<int32>(Data->GetStacks());
	}
}

int32 CountInFlightToNeed(const TArray<FInFlightToSink>& Flights, FMassEntityHandle Building, const TArray<FPrimaryAssetId>& AcceptableItems)
{
	int32 Total = 0;
	for (const FInFlightToSink& Flight : Flights)
	{
		if (Flight.Sink == Building && AcceptableItems.Contains(Flight.Item))
		{
			Total += Flight.Quantity;
		}
	}
	return Total;
}

void SortAcceptableItems(TArray<FPrimaryAssetId>& Items)
{
	Items.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) -> bool
	{
		const FName TypeA = A.PrimaryAssetType.GetName();
		const FName TypeB = B.PrimaryAssetType.GetName();
		if (TypeA != TypeB)
		{
			return TypeA.LexicalLess(TypeB);
		}
		return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName);
	});
}

struct FIngredientNeedGroup
{
	TArray<FPrimaryAssetId> SortedKey;
	TArray<FPrimaryAssetId> AcceptableItems;
	int32 Amount = 0;
	bool bCountByTags = false;
	FPrimaryAssetId ItemDefinitionId;
	FGameplayTagContainer RequiredTags;
	FGameplayTagContainer DenyTags;
};

void AddIngredientToGroups(
	TArray<FIngredientNeedGroup>& Groups,
	TArray<FPrimaryAssetId>&& AcceptableItems,
	int32 Amount,
	bool bCountByTags,
	const FPrimaryAssetId& ItemDefinitionId,
	FGameplayTagContainer&& RequiredTags,
	FGameplayTagContainer&& DenyTags)
{
	TArray<FPrimaryAssetId> SortedKey = AcceptableItems;
	SortAcceptableItems(SortedKey);
	for (FIngredientNeedGroup& Group : Groups)
	{
		if (Group.SortedKey == SortedKey)
		{
			Group.Amount += Amount;
			return;
		}
	}

	FIngredientNeedGroup& Group = Groups.AddDefaulted_GetRef();
	Group.SortedKey = MoveTemp(SortedKey);
	Group.AcceptableItems = MoveTemp(AcceptableItems);
	Group.Amount = Amount;
	Group.bCountByTags = bCountByTags;
	Group.ItemDefinitionId = ItemDefinitionId;
	Group.RequiredTags = MoveTemp(RequiredTags);
	Group.DenyTags = MoveTemp(DenyTags);
}

void AppendStoreHaves(
	const FMassEntityHandle Place,
	const FArcMassItemStoreFragment& Store,
	const TMap<FMassEntityHandle, TMap<FPrimaryAssetId, int32>>& InFlightFromSource,
	TArray<FArcHaveRow>& OutHaves)
{
	TMap<FPrimaryAssetId, int32> StacksByDef;
	AccumulateStacksByDefinition(Store, StacksByDef);

	const TMap<FPrimaryAssetId, int32>* Reserved = InFlightFromSource.Find(Place);
	for (const TPair<FPrimaryAssetId, int32>& Pair : StacksByDef)
	{
		int32 Available = Pair.Value;
		if (Reserved)
		{
			if (const int32* Quantity = Reserved->Find(Pair.Key))
			{
				Available -= *Quantity;
			}
		}
		if (Available > 0)
		{
			FArcHaveRow Row;
			Row.Place = Place;
			Row.Item = Pair.Key;
			Row.Quantity = Available;
			OutHaves.Add(MoveTemp(Row));
		}
	}
}
}

UArcLedgerProcessor::UArcLedgerProcessor()
	: ProductionQuery(*this)
	, OutputHaveQuery(*this)
	, StorageQuery(*this)
	, ItemSourceQuery(*this)
	, TransportJobQuery(*this)
	, StrandedCargoQuery(*this)
	, ItemConsumerQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcProductionProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(UArcGatheringProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UArcLedgerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UArcSettlementLedgerSubsystem>(EMassFragmentAccess::ReadWrite);

	ProductionQuery.Initialize(EntityManager);
	ProductionQuery.AddConstSharedRequirement<FArcProductionConfigFragment>(EMassFragmentPresence::All);
	ProductionQuery.AddRequirement<FArcProductionSlotsFragment>(EMassFragmentAccess::ReadWrite);
	ProductionQuery.AddSparseRequirement<FArcBuildingInputStore>(EMassFragmentPresence::All);
	ProductionQuery.RegisterWithProcessor(*this);

	OutputHaveQuery.Initialize(EntityManager);
	OutputHaveQuery.AddSparseRequirement<FArcBuildingOutputStore>(EMassFragmentPresence::All);
	OutputHaveQuery.AddConstSharedRequirement<FArcStorageConfigFragment>(EMassFragmentPresence::None);
	OutputHaveQuery.RegisterWithProcessor(*this);

	StorageQuery.Initialize(EntityManager);
	StorageQuery.AddConstSharedRequirement<FArcStorageConfigFragment>(EMassFragmentPresence::All);
	StorageQuery.AddSparseRequirement<FArcBuildingOutputStore>(EMassFragmentPresence::All);
	StorageQuery.RegisterWithProcessor(*this);

	ItemSourceQuery.Initialize(EntityManager);
	ItemSourceQuery.AddConstSharedRequirement<FArcItemSourceConfigFragment>(EMassFragmentPresence::All);
	ItemSourceQuery.RegisterWithProcessor(*this);

	TransportJobQuery.Initialize(EntityManager);
	TransportJobQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadOnly);
	TransportJobQuery.AddSparseRequirement<FArcTransportJobFragment>(EMassFragmentPresence::All);
	TransportJobQuery.RegisterWithProcessor(*this);

	StrandedCargoQuery.Initialize(EntityManager);
	StrandedCargoQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadOnly);
	StrandedCargoQuery.AddRequirement<FArcCarrierAssignmentFragment>(EMassFragmentAccess::ReadOnly);
	StrandedCargoQuery.AddSparseRequirement<FArcMassItemStoreFragment>(EMassFragmentPresence::All);
	StrandedCargoQuery.AddSparseRequirement<FArcTransportJobFragment>(EMassFragmentPresence::None);
	StrandedCargoQuery.RegisterWithProcessor(*this);

	ItemConsumerQuery.Initialize(EntityManager);
	ItemConsumerQuery.AddConstSharedRequirement<FArcItemConsumerConfigFragment>(EMassFragmentPresence::All);
	ItemConsumerQuery.AddSparseRequirement<FArcBuildingInputStore>(EMassFragmentPresence::All);
	ItemConsumerQuery.RegisterWithProcessor(*this);
}

void UArcLedgerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcSettlementLedgerSubsystem* Ledger = Context.GetMutableSubsystem<UArcSettlementLedgerSubsystem>();
	if (!Ledger)
	{
		return;
	}

	Ledger->Needs.Reset();
	Ledger->Haves.Reset();
	Ledger->StorageHaves.Reset();

	int32 ItemSourceCount = 0;
	int32 StorageCount = 0;
	int32 StorageOccupied = 0;
	int32 StorageCapacity = 0;

	TMap<FMassEntityHandle, TMap<FPrimaryAssetId, int32>> InFlightFromSource;
	TArray<ArcLedgerProcessorPrivate::FInFlightToSink> InFlightToSinks;

	TransportJobQuery.ForEachEntityChunk(Context, [&EntityManager, &InFlightFromSource, &InFlightToSinks](FMassExecutionContext& Ctx)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcTransportJobFragment* Job = Ctx.GetSparseElement<FArcTransportJobFragment>(EntityIt);
			if (!Job || !Job->Item.IsValid())
			{
				continue;
			}

			if (Job->Phase == EArcTransportJobPhase::Pickup)
			{
				if (Job->Source.IsValid() && Job->Quantity > 0)
				{
					InFlightFromSource.FindOrAdd(Job->Source).FindOrAdd(Job->Item) += Job->Quantity;
				}
				continue;
			}

			if (Job->Phase != EArcTransportJobPhase::Drop || !Job->Sink.IsValid())
			{
				continue;
			}

			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const FMassEntityView EntityView(EntityManager, Entity);
			const FArcMassItemStoreFragment* BodyStore =
				EntityView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			if (!BodyStore)
			{
				continue;
			}

			ArcLedgerProcessorPrivate::FInFlightToSink Flight;
			Flight.Sink = Job->Sink;
			Flight.Item = Job->Item;
			Flight.Quantity = ArcMassItems::CountItemsByDefinition(*BodyStore, Job->Item);
			if (Flight.Quantity > 0)
			{
				InFlightToSinks.Add(Flight);
			}
		}
	});

	StrandedCargoQuery.ForEachEntityChunk(Context, [Ledger, &InFlightFromSource](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcBodyFragment> Bodies = Ctx.GetFragmentView<FArcBodyFragment>();
		const TConstArrayView<FArcCarrierAssignmentFragment> Assignments =
			Ctx.GetFragmentView<FArcCarrierAssignmentFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			if (!Assignments[EntityIt].bAssigned || Bodies[EntityIt].State != EArcBodyState::Idle)
			{
				continue;
			}

			const FArcMassItemStoreFragment* BodyStore = Ctx.GetSparseElement<FArcMassItemStoreFragment>(EntityIt);
			if (BodyStore)
			{
				ArcLedgerProcessorPrivate::AppendStoreHaves(
					Ctx.GetEntity(EntityIt), *BodyStore, InFlightFromSource, Ledger->Haves);
			}
		}
	});

	ProductionQuery.ForEachEntityChunk(Context, [&EntityManager, Ledger, &InFlightToSinks](FMassExecutionContext& Ctx)
	{
		const FArcProductionConfigFragment& Config = Ctx.GetConstSharedFragment<FArcProductionConfigFragment>();

		TArrayView<FArcProductionSlotsFragment> ProductionSlots = Ctx.GetMutableFragmentView<FArcProductionSlotsFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const FMassEntityView EntityView(EntityManager, Entity);
			FArcProductionSlotsFragment& Production = ProductionSlots[EntityIt];
			const FArcSmartObjectDefinitionSharedFragment* Def =
				EntityView.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>();
			ArcWorldSim::SyncProductionSlots(
				Production,
				Config,
				Def ? Def->SmartObjectDefinition.Get() : nullptr);

			const FArcBuildingInputStore* InputStore = EntityView.GetSparseFragmentDataPtr<FArcBuildingInputStore>();
			if (!InputStore)
			{
				continue;
			}

			TArray<ArcLedgerProcessorPrivate::FIngredientNeedGroup> Groups;
			for (const FArcProductionSlot& Slot : Production.Slots)
			{
				if (Slot.RecipeIndex == INDEX_NONE || !Config.Recipes.IsValidIndex(Slot.RecipeIndex))
				{
					continue;
				}

				const FArcRecipeDefinitionData* Recipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(Config.Recipes[Slot.RecipeIndex].AssetId);
				if (!Recipe)
				{
					continue;
				}

				for (const FInstancedStruct& IngredientStruct : Recipe->Ingredients)
				{
					const FArcRecipeIngredient* Ingredient = IngredientStruct.GetPtr<FArcRecipeIngredient>();
					if (Ingredient != nullptr && Ingredient->bOptional)
					{
						continue;
					}

					if (const FArcRecipeIngredient_ItemDef* ItemDef = IngredientStruct.GetPtr<FArcRecipeIngredient_ItemDef>())
					{
						const FPrimaryAssetId Id = ItemDef->ItemDefinitionId;
						if (!Id.IsValid())
						{
							continue;
						}

						TArray<FPrimaryAssetId> AcceptableItems;
						AcceptableItems.Add(Id);
						ArcLedgerProcessorPrivate::AddIngredientToGroups(
							Groups,
							MoveTemp(AcceptableItems),
							ItemDef->Amount,
							false,
							Id,
							FGameplayTagContainer(),
							FGameplayTagContainer());
					}
					else if (const FArcRecipeIngredient_Tags* Tags = IngredientStruct.GetPtr<FArcRecipeIngredient_Tags>())
					{
						TArray<FPrimaryAssetId> AcceptableItems;
						Ledger->CollectItemIdsForAssetTags(Tags->RequiredTags, Tags->DenyTags, AcceptableItems);
						ArcLedgerProcessorPrivate::AddIngredientToGroups(
							Groups,
							MoveTemp(AcceptableItems),
							Tags->Amount,
							true,
							FPrimaryAssetId(),
							FGameplayTagContainer(Tags->RequiredTags),
							FGameplayTagContainer(Tags->DenyTags));
					}
				}
			}

			for (ArcLedgerProcessorPrivate::FIngredientNeedGroup& Group : Groups)
			{
				const int32 OnHand = Group.bCountByTags
					? ArcMassItems::CountItemsByTags(*InputStore, Group.RequiredTags, Group.DenyTags)
					: ArcMassItems::CountItemsByDefinition(*InputStore, Group.ItemDefinitionId);
				const int32 InFlight = ArcLedgerProcessorPrivate::CountInFlightToNeed(
					InFlightToSinks,
					Entity,
					Group.AcceptableItems);
				FArcNeedRow Need;
				// InputCapacity is per ingredient group, not a shared store cap.
				Need.Quantity = Config.InputCapacity - OnHand - InFlight;
				Need.Building = Entity;
				Need.AcceptableItems = MoveTemp(Group.AcceptableItems);
				if (Need.Quantity > 0)
				{
					Ledger->Needs.Add(MoveTemp(Need));
				}
			}
		}
	});

	ItemConsumerQuery.ForEachEntityChunk(Context, [&EntityManager, Ledger, &InFlightToSinks](FMassExecutionContext& Ctx)
	{
		const FArcItemConsumerConfigFragment& Config = Ctx.GetConstSharedFragment<FArcItemConsumerConfigFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const FMassEntityView EntityView(EntityManager, Entity);
			const FArcBuildingInputStore* InputStore = EntityView.GetSparseFragmentDataPtr<FArcBuildingInputStore>();
			if (!InputStore)
			{
				continue;
			}

			for (const FArcNamedPrimaryAssetId& Wanted : Config.WantedItems)
			{
				const FPrimaryAssetId Id = Wanted;
				if (!Id.IsValid())
				{
					continue;
				}

				const int32 OnHand = ArcMassItems::CountItemsByDefinition(*InputStore, Id);
				TArray<FPrimaryAssetId> AcceptableItems;
				AcceptableItems.Add(Id);
				const int32 InFlight = ArcLedgerProcessorPrivate::CountInFlightToNeed(
					InFlightToSinks,
					Entity,
					AcceptableItems);
				FArcNeedRow Need;
				Need.Quantity = Config.InputCapacity - OnHand - InFlight;
				Need.Building = Entity;
				Need.AcceptableItems = MoveTemp(AcceptableItems);
				if (Need.Quantity > 0)
				{
					Ledger->Needs.Add(MoveTemp(Need));
				}
			}
		}
	});

	OutputHaveQuery.ForEachEntityChunk(Context, [&EntityManager, Ledger, &InFlightFromSource](FMassExecutionContext& Ctx)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const FMassEntityView EntityView(EntityManager, Entity);
			const FArcBuildingOutputStore* OutputStore = EntityView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>();
			if (!OutputStore)
			{
				continue;
			}

			ArcLedgerProcessorPrivate::AppendStoreHaves(Entity, *OutputStore, InFlightFromSource, Ledger->Haves);
		}
	});

	ItemSourceQuery.ForEachEntityChunk(Context, [Ledger, &InFlightFromSource, &ItemSourceCount](FMassExecutionContext& Ctx)
	{
		ItemSourceCount += Ctx.GetNumEntities();
		const FArcItemSourceConfigFragment& Config = Ctx.GetConstSharedFragment<FArcItemSourceConfigFragment>();
		const FPrimaryAssetId ItemId = Config.ItemDefinition;
		if (!ItemId.IsValid() || Config.AmountPerInteraction <= 0)
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const TMap<FPrimaryAssetId, int32>* ReservedByItem = InFlightFromSource.Find(Entity);
			const int32* ReservedQuantity = ReservedByItem ? ReservedByItem->Find(ItemId) : nullptr;
			const int32 Available = FMath::Max(
				0,
				Config.AmountPerInteraction - (ReservedQuantity ? *ReservedQuantity : 0));
			if (Available <= 0)
			{
				continue;
			}

			FArcHaveRow Row;
			Row.Place = Entity;
			Row.Item = ItemId;
			Row.Quantity = Available;
			Ledger->Haves.Add(MoveTemp(Row));
		}
	});

	StorageQuery.ForEachEntityChunk(Context,
		[&EntityManager, Ledger, &InFlightFromSource, &StorageCount, &StorageOccupied, &StorageCapacity](FMassExecutionContext& Ctx)
	{
		const FArcStorageConfigFragment& Config = Ctx.GetConstSharedFragment<FArcStorageConfigFragment>();
		StorageCount += Ctx.GetNumEntities();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const FMassEntityView EntityView(EntityManager, Entity);
			const FArcBuildingOutputStore* OutputStore = EntityView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>();
			if (!OutputStore)
			{
				continue;
			}

			StorageOccupied += ArcLedgerProcessorPrivate::SumStacks(*OutputStore);
			StorageCapacity += Config.Capacity;
			ArcLedgerProcessorPrivate::AppendStoreHaves(Entity, *OutputStore, InFlightFromSource, Ledger->StorageHaves);
		}
	});

	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Ledger"), TEXT("Needs"), Ledger->Needs.Num());
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Ledger"), TEXT("Haves"), Ledger->Haves.Num());
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Ledger"), TEXT("StorageHaves"), Ledger->StorageHaves.Num());
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("ItemSource"), TEXT("Count"), ItemSourceCount);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Storage"), TEXT("Count"), StorageCount);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Storage"), TEXT("Occupied"), StorageOccupied);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Storage"), TEXT("Capacity"), StorageCapacity);
}
