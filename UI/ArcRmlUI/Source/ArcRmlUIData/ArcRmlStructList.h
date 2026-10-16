// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"

#include "ArcRmlStructList.generated.h"

USTRUCT(BlueprintType)
struct ARCRMLUIDATA_API FArcRmlStructList
{
	GENERATED_BODY()

	void SetRowType(const UScriptStruct* InRowType);
	const UScriptStruct* GetRowType() const
	{
		return RowType;
	}

	bool Set(const FString& Key, FConstStructView Row);

	template <typename T>
	bool Set(const FString& Key, const T& Row)
	{
		return Set(Key, FConstStructView::Make(Row));
	}

	bool Remove(const FString& Key);
	void SetOrder(TConstArrayView<FString> InKeys);
	void Reset();

	int32 Num() const
	{
		return Rows.Num();
	}

	const FInstancedStruct* Find(const FString& Key) const;

	template <typename T>
	const T* FindAs(const FString& Key) const
	{
		const FInstancedStruct* Row = Find(Key);
		return Row ? Row->GetPtr<T>() : nullptr;
	}

	const TArray<FString>& GetKeys() const
	{
		return Keys;
	}

	const TArray<FInstancedStruct>& GetRows() const
	{
		return Rows;
	}

	const TArray<uint64>& GetRowVersions() const
	{
		return RowVersions;
	}

	uint64 GetStructureVersion() const
	{
		return StructureVersion;
	}

private:
	void RebuildIndex();

	UPROPERTY(Transient)
	TObjectPtr<const UScriptStruct> RowType;

	UPROPERTY(Transient)
	TArray<FString> Keys;

	UPROPERTY(Transient)
	TArray<FInstancedStruct> Rows;

	TArray<uint64> RowVersions;
	TMap<FString, int32> KeyToIndex;
	uint64 StructureVersion = 0;
};
