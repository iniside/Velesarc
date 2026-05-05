// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityElementTypes.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemTypes.h"
#include "Items/ArcItemSpec.h"
#include "StructUtils/InstancedStruct.h"
#include "Replication/ArcIrisReplicatedArray.h"
#include "ArcMassItemStoreFragment.generated.h"

class UArcItemDefinition;

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassReplicatedItem : public FArcIrisReplicatedArrayItem
{
	GENERATED_BODY()

	UPROPERTY()
	FInstancedStruct ItemData;

	FArcMassReplicatedItem() = default;

	explicit FArcMassReplicatedItem(const FArcItemSpec& InSpec)
	{
		ItemData = FArcItemData::NewFromSpec(InSpec);
		FArcItemData* Data = ItemData.GetMutablePtr<FArcItemData>();
		Data->Spec = InSpec;
	}

	FArcItemData* ToItem() { return ItemData.GetMutablePtr<FArcItemData>(); }
	const FArcItemData* ToItem() const { return ItemData.GetPtr<FArcItemData>(); }
	bool IsValid() const { return ItemData.IsValid(); }

	bool operator==(const FArcMassReplicatedItem& Other) const
	{
		const FArcItemData* A = ToItem();
		const FArcItemData* B = Other.ToItem();
		if (A && B) return *A == *B;
		return A == B;
	}

	bool operator==(const FArcItemId& InItemId) const
	{
		const FArcItemData* Data = ToItem();
		return Data && Data->GetItemId() == InItemId;
	}

	bool operator==(const FPrimaryAssetId& AssetId) const
	{
		const FArcItemData* Data = ToItem();
		return Data && Data->GetItemDefinitionId() == AssetId;
	}

	bool operator==(const UArcItemDefinition* ItemDefinition) const
	{
		const FArcItemData* Data = ToItem();
		return Data && Data->GetItemDefinition() == ItemDefinition;
	}

	void PreReplicatedRemove(const struct FArcMassReplicatedItemArray& Array) {}
	void PostReplicatedAdd(const struct FArcMassReplicatedItemArray& Array) {}
	void PostReplicatedChange(const struct FArcMassReplicatedItemArray& Array) {}
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassReplicatedItemArray : public FArcIrisReplicatedArray
{
	GENERATED_BODY()

	FArcMassReplicatedItemArray() { InitCallbacks<FArcMassReplicatedItemArray, FArcMassReplicatedItem>(); }

	UPROPERTY()
	TArray<FArcMassReplicatedItem> Items;

	int32 AddItem(FArcMassReplicatedItem&& Item) { return FArcIrisReplicatedArray::AddItem(Items, MoveTemp(Item)); }
	void RemoveItemAt(int32 Index) { FArcIrisReplicatedArray::RemoveItemAt(Items, Index); }

	void MarkItemDirty(int32 Index) { MarkItemDirtyByIndex(Items[Index].ReplicationKey, Index, Items[Index].IrisRepID); }
	void MarkItemDirty(FArcMassReplicatedItem& Item) { FArcIrisReplicatedArray::MarkItemDirty(Items, Item); }
	void MarkArrayDirty() { FArcIrisReplicatedArray::MarkAllDirty(Items); }
	void ClearDirtyState() { FArcIrisReplicatedArray::ClearDirtyState(); }

	FArcMassReplicatedItem* FindById(FArcItemId ItemId);
	int32 FindIndexById(FArcItemId ItemId) const;
	int32 FindIndexByReplicationID(int32 RepID) const;
};

// Base item store fragment. Derive to create specialised stores.
// To support multiple stores on a single entity (e.g. a high-churn inventory store
// and a static equipment store), derive FArcMassItemStoreFragment, then add one
// UArcMassItemStoreTrait per derived type to the entity config.
USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemStoreFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassReplicatedItemArray ReplicatedItems;
};

template<>
struct TMassFragmentTraits<FArcMassItemStoreFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
