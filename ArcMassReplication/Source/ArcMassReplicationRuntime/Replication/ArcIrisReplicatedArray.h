// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "ArcIrisReplicatedArray.generated.h"

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcIrisReplicatedArrayItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 IrisRepID = INDEX_NONE;

	UPROPERTY()
	int32 IrisRepKey = 0;
};

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcIrisReplicatedArray
{
	GENERATED_BODY()

	TArray<int32> PendingRemovals;
	TArray<int32> ReceivedAddedIDs;
	TArray<int32> ReceivedChangedIDs;
	int32 NextReplicationID = 0;

	using FItemCallback = void(*)(void* ItemMemory, FArcIrisReplicatedArray& Array);
	FItemCallback PreReplicatedRemoveCallback = nullptr;
	FItemCallback PostReplicatedAddCallback = nullptr;
	FItemCallback PostReplicatedChangeCallback = nullptr;

	template<typename ConcreteArrayType, typename ItemType>
	void InitCallbacks()
	{
		PreReplicatedRemoveCallback = [](void* ItemMemory, FArcIrisReplicatedArray& Array)
		{
			static_cast<ItemType*>(ItemMemory)->PreReplicatedRemove(static_cast<ConcreteArrayType&>(Array));
		};
		PostReplicatedAddCallback = [](void* ItemMemory, FArcIrisReplicatedArray& Array)
		{
			static_cast<ItemType*>(ItemMemory)->PostReplicatedAdd(static_cast<ConcreteArrayType&>(Array));
		};
		PostReplicatedChangeCallback = [](void* ItemMemory, FArcIrisReplicatedArray& Array)
		{
			static_cast<ItemType*>(ItemMemory)->PostReplicatedChange(static_cast<ConcreteArrayType&>(Array));
		};
	}

	template<typename ItemType>
	int32 AddItem(TArray<ItemType>& Items, ItemType&& Item)
	{
		int32 RepID = NextReplicationID++;
		Item.ReplicationID = RepID;
		Item.ReplicationKey = 0;
		Item.IrisRepID = RepID;
		Item.IrisRepKey = 0;
		int32 Index = Items.Add(MoveTemp(Item));
		ReceivedAddedIDs.Add(RepID);
		return Index;
	}

	template<typename ItemType>
	void RemoveItemAt(TArray<ItemType>& Items, int32 Index)
	{
		if (!Items.IsValidIndex(Index))
		{
			return;
		}
		PendingRemovals.Add(Items[Index].IrisRepID);
		Items.RemoveAtSwap(Index);
	}

	template<typename ItemType>
	void MarkItemDirty(TArray<ItemType>& Items, ItemType& Item)
	{
		int32 Index = static_cast<int32>(&Item - Items.GetData());
		if (Items.IsValidIndex(Index))
		{
			++Items[Index].ReplicationKey;
			++Items[Index].IrisRepKey;
			ReceivedChangedIDs.Add(Items[Index].IrisRepID);
		}
	}

	template<typename ItemType>
	void MarkAllDirty(TArray<ItemType>& Items)
	{
		for (ItemType& Item : Items)
		{
			++Item.ReplicationKey;
			++Item.IrisRepKey;
			ReceivedChangedIDs.Add(Item.IrisRepID);
		}
	}

	void MarkItemDirtyByIndex(int32& OutReplicationKey, int32 Index, int32 IrisRepID);
	void ClearDirtyState();
	bool HasDirtyItems() const;
	int32 FindIndexByIrisRepID(const FArcIrisReplicatedArrayItem* ItemsData, int32 ItemCount, int32 RepID) const;
};
