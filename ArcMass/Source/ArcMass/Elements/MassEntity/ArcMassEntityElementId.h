// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

/**
 * ID for a specific Mass entity instance managed by a componentless placed entity partition actor.
 * Identity is the owning partition actor + entry index + transform index.
 * All three must be valid (actor valid, indices != INDEX_NONE) for the ID to be considered valid.
 */
struct FArcMassEntityElementId
{
	explicit operator bool() const
	{
		return PartitionActor.IsValid()
			&& EntryIndex != INDEX_NONE
			&& TransformIndex != INDEX_NONE;
	}

	bool operator==(const FArcMassEntityElementId& InRHS) const
	{
		return PartitionActor == InRHS.PartitionActor
			&& EntryIndex == InRHS.EntryIndex
			&& TransformIndex == InRHS.TransformIndex;
	}

	bool operator!=(const FArcMassEntityElementId& InRHS) const
	{
		return !(*this == InRHS);
	}

	friend inline uint32 GetTypeHash(const FArcMassEntityElementId& InId)
	{
		return HashCombine(HashCombine(GetTypeHash(InId.PartitionActor), GetTypeHash(InId.EntryIndex)), GetTypeHash(InId.TransformIndex));
	}

	TWeakObjectPtr<AActor> PartitionActor = nullptr;
	/** Index of the placed entity entry within the partition actor. INDEX_NONE means invalid. */
	int32 EntryIndex = INDEX_NONE;
	/** Index of the transform slot within the entry. INDEX_NONE means invalid. */
	int32 TransformIndex = INDEX_NONE;
};
