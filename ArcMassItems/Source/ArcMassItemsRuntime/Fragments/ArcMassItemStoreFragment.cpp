// Copyright Lukasz Baran. All Rights Reserved.

#include "Fragments/ArcMassItemStoreFragment.h"

FArcMassReplicatedItem* FArcMassReplicatedItemArray::FindById(FArcItemId ItemId)
{
	for (FArcMassReplicatedItem& Item : Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data && Data->GetItemId() == ItemId)
		{
			return &Item;
		}
	}
	return nullptr;
}

int32 FArcMassReplicatedItemArray::FindIndexById(FArcItemId ItemId) const
{
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const FArcItemData* Data = Items[Index].ToItem();
		if (Data && Data->GetItemId() == ItemId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 FArcMassReplicatedItemArray::FindIndexByReplicationID(int32 RepID) const
{
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (Items[Index].ReplicationID == RepID)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
