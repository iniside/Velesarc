// Copyright Lukasz Baran. All Rights Reserved.

#include "PCG/ArcPCGPlacedEntityPartitionActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPCGPlacedEntityPartitionActor)

AArcPCGPlacedEntityPartitionActor::AArcPCGPlacedEntityPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool AArcPCGPlacedEntityPartitionActor::CanEditSMInstance(const FSMInstanceId& InstanceId) const
{
	return false;
}

bool AArcPCGPlacedEntityPartitionActor::CanMoveSMInstance(const FSMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const
{
	return false;
}

bool AArcPCGPlacedEntityPartitionActor::DeleteSMInstances(TArrayView<const FSMInstanceId> InstanceIds)
{
	return false;
}

bool AArcPCGPlacedEntityPartitionActor::DuplicateSMInstances(TArrayView<const FSMInstanceId> InstanceIds, TArray<FSMInstanceId>& OutNewInstanceIds)
{
	return false;
}
