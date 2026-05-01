// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcMassReplicationDescriptorSet.h"
#include "Fragments/ArcMassReplicationFilter.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"

namespace ArcMassReplication
{

FArcMassReplicationDescriptorSet FArcMassReplicationDescriptorSet::Build(const TArray<const UScriptStruct*>& InFragmentTypes)
{
	FArcMassReplicationDescriptorSet Result;

	for (int32 Index = 0; Index < InFragmentTypes.Num(); ++Index)
	{
		const UScriptStruct* StructType = InFragmentTypes[Index];
		Result.FragmentTypes.Add(StructType);
		Result.FragmentFilters.Add(EArcMassReplicationFilter::Spatial);

		TRefCountPtr<const UE::Net::FReplicationStateDescriptor> Descriptor =
			UE::Net::FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(StructType);

		if (!Descriptor.IsValid() || Descriptor->MemberCount == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("FArcMassReplicationDescriptorSet: no valid Iris descriptor for %s — fragment will be skipped during serialization"),
				*StructType->GetName());
		}

		Result.Descriptors.Add(MoveTemp(Descriptor));
	}

	Result.Hash = ComputeHash(Result.FragmentTypes);
	return Result;
}

FArcMassReplicationDescriptorSet FArcMassReplicationDescriptorSet::Build(const TArray<FArcMassReplicatedFragmentEntry>& InEntries)
{
	FArcMassReplicationDescriptorSet Result;

	for (const FArcMassReplicatedFragmentEntry& Entry : InEntries)
	{
		const UScriptStruct* StructType = Entry.FragmentType;
		if (!StructType)
		{
			continue;
		}

		Result.FragmentTypes.Add(StructType);
		Result.FragmentFilters.Add(Entry.Filter);

		TRefCountPtr<const UE::Net::FReplicationStateDescriptor> Descriptor =
			UE::Net::FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(StructType);

		if (!Descriptor.IsValid() || Descriptor->MemberCount == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("FArcMassReplicationDescriptorSet: no valid Iris descriptor for %s — fragment will be skipped during serialization"),
				*StructType->GetName());
		}

		Result.Descriptors.Add(MoveTemp(Descriptor));
	}

	Result.Hash = ComputeHash(Result.FragmentTypes);
	return Result;
}

uint32 FArcMassReplicationDescriptorSet::ComputeHash(const TArray<const UScriptStruct*>& InTypes)
{
	TArray<const UScriptStruct*> Sorted = InTypes;
	Algo::Sort(Sorted, [](const UScriptStruct* A, const UScriptStruct* B)
	{
		return A->GetFName().LexicalLess(B->GetFName());
	});

	uint32 ResultHash = 0;
	for (const UScriptStruct* Type : Sorted)
	{
		ResultHash = HashCombine(ResultHash, ::GetTypeHash(Type));
	}
	return ResultHash;
}

} // namespace ArcMassReplication
