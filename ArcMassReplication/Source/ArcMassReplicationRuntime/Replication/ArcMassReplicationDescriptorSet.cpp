// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcMassReplicationDescriptorSet.h"
#include "Fragments/ArcMassReplicationFilter.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcDescriptorSet, Verbose, All);

namespace ArcMassReplication
{

FArcMassReplicationDescriptorSet FArcMassReplicationDescriptorSet::Build(const TArray<const UScriptStruct*>& InFragmentTypes)
{
	UE_LOG(LogArcDescriptorSet, Log, TEXT("Build(types): enter InFragmentTypes=%d"), InFragmentTypes.Num());

	FArcMassReplicationDescriptorSet Result;

	for (int32 Index = 0; Index < InFragmentTypes.Num(); ++Index)
	{
		const UScriptStruct* StructType = InFragmentTypes[Index];
		UE_LOG(LogArcDescriptorSet, Log, TEXT("Build(types): [%d] StructType=%s"), Index, StructType ? *StructType->GetName() : TEXT("null"));

		Result.FragmentTypes.Add(StructType);
		Result.FragmentFilters.Add(EArcMassReplicationFilter::Spatial);

		TRefCountPtr<const UE::Net::FReplicationStateDescriptor> Descriptor =
			UE::Net::FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(StructType);

		bool bDescValid = Descriptor.IsValid();
		int32 MemberCount = bDescValid ? (int32)Descriptor->MemberCount : -1;
		UE_LOG(LogArcDescriptorSet, Log, TEXT("Build(types): [%d] StructType=%s Descriptor=%s MemberCount=%d"),
			Index,
			StructType ? *StructType->GetName() : TEXT("null"),
			bDescValid ? TEXT("valid") : TEXT("null/invalid"),
			MemberCount);

		if (!bDescValid || Descriptor->MemberCount == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("FArcMassReplicationDescriptorSet: no valid Iris descriptor for %s — fragment will be skipped during serialization"),
				*StructType->GetName());
		}

		Result.Descriptors.Add(MoveTemp(Descriptor));
	}

	Result.Hash = ComputeHash(Result.FragmentTypes);
	UE_LOG(LogArcDescriptorSet, Log, TEXT("Build(types): exit Hash=%u FragmentTypes=%d Descriptors=%d"), Result.Hash, Result.FragmentTypes.Num(), Result.Descriptors.Num());
	return Result;
}

FArcMassReplicationDescriptorSet FArcMassReplicationDescriptorSet::Build(const TArray<FArcMassReplicatedFragmentEntry>& InEntries)
{
	UE_LOG(LogArcDescriptorSet, Log, TEXT("Build(entries): enter InEntries=%d"), InEntries.Num());

	FArcMassReplicationDescriptorSet Result;

	for (int32 Index = 0; Index < InEntries.Num(); ++Index)
	{
		const FArcMassReplicatedFragmentEntry& Entry = InEntries[Index];
		const UScriptStruct* StructType = Entry.FragmentType;
		if (!StructType)
		{
			UE_LOG(LogArcDescriptorSet, Warning, TEXT("Build(entries): [%d] StructType null, skipping"), Index);
			continue;
		}

		UE_LOG(LogArcDescriptorSet, Log, TEXT("Build(entries): [%d] StructType=%s Filter=%d"), Index, *StructType->GetName(), (int32)Entry.Filter);

		Result.FragmentTypes.Add(StructType);
		Result.FragmentFilters.Add(Entry.Filter);

		TRefCountPtr<const UE::Net::FReplicationStateDescriptor> Descriptor =
			UE::Net::FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(StructType);

		bool bDescValid = Descriptor.IsValid();
		int32 MemberCount = bDescValid ? (int32)Descriptor->MemberCount : -1;
		UE_LOG(LogArcDescriptorSet, Log, TEXT("Build(entries): [%d] StructType=%s Descriptor=%s MemberCount=%d"),
			Index,
			*StructType->GetName(),
			bDescValid ? TEXT("valid") : TEXT("null/invalid"),
			MemberCount);

		if (!bDescValid || Descriptor->MemberCount == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("FArcMassReplicationDescriptorSet: no valid Iris descriptor for %s — fragment will be skipped during serialization"),
				*StructType->GetName());
		}

		Result.Descriptors.Add(MoveTemp(Descriptor));
	}

	Result.Hash = ComputeHash(Result.FragmentTypes);
	UE_LOG(LogArcDescriptorSet, Log, TEXT("Build(entries): exit Hash=%u FragmentTypes=%d Descriptors=%d"), Result.Hash, Result.FragmentTypes.Num(), Result.Descriptors.Num());
	return Result;
}

uint32 FArcMassReplicationDescriptorSet::ComputeHash(const TArray<const UScriptStruct*>& InTypes)
{
	UE_LOG(LogArcDescriptorSet, Verbose, TEXT("ComputeHash: enter InTypes=%d"), InTypes.Num());

	TArray<const UScriptStruct*> Sorted = InTypes;
	Algo::Sort(Sorted, [](const UScriptStruct* A, const UScriptStruct* B)
	{
		return A->GetFName().LexicalLess(B->GetFName());
	});

	uint32 ResultHash = 0;
	for (int32 i = 0; i < Sorted.Num(); ++i)
	{
		UE_LOG(LogArcDescriptorSet, Verbose, TEXT("ComputeHash: sorted[%d]=%s"), i, Sorted[i] ? *Sorted[i]->GetName() : TEXT("null"));
		ResultHash = HashCombine(ResultHash, ::GetTypeHash(Sorted[i]));
	}

	UE_LOG(LogArcDescriptorSet, Verbose, TEXT("ComputeHash: exit Hash=%u"), ResultHash);
	return ResultHash;
}

} // namespace ArcMassReplication
