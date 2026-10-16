// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"

class UScriptStruct;

struct FArcItemEventKey
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;

	bool operator==(const FArcItemEventKey& Other) const
	{
		return Entity == Other.Entity && StoreType == Other.StoreType;
	}

	friend uint32 GetTypeHash(const FArcItemEventKey& K)
	{
		const UPTRINT TypePtr = reinterpret_cast<UPTRINT>(K.StoreType);
		return HashCombine(GetTypeHash(K.Entity), ::GetTypeHash(TypePtr));
	}
};

/** Like FArcItemEventKey but additionally keyed on a slot id, so subscribers can bind
 *  to slot-assigned / slot-cleared edges for one specific slot on one entity+store. */
struct FArcItemSlotEventKey
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;
	FGameplayTag         SlotId;

	bool operator==(const FArcItemSlotEventKey& Other) const
	{
		return Entity == Other.Entity && StoreType == Other.StoreType && SlotId == Other.SlotId;
	}

	friend uint32 GetTypeHash(const FArcItemSlotEventKey& K)
	{
		const UPTRINT TypePtr = reinterpret_cast<UPTRINT>(K.StoreType);
		uint32 Hash = HashCombine(GetTypeHash(K.Entity), ::GetTypeHash(TypePtr));
		return HashCombine(Hash, GetTypeHash(K.SlotId));
	}
};
