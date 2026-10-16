// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemDefinitionData.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/PrimaryAssetId.h"

/**
 * Writes item definition records (.arcitem) under the ArcCoreTest plugin content root, indexes
 * them through UArcCoreAssetManager::ReindexJsonFile and removes them again (index entry + file)
 * on RemoveAll or destruction. Write and WriteVariant return an invalid id when the record could
 * not be written or indexed. Rewrite edits the file of a record written by this object and
 * reindexes it under the same id; Remove drops the index entry and deletes the file. Both return
 * false for an id this object did not write.
 */
class ARCSHAREDTESTHARNESS_API FArcItemTestRecordFiles
{
public:
	FArcItemTestRecordFiles() = default;
	FArcItemTestRecordFiles(const FArcItemTestRecordFiles&) = delete;
	FArcItemTestRecordFiles& operator=(const FArcItemTestRecordFiles&) = delete;
	~FArcItemTestRecordFiles();

	FPrimaryAssetId Write(const FArcItemDefinitionData& Record);

	FPrimaryAssetId WriteVariant(const FPrimaryAssetId& SourceId, TFunctionRef<void(FArcItemDefinitionData&)> Edit);

	bool Rewrite(const FPrimaryAssetId& Id, TFunctionRef<void(FArcItemDefinitionData&)> Edit);

	bool Remove(const FPrimaryAssetId& Id);

	void RemoveAll();

private:
	FString FindOwnedFilename(const FPrimaryAssetId& Id) const;

	TArray<FString> Filenames;
};

namespace ArcItemTestRecords
{
	template <typename T>
	void AddFragment(FArcItemDefinitionData& Record, const T& Fragment)
	{
		FInstancedStruct Instance;
		Instance.InitializeAs<T>(Fragment);
		Record.FragmentSet.Add(FArcInstancedStruct(Instance));
	}

	template <typename T>
	void AddScalableFragment(FArcItemDefinitionData& Record, const T& Fragment)
	{
		FInstancedStruct Instance;
		Instance.InitializeAs<T>(Fragment);
		Record.ScalableFloatFragmentSet.Add(FArcInstancedStruct(Instance));
	}

	ARCSHAREDTESTHARNESS_API bool AddFragmentByType(FArcItemDefinitionData& Record, UScriptStruct* FragmentType);

	template <typename T>
	void SetStackMethod(FArcItemDefinitionData& Record, const T& StackMethod)
	{
		Record.StackMethod.InitializeAs<T>(StackMethod);
	}
}
