// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcBodyItemTransfer.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Operations/ArcMassItemOperations.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcWorldSimStores.h"

namespace ArcBodyItemTransferPrivate
{
struct FTransferOperation
{
	FArcItemId ItemId;
	int32 Amount = 0;
	int32 Level = 1;
	bool bWhole = false;
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
}

int32 ArcBodyItemTransfer::TransferItems(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle From,
	const FArcMassItemStoreFragment& FromStore,
	const UScriptStruct* FromStoreType,
	const FMassEntityHandle To,
	const UScriptStruct* ToStoreType,
	const FPrimaryAssetId& Item,
	const int32 MaxAmount)
{
	if (!Item.IsValid() || MaxAmount <= 0 || !FromStoreType || !ToStoreType)
	{
		return 0;
	}

	TArray<ArcBodyItemTransferPrivate::FTransferOperation> Operations;
	int32 Remaining = MaxAmount;
	for (const FArcMassReplicatedItem& Entry : FromStore.ReplicatedItems.Items)
	{
		if (Remaining <= 0)
		{
			break;
		}

		const FArcItemData* Data = Entry.ToItem();
		if (!Data || Data->GetItemDefinitionId() != Item || !Data->GetItemId().IsValid())
		{
			continue;
		}

		const int32 Stacks = static_cast<int32>(Data->GetStacks());
		if (Stacks <= 0)
		{
			continue;
		}

		ArcBodyItemTransferPrivate::FTransferOperation Operation;
		Operation.ItemId = Data->GetItemId();
		Operation.Amount = FMath::Min(Stacks, Remaining);
		Operation.Level = Data->GetLevel();
		Operation.bWhole = Operation.Amount == Stacks;
		Remaining -= Operation.Amount;
		Operations.Add(Operation);
	}

	int32 Transferred = 0;
	for (const ArcBodyItemTransferPrivate::FTransferOperation& Operation : Operations)
	{
		if (Operation.bWhole)
		{
			if (ArcMassItems::MoveItem(EntityManager, From, To, Operation.ItemId, FromStoreType, ToStoreType))
			{
				Transferred += Operation.Amount;
			}
			continue;
		}

		if (!ArcMassItems::ModifyStacks(EntityManager, From, FromStoreType, Operation.ItemId, -Operation.Amount))
		{
			continue;
		}

		const FArcItemSpec Spec = FArcItemSpec::NewItem(Item, Operation.Level, Operation.Amount);
		const FArcItemId AddedId = ArcMassItems::AddItem(EntityManager, To, ToStoreType, Spec);
		if (!AddedId.IsValid())
		{
			ArcMassItems::ModifyStacks(EntityManager, From, FromStoreType, Operation.ItemId, Operation.Amount);
			continue;
		}

		Transferred += Operation.Amount;
	}

	return Transferred;
}

int32 ArcBodyItemTransfer::Pickup(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle User,
	const FMassEntityHandle Source,
	const FPrimaryAssetId& Item,
	const int32 MaxAmount)
{
	if (!Item.IsValid() || MaxAmount <= 0
		|| !EntityManager.IsEntityValid(User) || !EntityManager.IsEntityValid(Source))
	{
		return 0;
	}

	const FMassEntityView SourceView(EntityManager, Source);
	const FArcBuildingOutputStore* SourceStore =
		SourceView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>();
	if (!SourceStore)
	{
		return 0;
	}

	const int32 Present = ArcMassItems::CountItemsByDefinition(*SourceStore, Item);
	return TransferItems(
		EntityManager,
		Source,
		*SourceStore,
		FArcBuildingOutputStore::StaticStruct(),
		User,
		FArcMassItemStoreFragment::StaticStruct(),
		Item,
		FMath::Min(MaxAmount, Present));
}

int32 ArcBodyItemTransfer::Drop(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle User,
	const FMassEntityHandle Sink,
	const FPrimaryAssetId& Item,
	const int32 MaxAmount)
{
	if (!Item.IsValid() || MaxAmount <= 0
		|| !EntityManager.IsEntityValid(User) || !EntityManager.IsEntityValid(Sink))
	{
		return 0;
	}

	const FMassEntityView BodyView(EntityManager, User);
	const FArcMassItemStoreFragment* BodyStore =
		BodyView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
	if (!BodyStore)
	{
		return 0;
	}

	const FMassEntityView SinkView(EntityManager, Sink);
	const bool bSinkUsesOutputStore =
		SinkView.GetConstSharedFragmentDataPtr<FArcStorageConfigFragment>() != nullptr
		|| SinkView.GetConstSharedFragmentDataPtr<FArcGatheringConfigFragment>() != nullptr;
	const UScriptStruct* SinkStoreType = bSinkUsesOutputStore
		? FArcBuildingOutputStore::StaticStruct()
		: FArcBuildingInputStore::StaticStruct();
	const FArcMassItemStoreFragment* SinkStore = bSinkUsesOutputStore
		? static_cast<const FArcMassItemStoreFragment*>(SinkView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>())
		: static_cast<const FArcMassItemStoreFragment*>(SinkView.GetSparseFragmentDataPtr<FArcBuildingInputStore>());
	if (!SinkStore)
	{
		return 0;
	}
	int32 TransferQuantity = MaxAmount;
	if (const FArcGatheringConfigFragment* GatheringConfig =
		SinkView.GetConstSharedFragmentDataPtr<FArcGatheringConfigFragment>())
	{
		TransferQuantity = FMath::Min(
			TransferQuantity,
			FMath::Max(
				0,
				GatheringConfig->OutputCapacity - ArcBodyItemTransferPrivate::CountAllItems(*SinkStore)));
	}
	else if (const FArcStorageConfigFragment* StorageConfig =
		SinkView.GetConstSharedFragmentDataPtr<FArcStorageConfigFragment>())
	{
		TransferQuantity = FMath::Min(
			TransferQuantity,
			FMath::Max(0, StorageConfig->Capacity - ArcBodyItemTransferPrivate::CountAllItems(*SinkStore)));
	}
	if (TransferQuantity <= 0)
	{
		return 0;
	}

	return TransferItems(
		EntityManager,
		User,
		*BodyStore,
		FArcMassItemStoreFragment::StaticStruct(),
		Sink,
		SinkStoreType,
		Item,
		TransferQuantity);
}
