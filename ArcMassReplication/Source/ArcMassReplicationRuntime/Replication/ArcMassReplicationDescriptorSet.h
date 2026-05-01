// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Fragments/ArcMassReplicationFilter.h"

namespace ArcMassReplication
{

struct ARCMASSREPLICATIONRUNTIME_API FArcMassReplicationDescriptorSet
{
	TArray<TRefCountPtr<const UE::Net::FReplicationStateDescriptor>> Descriptors;
	TArray<const UScriptStruct*> FragmentTypes;
	TArray<EArcMassReplicationFilter> FragmentFilters;
	uint32 Hash = 0;

	static FArcMassReplicationDescriptorSet Build(const TArray<const UScriptStruct*>& InFragmentTypes);
	static FArcMassReplicationDescriptorSet Build(const TArray<FArcMassReplicatedFragmentEntry>& InEntries);

	static uint32 ComputeHash(const TArray<const UScriptStruct*>& InTypes);
};

} // namespace ArcMassReplication
