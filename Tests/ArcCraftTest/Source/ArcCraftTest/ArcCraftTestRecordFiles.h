// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ArcCoreAssetManager.h"
#include "HAL/FileManager.h"
#include "JsonAssets/ArcJsonAssetFile.h"
#include "JsonAssets/ArcJsonAssetPaths.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/UnrealType.h"

/**
 * Writes JSON record files under the ArcCraftTest plugin content root, indexes them through
 * UArcCoreAssetManager::ReindexJsonFile and removes them again (index entry + file) on
 * RemoveAll or destruction. Write returns an invalid id when the record could not be
 * written or indexed.
 */
class FArcCraftTestRecordFiles
{
public:
	FArcCraftTestRecordFiles() = default;
	FArcCraftTestRecordFiles(const FArcCraftTestRecordFiles&) = delete;
	FArcCraftTestRecordFiles& operator=(const FArcCraftTestRecordFiles&) = delete;

	~FArcCraftTestRecordFiles()
	{
		RemoveAll();
	}

	template <typename RecordType>
	FPrimaryAssetId Write(const RecordType& Record)
	{
		return WriteInstanced(FInstancedStruct::Make(Record));
	}

	FPrimaryAssetId WriteInstanced(FInstancedStruct Record)
	{
		const FArcJsonAssetTypeInfo* Type = FindType(Record.GetScriptStruct());
		const FArcJsonAssetContentRoot* Root = ArcJsonAssetPaths::GetContentRoots().Find(RootMountName);
		if (Type == nullptr || Root == nullptr)
		{
			return FPrimaryAssetId();
		}

		const FStructProperty* IdProperty = ArcJsonAssets::FindIdProperty(*Type);
		if (IdProperty == nullptr)
		{
			return FPrimaryAssetId();
		}

		const FGuid Guid = FGuid::NewGuid();
		*IdProperty->ContainerPtrToValuePtr<FGuid>(Record.GetMutableMemory()) = Guid;

		const FPrimaryAssetId Id = ArcJsonAssets::MakeRecordId(*Type, Record);
		if (!Id.IsValid())
		{
			return FPrimaryAssetId();
		}

		const FString Directory = Root->ContentDir / DirectoryName;
		const FString Filename = Directory / Guid.ToString(EGuidFormats::Digits) + TEXT(".") + Type->Extension;
		IFileManager::Get().MakeDirectory(*Directory, true);
		if (!ArcJsonAssetFile::WriteRecordFile(Filename, Record))
		{
			return FPrimaryAssetId();
		}
		Filenames.Add(Filename);
		RootContentDir = Root->ContentDir;

		if (!UArcCoreAssetManager::Get().ReindexJsonFile(Filename))
		{
			return FPrimaryAssetId();
		}
		return Id;
	}

	void RemoveAll()
	{
		for (const FString& Filename : Filenames)
		{
			UArcCoreAssetManager::Get().RemoveJsonFile(Filename);
			IFileManager::Get().Delete(*Filename, false, true, true);
		}
		Filenames.Reset();

		if (!RootContentDir.IsEmpty())
		{
			IFileManager::Get().DeleteDirectory(*(RootContentDir / DirectoryName), false, false);
			IFileManager::Get().DeleteDirectory(*RootContentDir, false, false);
			RootContentDir.Reset();
		}
	}

private:
	static const FArcJsonAssetTypeInfo* FindType(const UScriptStruct* Struct)
	{
		for (const FArcJsonAssetTypeInfo& Type : ArcJsonAssets::GetTypes())
		{
			if (Type.Struct == Struct)
			{
				return &Type;
			}
		}
		return nullptr;
	}

	static constexpr const TCHAR* RootMountName = TEXT("ArcCraftTest");
	static constexpr const TCHAR* DirectoryName = TEXT("TestRecords");

	TArray<FString> Filenames;
	FString RootContentDir;
};
