// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcMassItemRecordSyncSubsystem.h"

#include "Core/ArcCoreAssetManager.h"
#include "Engine/World.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "UObject/UObjectIterator.h"

void UArcMassItemRecordSyncSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UMassEntitySubsystem>();

	if (UAssetManager::IsInitialized())
	{
		if (UArcCoreAssetManager* AssetManager = Cast<UArcCoreAssetManager>(&UAssetManager::Get()))
		{
			RecordChangedHandle = AssetManager->OnJsonRecordChanged().AddUObject(this, &UArcMassItemRecordSyncSubsystem::OnJsonRecordChanged);
		}
	}
}

void UArcMassItemRecordSyncSubsystem::Deinitialize()
{
	if (RecordChangedHandle.IsValid() && UAssetManager::IsInitialized())
	{
		if (UArcCoreAssetManager* AssetManager = Cast<UArcCoreAssetManager>(&UAssetManager::Get()))
		{
			AssetManager->OnJsonRecordChanged().Remove(RecordChangedHandle);
		}
	}
	RecordChangedHandle.Reset();

	Super::Deinitialize();
}

void UArcMassItemRecordSyncSubsystem::OnJsonRecordChanged(const FPrimaryAssetId& Id)
{
	const FArcJsonAssetTypeInfo* TypeInfo = UArcCoreAssetManager::Get().FindJsonRecordType(Id.PrimaryAssetType);
	if (TypeInfo == nullptr || TypeInfo->Struct != FArcItemDefinitionData::StaticStruct())
	{
		return;
	}

	UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (MassSubsystem == nullptr)
	{
		return;
	}
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	TArray<FArcItemData*> ChangedItems;
	TArray<FArcItemData*> SocketOwners;
	for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
	{
		if (!StructIt->IsChildOf(FArcMassItemStoreFragment::StaticStruct()))
		{
			continue;
		}

		for (UE::Mass::FSparseElementIterator ElementIt = EntityManager.CreateSparseElementsIterator(*StructIt); ElementIt; ++ElementIt)
		{
			FArcMassItemStoreFragment* Store = ElementIt.GetElementView().GetPtr<FArcMassItemStoreFragment>();
			if (Store == nullptr)
			{
				continue;
			}

			for (FArcMassReplicatedItem& ReplicatedItem : Store->ReplicatedItems.Items)
			{
				FArcItemData* Item = ReplicatedItem.ToItem();
				if (Item == nullptr)
				{
					continue;
				}

				if (Item->GetItemDefinitionId() == Id)
				{
					ChangedItems.Add(Item);
				}

				// An owner merges the scalable float fragments of its socketed items, so a socket item's record change rebuilds the owner too.
				for (const FArcItemData* SocketItem : Item->GetItemsInSockets())
				{
					if (SocketItem != nullptr && SocketItem->GetItemDefinitionId() == Id)
					{
						SocketOwners.Add(Item);
						break;
					}
				}
			}
		}
	}

	for (FArcItemData* Item : ChangedItems)
	{
		Item->RefreshItemDefinition();
	}
	for (FArcItemData* Item : SocketOwners)
	{
		Item->RefreshItemDefinition();
	}
}
