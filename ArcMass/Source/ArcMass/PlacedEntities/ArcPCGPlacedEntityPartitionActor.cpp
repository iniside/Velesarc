// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcPCGPlacedEntityPartitionActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPCGPlacedEntityPartitionActor)

AArcPCGPlacedEntityPartitionActor::AArcPCGPlacedEntityPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool AArcPCGPlacedEntityPartitionActor::CanEditMassEntityInstance(const FArcMassEntityElementId& InId) const
{
	return false;
}

bool AArcPCGPlacedEntityPartitionActor::CanMoveMassEntityInstance(const FArcMassEntityElementId& InId, ETypedElementWorldType WorldType) const
{
	return false;
}

bool AArcPCGPlacedEntityPartitionActor::DeleteMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds)
{
	return false;
}

bool AArcPCGPlacedEntityPartitionActor::DuplicateMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds, TArray<FArcMassEntityElementId>& OutNewIds)
{
	return false;
}
