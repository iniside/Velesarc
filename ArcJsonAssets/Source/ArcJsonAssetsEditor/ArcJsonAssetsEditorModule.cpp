// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcJsonAssetsEditorModule.h"

#include "ArcPrimaryAssetIdSources.h"
#include "Browser/ArcJsonAssetsFileDataSource.h"
#include "Brushes/SlateImageBrush.h"
#include "Core/ArcCoreAssetManager.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserFileDataCore.h"
#include "ContentBrowserFileDataPayload.h"
#include "DirectoryWatcherModule.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "IContentBrowserDataModule.h"
#include "IDirectoryWatcher.h"
#include "Interfaces/IPluginManager.h"
#include "JsonAssets/ArcJsonAssetFile.h"
#include "JsonAssets/ArcJsonAssetPaths.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "JsonAssets/ArcJsonAssetsLog.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "StructUtils/InstancedStruct.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Tab/SArcJsonAssetEditor.h"
#include "UObject/Package.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "ArcJsonAssetsEditor"

namespace ArcJsonAssetsEditorModulePrivate
{
	constexpr double DebounceSeconds = 0.2;
	const TCHAR* const ModuleName = TEXT("ArcJsonAssetsEditor");
	const TCHAR* const PluginName = TEXT("ArcJsonAssets");
	const TCHAR* const DataSourceName = TEXT("ArcJsonAssetsData");
	const TCHAR* const StyleSetName = TEXT("ArcJsonAssetsEditor");
	const TCHAR* const TypeNamePackage = TEXT("/Script/ArcJsonAssets");
	const TCHAR* const ThumbnailFile = TEXT("ArcJsonAsset.svg");
	const TCHAR* const TabPlaceholderId = TEXT("StandaloneToolkit");

	FName MakeMountPath(const FArcJsonAssetContentRoot& Root)
	{
		return FName(TEXT("/") + Root.MountName);
	}

	FString MakeThumbnailStyleName(const FArcJsonAssetTypeInfo& Type)
	{
		return TEXT("ClassThumbnail.") + Type.PrimaryAssetType.ToString();
	}

	bool IsUnderDirectory(const FString& Filename, const FString& Directory)
	{
		return Filename.StartsWith(Directory + TEXT("/"));
	}

	UArcCoreAssetManager* GetAssetManager()
	{
		return Cast<UArcCoreAssetManager>(UAssetManager::GetIfInitialized());
	}

	bool CannotDuplicate(const FName InFilePath, const FString& InFilename, FText* OutErrorMsg)
	{
		ContentBrowserFileData::SetOptionalErrorMessage(OutErrorMsg,
			LOCTEXT("CannotDuplicate", "JSON assets cannot be duplicated: the copy would repeat the record id"));
		return false;
	}

	bool CreateFile(const FName InFilePath, const FString& InFilename, const FStructOnScope& CreationConfig)
	{
		const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByFilename(InFilename);
		if (!Type)
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls has no registered JSON asset type", *InFilename);
			return false;
		}
		return ArcJsonRecordEditing::CreateRecordFile(*Type, InFilename);
	}

	bool EditFile(const FName InFilePath, const FString& InFilename)
	{
		FArcJsonAssetsEditorModule* Module = FArcJsonAssetsEditorModule::GetIfLoaded();
		return Module && Module->OpenEditor(InFilename);
	}

	ContentBrowserFileData::FFileActions MakeFileActions(const FArcJsonAssetTypeInfo& Type)
	{
		ContentBrowserFileData::FFileActions FileActions;
		FileActions.TypeExtension = Type.Extension;
		FileActions.TypeName = FTopLevelAssetPath(FName(TypeNamePackage), Type.PrimaryAssetType.GetName());
		FileActions.TypeDisplayName = Type.DisplayName;
		FileActions.TypeShortDescription = Type.DisplayName;
		FileActions.TypeFullDescription = Type.DisplayName;
		FileActions.DefaultNewFileName = TEXT("New") + Type.PrimaryAssetType.ToString();
		FileActions.TypeColor = Type.Color;
		FileActions.PassesFilter.BindStatic(&ContentBrowserFileData::FDefaultFileActions::ItemPassesFilter, true);
		FileActions.GetAttribute.BindStatic(&ContentBrowserFileData::FDefaultFileActions::GetItemAttribute);
		FileActions.Create.BindStatic(&CreateFile);
		FileActions.CanDuplicate.BindStatic(&CannotDuplicate);
		FileActions.Edit.BindStatic(&EditFile);
		return FileActions;
	}

	TArray<FArcPrimaryAssetIdSourceEntry> GetIdSourceEntries(FPrimaryAssetType PrimaryAssetType)
	{
		TArray<FArcPrimaryAssetIdSourceEntry> Result;
		if (const UArcCoreAssetManager* AssetManager = GetAssetManager())
		{
			for (const FArcJsonRecordEntry& Entry : AssetManager->GetJsonRecordEntries(PrimaryAssetType))
			{
				Result.Add({ Entry.Id, Entry.DisplayName });
			}
		}
		return Result;
	}

	void BrowseToId(const FPrimaryAssetId& Id)
	{
		if (const UArcCoreAssetManager* AssetManager = GetAssetManager())
		{
			ArcJsonRecordEditing::BrowseToRecordFile(AssetManager->GetJsonRecordFilename(Id));
		}
	}

	void ResaveCommand(const TArray<FString>& Args)
	{
		const FString Argument = FString::Join(Args, TEXT(" ")).TrimStartAndEnd();
		if (Argument.IsEmpty())
		{
			UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.Resave <absolute filename or package path>");
			return;
		}
		FString Filename = Argument;
		if (!FPaths::FileExists(Filename))
		{
			ArcJsonAssetPaths::ToFilename(ArcJsonAssetPaths::GetContentRoots(), Argument, Filename);
		}
		Filename = ArcJsonAssetPaths::NormalizeFilename(Filename);

		const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByFilename(Filename);
		if (!Type || !FPaths::FileExists(Filename))
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.Resave: %ls is not an existing JSON asset file", *Argument);
			return;
		}
		FInstancedStruct Record;
		if (!ArcJsonAssetFile::ReadRecordFile(*Type, Filename, Record))
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.Resave: %ls was not written because it does not parse", *Filename);
			return;
		}
		if (ArcJsonAssetFile::WriteRecordFile(Filename, Record))
		{
			UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Resave: wrote %ls", *Filename);
		}
	}

	FAutoConsoleCommand ResaveConsoleCommand(
		TEXT("ArcJsonAssets.Resave"),
		TEXT("ArcJsonAssets.Resave <absolute filename or package path>: parses a JSON asset file and writes it back in the editor's save format."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ResaveCommand));
}

FArcJsonAssetsEditorModule* FArcJsonAssetsEditorModule::GetIfLoaded()
{
	return FModuleManager::GetModulePtr<FArcJsonAssetsEditorModule>(ArcJsonAssetsEditorModulePrivate::ModuleName);
}

void FArcJsonAssetsEditorModule::StartupModule()
{
	if (!GIsEditor || IsRunningCommandlet())
	{
		return;
	}

	if (GEditor)
	{
		CreateDataSource();
	}
	else
	{
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FArcJsonAssetsEditorModule::HandlePostEngineInit);
	}

	DebounceHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FArcJsonAssetsEditorModule::TickDebounce), 0.05f);
}

void FArcJsonAssetsEditorModule::ShutdownModule()
{
	FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
	FTSTicker::RemoveTicker(DebounceHandle);

	ArcJsonAssetPaths::OnContentRootChanged().Remove(ContentRootChangedHandle);

	UnregisterIdSources();
	DestroyDataSource();

	FDirectoryWatcherModule* WatcherModule = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* Watcher = WatcherModule ? WatcherModule->Get() : nullptr;
	if (Watcher)
	{
		for (const TPair<FString, FDelegateHandle>& Entry : WatchedDirectories)
		{
			Watcher->UnregisterDirectoryChangedCallback_Handle(Entry.Key, Entry.Value);
		}
	}
	WatchedDirectories.Reset();
	PendingFiles.Reset();
	PendingDirectories.Reset();
	OpenEditors.Reset();
	RecordObjects.Reset();
	RecordEditors.Reset();

	UnregisterStyle();
}

bool FArcJsonAssetsEditorModule::OpenEditor(const FString& Filename)
{
	const FString Normalized = ArcJsonAssetPaths::NormalizeFilename(Filename);
	const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByFilename(Normalized);
	if (!Type)
	{
		return false;
	}
	if (const FArcJsonRecordEditorOpen* RecordEditor = RecordEditors.Find(Type->Extension))
	{
		return RecordEditor->IsBound() && RecordEditor->Execute(Normalized);
	}

	if (const TWeakPtr<SDockTab>* Existing = OpenEditors.Find(Normalized))
	{
		if (const TSharedPtr<SDockTab> ExistingTab = Existing->Pin())
		{
			ExistingTab->ActivateInParent(ETabActivationCause::SetDirectly);
			FGlobalTabmanager::Get()->DrawAttention(ExistingTab.ToSharedRef());
			return true;
		}
		OpenEditors.Remove(Normalized);
	}

	const TSharedRef<SArcJsonAssetEditor> Editor = SNew(SArcJsonAssetEditor)
		.Filename(Normalized)
		.OnRecordClosed(FSimpleDelegate::CreateStatic(&FArcJsonAssetsEditorModule::HandleEditorRecordClosed, Normalized));
	if (!Editor->IsLoaded())
	{
		return false;
	}

	const TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(TAttribute<FText>::CreateSP(&Editor.Get(), &SArcJsonAssetEditor::GetTabLabel))
		.OnCanCloseTab(SDockTab::FCanCloseTab::CreateSP(Editor, &SArcJsonAssetEditor::CanClose))
		.OnTabClosed(SDockTab::FOnTabClosedCallback::CreateStatic(&FArcJsonAssetsEditorModule::HandleEditorTabClosed, Normalized, TWeakPtr<SArcJsonAssetEditor>(Editor)))
		[
			Editor
		];
	if (StyleSet.IsValid())
	{
		Tab->SetTabIcon(StyleSet->GetBrush(*ArcJsonAssetsEditorModulePrivate::MakeThumbnailStyleName(*Type)));
	}

	OpenEditors.Add(Normalized, Tab);
	FGlobalTabmanager::Get()->InsertNewDocumentTab(ArcJsonAssetsEditorModulePrivate::TabPlaceholderId,
		FTabManager::ESearchPreference::PreferLiveTab, Tab);
	if (const TSharedPtr<SWindow> Window = Tab->GetParentWindow())
	{
		Window->BringToFront();
	}
	return true;
}

void FArcJsonAssetsEditorModule::HandleEditorTabClosed(TSharedRef<SDockTab> Tab, FString Filename, TWeakPtr<SArcJsonAssetEditor> Editor)
{
	if (const TSharedPtr<SArcJsonAssetEditor> PinnedEditor = Editor.Pin())
	{
		PinnedEditor->HandleTabClosed();
	}
	FArcJsonAssetsEditorModule* Module = GetIfLoaded();
	if (!Module)
	{
		return;
	}
	const TWeakPtr<SDockTab>* Existing = Module->OpenEditors.Find(Filename);
	if (Existing && Existing->Pin() == Tab)
	{
		Module->OpenEditors.Remove(Filename);
	}
}

void FArcJsonAssetsEditorModule::HandleEditorRecordClosed(FString Filename)
{
	FArcJsonAssetsEditorModule* Module = GetIfLoaded();
	const TWeakPtr<SDockTab>* Existing = Module ? Module->OpenEditors.Find(Filename) : nullptr;
	if (const TSharedPtr<SDockTab> Tab = Existing ? Existing->Pin() : nullptr)
	{
		Tab->RequestCloseTab();
	}
}

UArcJsonRecordEditObject* FArcJsonAssetsEditorModule::AcquireRecord(const FString& Filename)
{
	const FString Normalized = ArcJsonAssetPaths::NormalizeFilename(Filename);
	if (const TStrongObjectPtr<UArcJsonRecordEditObject>* Existing = RecordObjects.Find(Normalized))
	{
		UArcJsonRecordEditObject* Object = Existing->Get();
		++Object->HolderCount;
		return Object;
	}

	UArcJsonRecordEditObject* Object = NewObject<UArcJsonRecordEditObject>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional);
	if (!Object->LoadFromDisk(Normalized))
	{
		return nullptr;
	}
	Object->bOpen = true;
	Object->HolderCount = 1;
	RecordObjects.Add(Normalized, TStrongObjectPtr<UArcJsonRecordEditObject>(Object));
	return Object;
}

void FArcJsonAssetsEditorModule::ReleaseRecord(UArcJsonRecordEditObject* Object)
{
	if (!Object || Object->HolderCount <= 0)
	{
		return;
	}
	--Object->HolderCount;
	if (Object->HolderCount == 0 && Object->bOpen)
	{
		Object->bOpen = false;
		RemoveRecord(*Object);
	}
}

UArcJsonRecordEditObject* FArcJsonAssetsEditorModule::FindRecord(const FString& Filename) const
{
	const TStrongObjectPtr<UArcJsonRecordEditObject>* Existing = RecordObjects.Find(ArcJsonAssetPaths::NormalizeFilename(Filename));
	return Existing ? Existing->Get() : nullptr;
}

void FArcJsonAssetsEditorModule::RemoveRecord(const UArcJsonRecordEditObject& Object)
{
	const TStrongObjectPtr<UArcJsonRecordEditObject>* Existing = RecordObjects.Find(Object.Filename);
	if (Existing && Existing->Get() == &Object)
	{
		RecordObjects.Remove(Object.Filename);
	}
}

void FArcJsonAssetsEditorModule::RegisterRecordEditor(const FString& Extension, const FArcJsonRecordEditorOpen& Open)
{
	RecordEditors.Add(Extension, Open);
}

void FArcJsonAssetsEditorModule::UnregisterRecordEditor(const FString& Extension)
{
	RecordEditors.Remove(Extension);
}

void FArcJsonAssetsEditorModule::HandlePostEngineInit()
{
	FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
	PostEngineInitHandle.Reset();
	CreateDataSource();
}

void FArcJsonAssetsEditorModule::HandleContentRootChanged(const FArcJsonAssetContentRoot& Root, bool bMounted)
{
	if (bMounted)
	{
		MountRoot(Root);
	}
	else
	{
		DismountRoot(Root);
	}
}

void FArcJsonAssetsEditorModule::MountRoot(const FArcJsonAssetContentRoot& Root)
{
	const FString& ContentDir = Root.ContentDir;

	if (DataSource.IsValid())
	{
		const FName MountPath = ArcJsonAssetsEditorModulePrivate::MakeMountPath(Root);
		if (!DataSource->HasFileMount(MountPath))
		{
			DataSource->AddFileMount(MountPath, ContentDir);
		}
	}

	if (!IFileManager::Get().DirectoryExists(*ContentDir)
		|| WatchedDirectories.ContainsByPredicate([&ContentDir](const TPair<FString, FDelegateHandle>& Entry) -> bool { return Entry.Key == ContentDir; }))
	{
		return;
	}

	FDirectoryWatcherModule& WatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* Watcher = WatcherModule.Get();
	if (!Watcher)
	{
		return;
	}

	FDelegateHandle Handle;
	if (Watcher->RegisterDirectoryChangedCallback_Handle(ContentDir,
		IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FArcJsonAssetsEditorModule::HandleDirectoryChanged, ContentDir), Handle,
		IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges))
	{
		WatchedDirectories.Add(TPair<FString, FDelegateHandle>(ContentDir, Handle));
	}
}

void FArcJsonAssetsEditorModule::DismountRoot(const FArcJsonAssetContentRoot& Root)
{
	const FString& ContentDir = Root.ContentDir;

	if (DataSource.IsValid())
	{
		const FName MountPath = ArcJsonAssetsEditorModulePrivate::MakeMountPath(Root);
		if (DataSource->HasFileMount(MountPath))
		{
			DataSource->RemoveFileMount(MountPath);
		}
	}

	const int32 Index = WatchedDirectories.IndexOfByPredicate([&ContentDir](const TPair<FString, FDelegateHandle>& Entry) -> bool { return Entry.Key == ContentDir; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	FDirectoryWatcherModule* WatcherModule = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* Watcher = WatcherModule ? WatcherModule->Get() : nullptr;
	if (Watcher)
	{
		Watcher->UnregisterDirectoryChangedCallback_Handle(WatchedDirectories[Index].Key, WatchedDirectories[Index].Value);
	}
	WatchedDirectories.RemoveAt(Index);
}

void FArcJsonAssetsEditorModule::CreateDataSource()
{
	ContentBrowserFileData::FFileConfigData Config;

	ContentBrowserFileData::FDirectoryActions DirectoryActions;
	DirectoryActions.PassesFilter.BindStatic(&ContentBrowserFileData::FDefaultFileActions::ItemPassesFilter, false);
	DirectoryActions.GetAttribute.BindStatic(&ContentBrowserFileData::FDefaultFileActions::GetItemAttribute);
	Config.SetDirectoryActions(DirectoryActions);

	for (const FArcJsonAssetTypeInfo& Type : ArcJsonAssets::GetTypes())
	{
		Config.RegisterFileActions(ArcJsonAssetsEditorModulePrivate::MakeFileActions(Type));
		DataSourceExtensions.Add(Type.Extension);
	}

	DataSource.Reset(NewObject<UArcJsonAssetsFileDataSource>(GetTransientPackage(), ArcJsonAssetsEditorModulePrivate::DataSourceName));
	DataSource->Initialize(Config);

	ContentRootChangedHandle = ArcJsonAssetPaths::OnContentRootChanged().AddRaw(this, &FArcJsonAssetsEditorModule::HandleContentRootChanged);
	for (const FArcJsonAssetContentRoot& Root : ArcJsonAssetPaths::GetContentRoots().Roots)
	{
		MountRoot(Root);
	}

	if (UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem())
	{
		ContentBrowserData->ActivateDataSource(ArcJsonAssetsEditorModulePrivate::DataSourceName);
	}

	RegisterStyle();
	RegisterIdSources();
}

void FArcJsonAssetsEditorModule::DestroyDataSource()
{
	if (!DataSource.IsValid())
	{
		return;
	}

	if (UObjectInitialized())
	{
		for (const FArcJsonAssetContentRoot& Root : ArcJsonAssetPaths::GetContentRoots().Roots)
		{
			const FName MountPath = ArcJsonAssetsEditorModulePrivate::MakeMountPath(Root);
			if (DataSource->HasFileMount(MountPath))
			{
				DataSource->RemoveFileMount(MountPath);
			}
		}
		DataSource->Shutdown();
	}
	DataSource.Reset();
	DataSourceExtensions.Reset();
}

void FArcJsonAssetsEditorModule::RegisterStyle()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ArcJsonAssetsEditorModulePrivate::PluginName);
	if (!Plugin.IsValid() || StyleSet.IsValid())
	{
		return;
	}

	const FString ResourceDir = Plugin->GetBaseDir() / TEXT("Resources");
	const FString ThumbnailPath = ResourceDir / ArcJsonAssetsEditorModulePrivate::ThumbnailFile;
	const FVector2D Thumbnail64(64.0f, 64.0f);

	StyleSet = MakeShared<FSlateStyleSet>(ArcJsonAssetsEditorModulePrivate::StyleSetName);
	StyleSet->SetContentRoot(ResourceDir);
	for (const FArcJsonAssetTypeInfo& Type : ArcJsonAssets::GetTypes())
	{
		StyleSet->Set(*ArcJsonAssetsEditorModulePrivate::MakeThumbnailStyleName(Type), new FSlateVectorImageBrush(ThumbnailPath, Thumbnail64));
	}
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FArcJsonAssetsEditorModule::UnregisterStyle()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}

void FArcJsonAssetsEditorModule::RegisterIdSources()
{
	UArcCoreAssetManager* AssetManager = ArcJsonAssetsEditorModulePrivate::GetAssetManager();
	if (!AssetManager)
	{
		return;
	}
	for (const FPrimaryAssetType& Type : AssetManager->GetJsonRecordTypes())
	{
		AddIdSource(Type);
	}
	if (!JsonRecordChangedHandle.IsValid())
	{
		JsonRecordChangedHandle = AssetManager->OnJsonRecordChanged().AddRaw(this, &FArcJsonAssetsEditorModule::HandleJsonRecordChanged);
	}
}

void FArcJsonAssetsEditorModule::AddIdSource(const FPrimaryAssetType& Type)
{
	if (IdSourceTypes.Contains(Type))
	{
		return;
	}
	FArcPrimaryAssetIdSource Source;
	Source.GetEntries.BindStatic(&ArcJsonAssetsEditorModulePrivate::GetIdSourceEntries, Type);
	Source.BrowseTo.BindStatic(&ArcJsonAssetsEditorModulePrivate::BrowseToId);
	ArcPrimaryAssetIdSources::Register(Type, Source);
	IdSourceTypes.Add(Type);
}

void FArcJsonAssetsEditorModule::HandleJsonRecordChanged(const FPrimaryAssetId& Id)
{
	AddIdSource(Id.PrimaryAssetType);
}

void FArcJsonAssetsEditorModule::UnregisterIdSources()
{
	if (JsonRecordChangedHandle.IsValid())
	{
		if (UArcCoreAssetManager* AssetManager = ArcJsonAssetsEditorModulePrivate::GetAssetManager())
		{
			AssetManager->OnJsonRecordChanged().Remove(JsonRecordChangedHandle);
		}
		JsonRecordChangedHandle.Reset();
	}
	for (const FPrimaryAssetType& Type : IdSourceTypes)
	{
		ArcPrimaryAssetIdSources::Unregister(Type);
	}
	IdSourceTypes.Reset();
}

void FArcJsonAssetsEditorModule::HandleDirectoryChanged(const TArray<FFileChangeData>& Changes, FString ContentDir)
{
	for (const FFileChangeData& Change : Changes)
	{
		const FString Filename = ArcJsonAssetPaths::NormalizeFilename(Change.Filename);
		if (!ArcJsonAssetsEditorModulePrivate::IsUnderDirectory(Filename, ContentDir))
		{
			continue;
		}

		if (const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByFilename(Filename))
		{
			if (DataSource.IsValid() && !DataSourceExtensions.Contains(Type->Extension) && !WarnedLateExtensions.Contains(Type->Extension))
			{
				WarnedLateExtensions.Add(Type->Extension);
				UE_LOGF(LogArcJsonAssets, Warning,
					"ArcJsonAssets: type .%ls was registered after the Content Browser data source was created; its files get no browser items, editor tab or id picker until the editor restarts",
					*Type->Extension);
			}
			PendingFiles.Add(Filename);
		}
		else if (FPaths::GetExtension(Filename).IsEmpty()
			&& (Change.Action == FFileChangeData::FCA_Added || Change.Action == FFileChangeData::FCA_Removed))
		{
			PendingDirectories.Add(Filename);
		}
		else
		{
			continue;
		}
		bChangesPending = true;
		LastChangeTime = FPlatformTime::Seconds();
	}
}

bool FArcJsonAssetsEditorModule::TickDebounce(float DeltaTime)
{
	if (bChangesPending && FPlatformTime::Seconds() - LastChangeTime >= ArcJsonAssetsEditorModulePrivate::DebounceSeconds)
	{
		bChangesPending = false;
		ProcessPendingChanges();
	}
	return true;
}

void FArcJsonAssetsEditorModule::ProcessPendingChanges()
{
	UArcCoreAssetManager* AssetManager = ArcJsonAssetsEditorModulePrivate::GetAssetManager();

	TSet<FString> Removed;
	TSet<FString> Present;
	for (const FString& Filename : PendingFiles)
	{
		if (FPaths::FileExists(Filename))
		{
			Present.Add(Filename);
		}
		else
		{
			Removed.Add(Filename);
		}
	}

	// A folder rename or move arrives as Removed/Added of the folder path itself (DirectoryWatchRequestWindows.cpp:313-320).
	for (const FString& Directory : PendingDirectories)
	{
		if (FPaths::DirectoryExists(Directory))
		{
			FPlatformFileManager::Get().GetPlatformFile().IterateDirectoryRecursively(*Directory,
				[&Present](const TCHAR* Path, bool bIsDirectory) -> bool
				{
					if (!bIsDirectory && ArcJsonAssets::FindTypeByFilename(Path))
					{
						Present.Add(ArcJsonAssetPaths::NormalizeFilename(Path));
					}
					return true;
				});
			continue;
		}

		if (AssetManager)
		{
			for (const FPrimaryAssetType& Type : AssetManager->GetJsonRecordTypes())
			{
				for (const FArcJsonRecordEntry& Entry : AssetManager->GetJsonRecordEntries(Type))
				{
					const FString Filename = AssetManager->GetJsonRecordFilename(Entry.Id);
					if (ArcJsonAssetsEditorModulePrivate::IsUnderDirectory(Filename, Directory))
					{
						Removed.Add(Filename);
					}
				}
			}
		}
		for (const TPair<FString, TStrongObjectPtr<UArcJsonRecordEditObject>>& Pair : RecordObjects)
		{
			if (ArcJsonAssetsEditorModulePrivate::IsUnderDirectory(Pair.Key, Directory))
			{
				Removed.Add(Pair.Key);
			}
		}
	}
	PendingFiles.Reset();
	PendingDirectories.Reset();

	TArray<FString> RemovedFiles;
	for (const FString& Filename : Removed)
	{
		if (!Present.Contains(Filename))
		{
			RemovedFiles.Add(Filename);
		}
	}
	TArray<FString> PresentFiles = Present.Array();
	RemovedFiles.Sort();
	PresentFiles.Sort();

	for (const FString& Filename : RemovedFiles)
	{
		if (AssetManager)
		{
			AssetManager->RemoveJsonFile(Filename);
		}
		NotifyRecord(Filename, false);
	}
	for (const FString& Filename : PresentFiles)
	{
		if (AssetManager)
		{
			AssetManager->ReindexJsonFile(Filename);
		}
		NotifyRecord(Filename, true);
	}
}

void FArcJsonAssetsEditorModule::NotifyRecord(const FString& Filename, bool bExists)
{
	UArcJsonRecordEditObject* Object = FindRecord(Filename);
	if (!Object)
	{
		return;
	}
	if (bExists)
	{
		Object->HandleFileChangedOnDisk();
	}
	else
	{
		Object->HandleFileRemoved();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcJsonAssetsEditorModule, ArcJsonAssetsEditor)
