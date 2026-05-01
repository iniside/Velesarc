// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcIrisEntityArray.h"
#include "Replication/ArcMassEntityReplicationProxy.h"


void FArcIrisReplicatedEntity::InitSlots(const TArray<const UScriptStruct*>& FragmentTypes)
{
	FragmentSlots.SetNum(FragmentTypes.Num());
	for (int32 Index = 0; Index < FragmentTypes.Num(); ++Index)
	{
		FragmentSlots[Index].InitializeAs(FragmentTypes[Index]);
	}
	FragmentDirtyMask = 0;
}

int32 FArcIrisEntityArray::AddEntity(FArcMassNetId NetId, const TArray<const UScriptStruct*>& FragmentTypes)
{
	int32 Index = Entities.Num();
	FArcIrisReplicatedEntity& NewEntity = Entities.AddDefaulted_GetRef();
	NewEntity.NetId = NetId;
	NewEntity.InitSlots(FragmentTypes);
	NewEntity.FragmentDirtyMask = (1U << FragmentTypes.Num()) - 1U;

	DirtyEntities.Add(true);
	return Index;
}

void FArcIrisEntityArray::RemoveEntity(FArcMassNetId NetId)
{
	int32 Index = FindIndexByNetId(NetId);
	if (Index == INDEX_NONE)
	{
		return;
	}

	PendingRemovals.Add(NetId);
	Entities.RemoveAtSwap(Index);

	DirtyEntities.Init(false, Entities.Num());
	for (int32 Idx = 0; Idx < Entities.Num(); ++Idx)
	{
		if (Entities[Idx].FragmentDirtyMask != 0)
		{
			DirtyEntities[Idx] = true;
		}
	}
}

int32 FArcIrisEntityArray::FindIndexByNetId(FArcMassNetId NetId) const
{
	for (int32 Index = 0; Index < Entities.Num(); ++Index)
	{
		if (Entities[Index].NetId == NetId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void FArcIrisEntityArray::MarkFragmentDirty(int32 EntityIndex, int32 FragmentSlot)
{
	if (!Entities.IsValidIndex(EntityIndex))
	{
		return;
	}

	Entities[EntityIndex].FragmentDirtyMask |= (1U << FragmentSlot);
	++Entities[EntityIndex].ReplicationKey;
	if (DirtyEntities.IsValidIndex(EntityIndex))
	{
		DirtyEntities[EntityIndex] = true;
	}
}

void FArcIrisEntityArray::MarkEntityDirty(int32 EntityIndex)
{
	if (!Entities.IsValidIndex(EntityIndex))
	{
		return;
	}

	int32 SlotCount = Entities[EntityIndex].FragmentSlots.Num();
	Entities[EntityIndex].FragmentDirtyMask = (1U << SlotCount) - 1U;
	++Entities[EntityIndex].ReplicationKey;
	if (DirtyEntities.IsValidIndex(EntityIndex))
	{
		DirtyEntities[EntityIndex] = true;
	}
}

void FArcIrisEntityArray::ClearDirtyState()
{
	PendingRemovals.Reset();
	DirtyEntities.Init(false, Entities.Num());
	for (FArcIrisReplicatedEntity& Entity : Entities)
	{
		Entity.FragmentDirtyMask = 0;
	}
}

bool FArcIrisEntityArray::HasDirtyEntities() const
{
	if (PendingRemovals.Num() > 0)
	{
		return true;
	}

	for (TConstSetBitIterator<> It(DirtyEntities); It; ++It)
	{
		return true;
	}
	return false;
}

void FArcIrisEntityArray::PreReplicatedRemove(FArcMassNetId NetId)
{
	if (OwnerProxy)
	{
		OwnerProxy->OnClientEntityRemoved(NetId);
	}
}

void FArcIrisEntityArray::PostReplicatedAdd(FArcMassNetId NetId)
{
	if (OwnerProxy)
	{
		int32 Index = FindIndexByNetId(NetId);
		if (Index != INDEX_NONE)
		{
			OwnerProxy->OnClientEntityAdded(NetId, Entities[Index].FragmentSlots);
		}
	}
}

void FArcIrisEntityArray::PostReplicatedChange(FArcMassNetId NetId, uint32 ChangedFragmentMask)
{
	if (OwnerProxy)
	{
		int32 Index = FindIndexByNetId(NetId);
		if (Index != INDEX_NONE)
		{
			OwnerProxy->OnClientEntityChanged(NetId, Entities[Index].FragmentSlots, ChangedFragmentMask);
		}
	}
}
