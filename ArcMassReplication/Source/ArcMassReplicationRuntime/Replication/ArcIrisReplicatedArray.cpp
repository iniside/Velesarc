// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcIrisReplicatedArray.h"

void FArcIrisReplicatedArray::MarkItemDirtyByIndex(int32& OutReplicationKey, int32 Index, int32 IrisRepID)
{
	++OutReplicationKey;
	ReceivedChangedIDs.Add(IrisRepID);
}

void FArcIrisReplicatedArray::ClearDirtyState()
{
	PendingRemovals.Reset();
	ReceivedAddedIDs.Reset();
	ReceivedChangedIDs.Reset();
}

bool FArcIrisReplicatedArray::HasDirtyItems() const
{
	return PendingRemovals.Num() > 0
		|| ReceivedAddedIDs.Num() > 0
		|| ReceivedChangedIDs.Num() > 0;
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
