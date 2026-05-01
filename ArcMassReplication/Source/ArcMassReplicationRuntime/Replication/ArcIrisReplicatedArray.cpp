// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcIrisReplicatedArray.h"

void FArcIrisReplicatedArray::MarkItemDirtyByIndex(int32& OutReplicationKey, int32 Index)
{
	++OutReplicationKey;
	if (DirtyItems.IsValidIndex(Index))
	{
		DirtyItems[Index] = true;
	}
}

void FArcIrisReplicatedArray::ClearDirtyState(int32 ItemCount)
{
	PendingRemovals.Reset();
	DirtyItems.Init(false, ItemCount);
}

bool FArcIrisReplicatedArray::HasDirtyItems() const
{
	if (PendingRemovals.Num() > 0)
	{
		return true;
	}
	for (TConstSetBitIterator<> It(DirtyItems); It; ++It)
	{
		return true;
	}
	return false;
}

int32 FArcIrisReplicatedArray::FindIndexByIrisRepID(const FArcIrisReplicatedArrayItem* ItemsData, int32 ItemCount, int32 RepID) const
{
	for (int32 Index = 0; Index < ItemCount; ++Index)
	{
		if (ItemsData[Index].IrisRepID == RepID)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
