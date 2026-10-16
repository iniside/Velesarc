// Copyright Lukasz Baran. All Rights Reserved.

#include "Editing/ArcJsonRecordEditing.h"

#include "ArcJsonAssetsEditorModule.h"
#include "Core/ArcCoreAssetManager.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserItem.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserDataModule.h"
#include "IContentBrowserSingleton.h"
#include "JsonAssets/ArcJsonAssetFile.h"
#include "JsonAssets/ArcJsonAssetPaths.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "JsonAssets/ArcJsonAssetsLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcJsonRecordEditing)

#define LOCTEXT_NAMESPACE "ArcJsonAssetsEditor"

namespace ArcJsonRecordEditingPrivate
{
	void ReindexFile(const FString& Filename)
	{
		if (UArcCoreAssetManager* AssetManager = Cast<UArcCoreAssetManager>(UAssetManager::GetIfInitialized()))
		{
			AssetManager->ReindexJsonFile(Filename);
		}
	}
}

const FString& UArcJsonRecordEditObject::GetFilename() const
{
	return Filename;
}

const FInstancedStruct& UArcJsonRecordEditObject::GetRecord() const
{
	return Record;
}

FInstancedStruct& UArcJsonRecordEditObject::GetMutableRecord()
{
	return Record;
}

bool UArcJsonRecordEditObject::IsDirty() const
{
	return bDirty;
}

bool UArcJsonRecordEditObject::IsOpen() const
{
	return bOpen;
}

bool UArcJsonRecordEditObject::Save()
{
	if (!bOpen || !Record.IsValid())
	{
		return false;
	}
	TArray<uint8> Utf8;
	if (!ArcJsonAssetFile::WriteRecordFile(Filename, Record, Utf8))
	{
		return false;
	}
	DiskUtf8 = MoveTemp(Utf8);
	bDirty = false;
	ArcJsonRecordEditingPrivate::ReindexFile(Filename);
	Changed.Broadcast(this, EArcJsonRecordEditChange::Saved);
	return true;
}

void UArcJsonRecordEditObject::Discard()
{
	Close(EArcJsonRecordEditChange::Discarded);
}

FArcJsonRecordEditChanged& UArcJsonRecordEditObject::OnChanged()
{
	return Changed;
}

void UArcJsonRecordEditObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (!bOpen)
	{
		return;
	}
	bDirty = true;
	Changed.Broadcast(this, EArcJsonRecordEditChange::Edited);
}

void UArcJsonRecordEditObject::PostEditUndo()
{
	if (!bOpen)
	{
		return;
	}
	bDirty = true;
	Changed.Broadcast(this, EArcJsonRecordEditChange::UndoRedo);
}

bool UArcJsonRecordEditObject::LoadFromDisk(const FString& InFilename)
{
	Filename = InFilename;
	const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByFilename(Filename);
	if (!Type || !Type->Struct)
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls has no registered JSON asset type", *Filename);
		return false;
	}
	TArray<uint8> Utf8;
	if (!FFileHelper::LoadFileToArray(Utf8, *Filename))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls could not be read", *Filename);
		return false;
	}
	TArray<uint8> ParseBuffer = Utf8;
	FInstancedStruct Parsed;
	if (!ArcJsonAssetFile::ParseRecord(*Type, ParseBuffer, Filename, Parsed))
	{
		return false;
	}
	Record = MoveTemp(Parsed);
	DiskUtf8 = MoveTemp(Utf8);
	bDirty = false;
	return true;
}

void UArcJsonRecordEditObject::HandleFileChangedOnDisk()
{
	TArray<uint8> Utf8;
	if (!bOpen || !FFileHelper::LoadFileToArray(Utf8, *Filename) || Utf8 == DiskUtf8)
	{
		return;
	}
	if (bDirty)
	{
		UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets: %ls changed on disk while it has unsaved edits; saving overwrites the file", *Filename);
		return;
	}
	const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByFilename(Filename);
	TArray<uint8> ParseBuffer = Utf8;
	FInstancedStruct Parsed;
	if (!Type || !ArcJsonAssetFile::ParseRecord(*Type, ParseBuffer, Filename, Parsed))
	{
		return;
	}

	// The reload is its own transaction, so undoing it is an explicit user action that leaves the object dirty.
	{
		const FScopedTransaction Transaction(LOCTEXT("ReloadFromDisk", "Reload from disk"));
		Modify();
		Record = MoveTemp(Parsed);
	}
	DiskUtf8 = MoveTemp(Utf8);
	bDirty = false;
	Changed.Broadcast(this, EArcJsonRecordEditChange::Reloaded);
}

void UArcJsonRecordEditObject::HandleFileRemoved()
{
	if (!bOpen)
	{
		return;
	}
	if (bDirty)
	{
		UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets: %ls was removed from disk while it has unsaved edits; saving writes it again", *Filename);
		return;
	}
	Close(EArcJsonRecordEditChange::Removed);
}

void UArcJsonRecordEditObject::Close(EArcJsonRecordEditChange Change)
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	if (FArcJsonAssetsEditorModule* Module = FArcJsonAssetsEditorModule::GetIfLoaded())
	{
		Module->RemoveRecord(*this);
	}
	Changed.Broadcast(this, Change);
}

namespace ArcJsonRecordEditing
{
	UArcJsonRecordEditObject* AcquireRecord(const FString& Filename)
	{
		FArcJsonAssetsEditorModule* Module = FArcJsonAssetsEditorModule::GetIfLoaded();
		return Module ? Module->AcquireRecord(Filename) : nullptr;
	}

	void ReleaseRecord(UArcJsonRecordEditObject* Object)
	{
		if (FArcJsonAssetsEditorModule* Module = FArcJsonAssetsEditorModule::GetIfLoaded())
		{
			Module->ReleaseRecord(Object);
		}
	}

	UArcJsonRecordEditObject* FindRecord(const FString& Filename)
	{
		const FArcJsonAssetsEditorModule* Module = FArcJsonAssetsEditorModule::GetIfLoaded();
		return Module ? Module->FindRecord(Filename) : nullptr;
	}

	void BrowseToRecordFile(const FString& Filename)
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
		FString PackagePath;
		if (!ContentBrowserData || Filename.IsEmpty() || !ArcJsonAssetPaths::ToPackagePath(ArcJsonAssetPaths::GetContentRoots(), Filename, PackagePath))
		{
			return;
		}

		const FName VirtualPath = ContentBrowserData->ConvertInternalPathToVirtual(FName(*PackagePath));
		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get();
		const FContentBrowserItem Item = ContentBrowserData->GetItemAtPath(VirtualPath, EContentBrowserItemTypeFilter::IncludeFiles);
		if (Item.IsValid())
		{
			ContentBrowser.SyncBrowserToItems({ Item });
			return;
		}
		ContentBrowser.SyncBrowserToFolders({ FPaths::GetPath(VirtualPath.ToString()) });
	}

	bool CreateRecordFile(const FArcJsonAssetTypeInfo& Type, const FString& Filename)
	{
		const FString Normalized = ArcJsonAssetPaths::NormalizeFilename(Filename);
		const FArcJsonAssetTypeInfo* FileType = ArcJsonAssets::FindTypeByFilename(Normalized);
		if (!FileType || FileType->Extension != Type.Extension)
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls does not have the extension .%ls", *Normalized, *Type.Extension);
			return false;
		}
		const FStructProperty* IdProperty = ArcJsonAssets::FindIdProperty(Type);
		if (!IdProperty || !Type.Struct)
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: type .%ls has no id property", *Type.Extension);
			return false;
		}
		if (FPaths::FileExists(Normalized))
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls already exists", *Normalized);
			return false;
		}
		FInstancedStruct Record;
		Record.InitializeAs(Type.Struct);
		*IdProperty->ContainerPtrToValuePtr<FGuid>(Record.GetMutableMemory()) = FGuid::NewGuid();
		if (!ArcJsonAssetFile::WriteRecordFile(Normalized, Record))
		{
			return false;
		}
		ArcJsonRecordEditingPrivate::ReindexFile(Normalized);
		return true;
	}

	void RegisterRecordEditor(const FString& Extension, const FArcJsonRecordEditorOpen& Open)
	{
		if (FArcJsonAssetsEditorModule* Module = FArcJsonAssetsEditorModule::GetIfLoaded())
		{
			Module->RegisterRecordEditor(Extension, Open);
		}
	}

	void UnregisterRecordEditor(const FString& Extension)
	{
		if (FArcJsonAssetsEditorModule* Module = FArcJsonAssetsEditorModule::GetIfLoaded())
		{
			Module->UnregisterRecordEditor(Extension);
		}
	}

	bool OpenRecordEditor(const FString& Filename)
	{
		FArcJsonAssetsEditorModule* Module = FArcJsonAssetsEditorModule::GetIfLoaded();
		return Module && Module->OpenEditor(Filename);
	}
}

#undef LOCTEXT_NAMESPACE
