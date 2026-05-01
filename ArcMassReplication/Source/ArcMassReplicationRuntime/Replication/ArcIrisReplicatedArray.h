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

	TBitArray<> DirtyItems;
	TArray<int32> PendingRemovals;
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
		DirtyItems.Add(true);
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
		DirtyItems.Init(false, Items.Num());
		for (int32 Idx = 0; Idx < Items.Num(); ++Idx)
		{
			DirtyItems[Idx] = true;
		}
	}

	template<typename ItemType>
	void MarkItemDirty(TArray<ItemType>& Items, ItemType& Item)
	{
		int32 Index = static_cast<int32>(&Item - Items.GetData());
		if (Items.IsValidIndex(Index))
		{
			++Items[Index].ReplicationKey;
			++Items[Index].IrisRepKey;
			if (DirtyItems.IsValidIndex(Index))
			{
				DirtyItems[Index] = true;
			}
		}
	}

	template<typename ItemType>
	void MarkAllDirty(TArray<ItemType>& Items)
	{
		DirtyItems.Init(true, Items.Num());
		for (ItemType& Item : Items)
		{
			++Item.ReplicationKey;
			++Item.IrisRepKey;
		}
	}

	void MarkItemDirtyByIndex(int32& OutReplicationKey, int32 Index);
	void ClearDirtyState(int32 ItemCount);
	bool HasDirtyItems() const;
	int32 FindIndexByIrisRepID(const FArcIrisReplicatedArrayItem* ItemsData, int32 ItemCount, int32 RepID) const;
};
