// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcItemTestRecordFiles.h"

#include "Core/ArcCoreAssetManager.h"
#include "HAL/FileManager.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "JsonAssets/ArcJsonAssetFile.h"
#include "JsonAssets/ArcJsonAssetPaths.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "StructUtils/StructView.h"

namespace ArcItemTestRecordFilesPrivate
{
	const TCHAR* RootMountName = TEXT("ArcCoreTest");
	const TCHAR* DirectoryName = TEXT("ItemTestRecords");
}

FArcItemTestRecordFiles::~FArcItemTestRecordFiles()
{
	RemoveAll();
}

FPrimaryAssetId FArcItemTestRecordFiles::Write(const FArcItemDefinitionData& Record)
{
	const FArcJsonAssetContentRoot* Root = ArcJsonAssetPaths::GetContentRoots().Find(ArcItemTestRecordFilesPrivate::RootMountName);
	if (Root == nullptr)
	{
		return FPrimaryAssetId();
	}

	const FArcJsonAssetTypeInfo& Type = FArcItemDefinitionData::GetJsonTypeInfo();

	FArcItemDefinitionData Copy = Record;
	const FGuid Guid = FGuid::NewGuid();
	Copy.ItemId = Guid;

	const FPrimaryAssetId Id = ArcJsonAssets::MakeRecordId(Type, FConstStructView::Make(Copy));
	if (!Id.IsValid())
	{
		return FPrimaryAssetId();
	}

	const FString Directory = Root->ContentDir / ArcItemTestRecordFilesPrivate::DirectoryName;
	const FString Filename = Directory / Guid.ToString(EGuidFormats::Digits) + TEXT(".") + Type.Extension;
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!ArcJsonAssetFile::WriteRecordFile(Filename, FInstancedStruct::Make(Copy)))
	{
		return FPrimaryAssetId();
	}
	Filenames.Add(Filename);

	if (!UArcCoreAssetManager::Get().ReindexJsonFile(Filename))
	{
		return FPrimaryAssetId();
	}
	return Id;
}

FPrimaryAssetId FArcItemTestRecordFiles::WriteVariant(const FPrimaryAssetId& SourceId, TFunctionRef<void(FArcItemDefinitionData&)> Edit)
{
	const FArcItemDefinitionData* Source = UArcCoreAssetManager::Get().FindJsonRecord<FArcItemDefinitionData>(SourceId);
	if (Source == nullptr)
	{
		return FPrimaryAssetId();
	}

	FArcItemDefinitionData Copy = *Source;
	Edit(Copy);
	return Write(Copy);
}

bool FArcItemTestRecordFiles::Rewrite(const FPrimaryAssetId& Id, TFunctionRef<void(FArcItemDefinitionData&)> Edit)
{
	const FString Filename = FindOwnedFilename(Id);
	if (Filename.IsEmpty())
	{
		return false;
	}

	const FArcItemDefinitionData* Source = UArcCoreAssetManager::Get().FindJsonRecord<FArcItemDefinitionData>(Id);
	if (Source == nullptr)
	{
		return false;
	}

	FArcItemDefinitionData Copy = *Source;
	Edit(Copy);
	Copy.ItemId = Source->ItemId;
	if (!ArcJsonAssetFile::WriteRecordFile(Filename, FInstancedStruct::Make(Copy)))
	{
		return false;
	}
	return UArcCoreAssetManager::Get().ReindexJsonFile(Filename);
}

bool FArcItemTestRecordFiles::Remove(const FPrimaryAssetId& Id)
{
	const FString Filename = FindOwnedFilename(Id);
	if (Filename.IsEmpty())
	{
		return false;
	}

	const bool bRemoved = UArcCoreAssetManager::Get().RemoveJsonFile(Filename);
	IFileManager::Get().Delete(*Filename, false, true, true);
	return bRemoved;
}

FString FArcItemTestRecordFiles::FindOwnedFilename(const FPrimaryAssetId& Id) const
{
	const FString Filename = ArcJsonAssetPaths::NormalizeFilename(UArcCoreAssetManager::Get().GetJsonRecordFilename(Id));
	for (const FString& Owned : Filenames)
	{
		if (ArcJsonAssetPaths::NormalizeFilename(Owned) == Filename)
		{
			return Owned;
		}
	}
	return FString();
}

void FArcItemTestRecordFiles::RemoveAll()
{
	for (const FString& Filename : Filenames)
	{
		UArcCoreAssetManager::Get().RemoveJsonFile(Filename);
		IFileManager::Get().Delete(*Filename, false, true, true);
	}
	Filenames.Reset();

	const FArcJsonAssetContentRoot* Root = ArcJsonAssetPaths::GetContentRoots().Find(ArcItemTestRecordFilesPrivate::RootMountName);
	if (Root != nullptr)
	{
		IFileManager::Get().DeleteDirectory(*(Root->ContentDir / ArcItemTestRecordFilesPrivate::DirectoryName), false, false);
	}
}

bool ArcItemTestRecords::AddFragmentByType(FArcItemDefinitionData& Record, UScriptStruct* FragmentType)
{
	if (FragmentType == nullptr)
	{
		return false;
	}

	const FName StructName = FragmentType->GetFName();
	TSet<FArcInstancedStruct>* Set = nullptr;
	if (FragmentType->IsChildOf(FArcItemFragment::StaticStruct()))
	{
		Set = &Record.FragmentSet;
	}
	else if (FragmentType->IsChildOf(FArcScalableFloatItemFragment::StaticStruct()))
	{
		Set = &Record.ScalableFloatFragmentSet;
	}
	if (Set == nullptr || Set->FindByHash(GetTypeHash(StructName), StructName) != nullptr)
	{
		return false;
	}

	FInstancedStruct Instance;
	Instance.InitializeAs(FragmentType);
	Set->Add(FArcInstancedStruct(Instance));
	return true;
}
