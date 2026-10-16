// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRmlStructList.h"

#include <atomic>

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcRmlStructList)

namespace ArcRmlStructListPrivate
{
	std::atomic<uint64> VersionCounter{0};

	uint64 NextVersion()
	{
		return VersionCounter.fetch_add(1, std::memory_order_relaxed) + 1;
	}
}

void FArcRmlStructList::SetRowType(const UScriptStruct* InRowType)
{
	if (RowType == InRowType)
	{
		return;
	}
	if (!ensureMsgf(RowType == nullptr && Rows.Num() == 0, TEXT("FArcRmlStructList row type is set once, before the first row")))
	{
		return;
	}
	RowType = InRowType;
}

bool FArcRmlStructList::Set(const FString& Key, FConstStructView Row)
{
	const UScriptStruct* Struct = Row.GetScriptStruct();
	if (!Struct || !Row.GetMemory())
	{
		return false;
	}
	if (!RowType)
	{
		SetRowType(Struct);
	}
	if (!ensureMsgf(Struct == RowType, TEXT("FArcRmlStructList rows must be '%s', got '%s'"), *GetNameSafe(RowType), *Struct->GetName()))
	{
		return false;
	}

	if (const int32* FoundIndex = KeyToIndex.Find(Key))
	{
		FInstancedStruct& Existing = Rows[*FoundIndex];
		if (RowType->CompareScriptStruct(Existing.GetMemory(), Row.GetMemory(), PPF_None))
		{
			return false;
		}
		RowType->CopyScriptStruct(Existing.GetMutableMemory(), Row.GetMemory());
		RowVersions[*FoundIndex] = ArcRmlStructListPrivate::NextVersion();
		return true;
	}

	const int32 Index = Rows.Num();
	Keys.Add(Key);
	Rows.AddDefaulted_GetRef().InitializeAs(Struct, Row.GetMemory());
	RowVersions.Add(ArcRmlStructListPrivate::NextVersion());
	KeyToIndex.Add(Key, Index);
	StructureVersion = ArcRmlStructListPrivate::NextVersion();
	return true;
}

bool FArcRmlStructList::Remove(const FString& Key)
{
	int32 Index = INDEX_NONE;
	if (!KeyToIndex.RemoveAndCopyValue(Key, Index))
	{
		return false;
	}

	Keys.RemoveAt(Index);
	Rows.RemoveAt(Index);
	RowVersions.RemoveAt(Index);
	for (int32 Shifted = Index; Shifted < Keys.Num(); ++Shifted)
	{
		KeyToIndex[Keys[Shifted]] = Shifted;
	}
	StructureVersion = ArcRmlStructListPrivate::NextVersion();
	return true;
}

void FArcRmlStructList::SetOrder(TConstArrayView<FString> InKeys)
{
	TArray<int32> Order;
	Order.Reserve(Rows.Num());
	TBitArray<> Placed(false, Rows.Num());
	for (const FString& Key : InKeys)
	{
		const int32* FoundIndex = KeyToIndex.Find(Key);
		if (FoundIndex && !Placed[*FoundIndex])
		{
			Placed[*FoundIndex] = true;
			Order.Add(*FoundIndex);
		}
	}
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (!Placed[Index])
		{
			Order.Add(Index);
		}
	}

	bool bChanged = false;
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		if (Order[Index] != Index)
		{
			bChanged = true;
			break;
		}
	}
	if (!bChanged)
	{
		return;
	}

	TArray<FString> NewKeys;
	TArray<FInstancedStruct> NewRows;
	TArray<uint64> NewVersions;
	NewKeys.Reserve(Order.Num());
	NewRows.Reserve(Order.Num());
	NewVersions.Reserve(Order.Num());
	for (const int32 OldIndex : Order)
	{
		NewKeys.Add(MoveTemp(Keys[OldIndex]));
		NewRows.Add(MoveTemp(Rows[OldIndex]));
		NewVersions.Add(RowVersions[OldIndex]);
	}
	Keys = MoveTemp(NewKeys);
	Rows = MoveTemp(NewRows);
	RowVersions = MoveTemp(NewVersions);
	RebuildIndex();
	StructureVersion = ArcRmlStructListPrivate::NextVersion();
}

void FArcRmlStructList::Reset()
{
	if (Rows.Num() == 0)
	{
		return;
	}
	Keys.Reset();
	Rows.Reset();
	RowVersions.Reset();
	KeyToIndex.Reset();
	StructureVersion = ArcRmlStructListPrivate::NextVersion();
}

const FInstancedStruct* FArcRmlStructList::Find(const FString& Key) const
{
	const int32* FoundIndex = KeyToIndex.Find(Key);
	return FoundIndex ? &Rows[*FoundIndex] : nullptr;
}

void FArcRmlStructList::RebuildIndex()
{
	KeyToIndex.Reset();
	KeyToIndex.Reserve(Keys.Num());
	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		KeyToIndex.Add(Keys[Index], Index);
	}
}
