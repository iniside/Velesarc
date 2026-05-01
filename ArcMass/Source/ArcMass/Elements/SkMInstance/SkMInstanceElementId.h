// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UInstancedSkinnedMeshComponent;

/**
 * ID for a specific instance within an instanced skinned mesh component.
 * Stores the FPrimitiveInstanceId value as a plain int32 to avoid pulling
 * InstanceDataTypes.generated.h into non-UHT headers.
 * INDEX_NONE means invalid.
 */
struct FSkMInstanceId
{
	explicit operator bool() const
	{
		return SkMComponent != nullptr
			&& InstanceId != INDEX_NONE;
	}

	bool operator==(const FSkMInstanceId& InRHS) const
	{
		return SkMComponent == InRHS.SkMComponent
			&& InstanceId == InRHS.InstanceId;
	}

	bool operator!=(const FSkMInstanceId& InRHS) const
	{
		return !(*this == InRHS);
	}

	friend inline uint32 GetTypeHash(const FSkMInstanceId& InId)
	{
		return HashCombine(GetTypeHash(InId.InstanceId), GetTypeHash(InId.SkMComponent));
	}

	UInstancedSkinnedMeshComponent* SkMComponent = nullptr;
	/** Corresponds to FPrimitiveInstanceId::Id — persistent for the lifetime of the instance. */
	int32 InstanceId = INDEX_NONE;
};

/**
 * ID for a specific instance within an instanced skinned mesh component, as used by typed elements.
 * Because FPrimitiveInstanceId is already persistent, this is structurally identical to FSkMInstanceId —
 * the two types are kept separate to mirror the SMInstance pattern and allow future divergence.
 */
struct FSkMInstanceElementId
{
	explicit operator bool() const
	{
		return SkMComponent != nullptr
			&& InstanceId != INDEX_NONE;
	}

	bool operator==(const FSkMInstanceElementId& InRHS) const
	{
		return SkMComponent == InRHS.SkMComponent
			&& InstanceId == InRHS.InstanceId;
	}

	bool operator!=(const FSkMInstanceElementId& InRHS) const
	{
		return !(*this == InRHS);
	}

	friend inline uint32 GetTypeHash(const FSkMInstanceElementId& InId)
	{
		return HashCombine(GetTypeHash(InId.InstanceId), GetTypeHash(InId.SkMComponent));
	}

	UInstancedSkinnedMeshComponent* SkMComponent = nullptr;
	/** Corresponds to FPrimitiveInstanceId::Id — persistent for the lifetime of the instance. */
	int32 InstanceId = INDEX_NONE;
};

/**
 * Convert a FSkMInstanceId to the corresponding FSkMInstanceElementId.
 * Since FPrimitiveInstanceId is already persistent, this is a trivial copy.
 */
inline FSkMInstanceElementId SkMInstanceIdToElementId(const FSkMInstanceId& InInstanceId)
{
	FSkMInstanceElementId ElementId;
	ElementId.SkMComponent = InInstanceId.SkMComponent;
	ElementId.InstanceId   = InInstanceId.InstanceId;
	return ElementId;
}

/**
 * Convert a FSkMInstanceElementId to the corresponding FSkMInstanceId.
 * Since FPrimitiveInstanceId is already persistent, this is a trivial copy.
 */
inline FSkMInstanceId ElementIdToSkMInstanceId(const FSkMInstanceElementId& InElementId)
{
	FSkMInstanceId InstanceId;
	InstanceId.SkMComponent = InElementId.SkMComponent;
	InstanceId.InstanceId   = InElementId.InstanceId;
	return InstanceId;
}
