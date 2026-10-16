// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Elements/Framework/TypedElementData.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementId.h"

/** Registered typed element type name for Mass entity instances. */
ARCMASS_API extern const FName NAME_ArcMassEntity;

/**
 * Element data that represents a specific Mass entity instance managed by a
 * componentless placed entity partition actor.
 */
struct FArcMassEntityElementData
{
	ARCMASS_API UE_DECLARE_TYPED_ELEMENT_DATA_RTTI(FArcMassEntityElementData);

	FArcMassEntityElementId InstanceElementId;
};

template <>
inline FString GetTypedElementDebugId<FArcMassEntityElementData>(const FArcMassEntityElementData& InElementData)
{
	return InElementData.InstanceElementId.PartitionActor.IsValid()
		? FString::Printf(TEXT("ArcMassEntity:%d:%d"), InElementData.InstanceElementId.EntryIndex, InElementData.InstanceElementId.TransformIndex)
		: TEXT("null");
}
