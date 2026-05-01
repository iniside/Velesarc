// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fragments/ArcMassNetId.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcIrisEntityArray.generated.h"

class AArcMassEntityReplicationProxy;

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcIrisReplicatedEntity
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassNetId NetId;

	uint32 FragmentDirtyMask = 0;
	uint32 ReplicationKey = 0;

	UPROPERTY()
	TArray<FInstancedStruct> FragmentSlots;

	TArray<TArray<uint8>> PendingFragmentBytes;

	void InitSlots(const TArray<const UScriptStruct*>& FragmentTypes);
};

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcIrisEntityArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcIrisReplicatedEntity> Entities;

	TBitArray<> DirtyEntities;

	TArray<FArcMassNetId> PendingRemovals;

	UPROPERTY(NotReplicated)
	TObjectPtr<AArcMassEntityReplicationProxy> OwnerProxy = nullptr;

	int32 AddEntity(FArcMassNetId NetId, const TArray<const UScriptStruct*>& FragmentTypes);
	void RemoveEntity(FArcMassNetId NetId);
	int32 FindIndexByNetId(FArcMassNetId NetId) const;

	void MarkFragmentDirty(int32 EntityIndex, int32 FragmentSlot);
	void MarkEntityDirty(int32 EntityIndex);
	void ClearDirtyState();
	bool HasDirtyEntities() const;
	int32 GetEntityCount() const { return Entities.Num(); }

	void PreReplicatedRemove(FArcMassNetId NetId);
	void PostReplicatedAdd(FArcMassNetId NetId);
	void PostReplicatedChange(FArcMassNetId NetId, uint32 ChangedFragmentMask);

};
