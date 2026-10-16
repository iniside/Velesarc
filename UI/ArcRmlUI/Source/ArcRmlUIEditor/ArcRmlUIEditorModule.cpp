// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRmlUIEditorModule.h"

#include "ArcRmlDocumentRef.h"
#include "ArcRmlUILog.h"
#include "ArcRmlUIModule.h"
#include "Brushes/SlateImageBrush.h"
#include "Compile/ArcRmlDocumentCompiler.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserFileDataCore.h"
#include "ContentBrowserFileDataPayload.h"
#include "ContentBrowserFileDataSource.h"
#include "Customization/ArcRmlDocumentRefCustomization.h"
#include "DirectoryWatcherModule.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Host/ArcRmlViewportSubsystem.h"
#include "IContentBrowserDataModule.h"
#include "IDirectoryWatcher.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "UObject/ICookInfo.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "ArcRmlUIEditor"

namespace ArcRmlUIEditorPrivate
{
	constexpr double DebounceSeconds = 0.2;
	const TCHAR* const DataSourceName = TEXT("ArcRmlData");
	const TCHAR* const PluginName = TEXT("ArcRmlUI");
	const TCHAR* const RmlUIModuleName = TEXT("ArcRmlUI");
	const TCHAR* const StyleSetName = TEXT("ArcRmlUIEditor");
	const TCHAR* const PropertyEditorModuleName = TEXT("PropertyEditor");

	bool IsSourceExtension(const FString& Extension)
	{
		return Extension == TEXT("rml") || Extension == TEXT("rcss");
	}

	bool IsDocumentResourceExtension(const FString& Extension)
	{
		return Extension == TEXT("svg") || Extension == TEXT("png") || Extension == TEXT("json");
	}

	FString StripTrailingSlashes(const FString& Path)
	{
		FString Result = Path;
		while (Result.Len() > 1 && (Result.EndsWith(TEXT("/")) || Result.EndsWith(TEXT("\\"))))
		{
			Result.LeftChopInline(1);
		}
		return Result;
	}

	FName MakeMountPath(const FArcRmlContentRoot& Root)
	{
		return FName(*StripTrailingSlashes(TEXT("/") + Root.MountName));
	}

	bool CannotCreate(const FName InDestFolderPath, const FString& InDestFolder, FText* OutErrorMsg)
	{
		ContentBrowserFileData::SetOptionalErrorMessage(OutErrorMsg, LOCTEXT("CannotCreate", "RML files are created outside the editor"));
		return false;
	}

	ContentBrowserFileData::FFileActions MakeFileActions(const TCHAR* Extension, const TCHAR* TypeName, const FText& DisplayName, const FLinearColor& Color)
	{
		ContentBrowserFileData::FFileActions FileActions;
		FileActions.TypeExtension = Extension;
		FileActions.TypeName = FTopLevelAssetPath(FName(TEXT("/Script/ArcRmlUI")), FName(TypeName));
		FileActions.TypeDisplayName = DisplayName;
		FileActions.TypeShortDescription = DisplayName;
		FileActions.TypeFullDescription = DisplayName;
		FileActions.TypeColor = Color;
		FileActions.PassesFilter.BindStatic(&ContentBrowserFileData::FDefaultFileActions::ItemPassesFilter, true);
		FileActions.GetAttribute.BindStatic(&ContentBrowserFileData::FDefaultFileActions::GetItemAttribute);
		FileActions.CanCreate.BindStatic(&CannotCreate);
		return FileActions;
	}
}

void FArcRmlUIEditorModule::StartupModule()
{
	CookStartedHandle = UE::Cook::FDelegates::CookStarted.AddRaw(this, &FArcRmlUIEditorModule::HandleCookStarted);

	if (!GIsEditor || IsRunningCommandlet())
	{
		return;
	}

	RegisterStyle();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(ArcRmlUIEditorPrivate::PropertyEditorModuleName);
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(FArcRmlDocumentRef::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcRmlDocumentRefCustomization::MakeInstance));

	FArcRmlUIModule& RmlUIModule = FArcRmlUIModule::Get();
	ContentRootChangedHandle = RmlUIModule.OnContentRootChanged().AddRaw(this, &FArcRmlUIEditorModule::HandleContentRootChanged);
	for (const FArcRmlContentRoot& Root : RmlUIModule.GetContentRoots().Roots)
	{
		MountRoot(Root);
	}

	if (GEditor)
	{
		CreateDataSource();
	}
	else
	{
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FArcRmlUIEditorModule::HandlePostEngineInit);
	}

	DebounceHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FArcRmlUIEditorModule::TickDebounce), 0.05f);
}

void FArcRmlUIEditorModule::ShutdownModule()
{
	UE::Cook::FDelegates::CookStarted.Remove(CookStartedHandle);
	FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
	FTSTicker::RemoveTicker(DebounceHandle);

	if (FArcRmlUIModule* RmlUIModule = FModuleManager::GetModulePtr<FArcRmlUIModule>(ArcRmlUIEditorPrivate::RmlUIModuleName))
	{
		RmlUIModule->OnContentRootChanged().Remove(ContentRootChangedHandle);
	}

	PreviewTabs.CloseAll();
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

	if (FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>(ArcRmlUIEditorPrivate::PropertyEditorModuleName))
	{
		PropertyEditorModule->UnregisterCustomPropertyTypeLayout(FArcRmlDocumentRef::StaticStruct()->GetFName());
	}

	UnregisterStyle();
}

void FArcRmlUIEditorModule::HandlePostEngineInit()
{
	FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
	PostEngineInitHandle.Reset();
	CreateDataSource();
}

void FArcRmlUIEditorModule::HandleContentRootChanged(const FArcRmlContentRoot& Root, bool bMounted)
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

void FArcRmlUIEditorModule::MountRoot(const FArcRmlContentRoot& Root)
{
	const FString ContentDir = ArcRmlUIEditorPrivate::StripTrailingSlashes(Root.ContentDir);

	if (DataSource.IsValid())
	{
		const FName MountPath = ArcRmlUIEditorPrivate::MakeMountPath(Root);
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
		IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FArcRmlUIEditorModule::HandleDirectoryChanged), Handle))
	{
		WatchedDirectories.Add(TPair<FString, FDelegateHandle>(ContentDir, Handle));
	}
}

void FArcRmlUIEditorModule::DismountRoot(const FArcRmlContentRoot& Root)
{
	const FString ContentDir = ArcRmlUIEditorPrivate::StripTrailingSlashes(Root.ContentDir);

	if (DataSource.IsValid())
	{
		const FName MountPath = ArcRmlUIEditorPrivate::MakeMountPath(Root);
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

void FArcRmlUIEditorModule::CreateDataSource()
{
	ContentBrowserFileData::FFileConfigData Config;

	ContentBrowserFileData::FDirectoryActions DirectoryActions;
	DirectoryActions.PassesFilter.BindStatic(&ContentBrowserFileData::FDefaultFileActions::ItemPassesFilter, false);
	DirectoryActions.GetAttribute.BindStatic(&ContentBrowserFileData::FDefaultFileActions::GetItemAttribute);
	Config.SetDirectoryActions(DirectoryActions);

	ContentBrowserFileData::FFileActions DocumentActions = ArcRmlUIEditorPrivate::MakeFileActions(TEXT("rml"), TEXT("RmlDocument"),
		LOCTEXT("RmlDocumentType", "RML Document"), FLinearColor(FColor(227, 163, 59)));
	DocumentActions.Edit.BindRaw(&PreviewTabs, &FArcRmlPreviewTabs::HandleEdit);
	Config.RegisterFileActions(DocumentActions);
	Config.RegisterFileActions(ArcRmlUIEditorPrivate::MakeFileActions(TEXT("rcss"), TEXT("RmlStyleSheet"),
		LOCTEXT("RmlStyleSheetType", "RCSS Style Sheet"), FLinearColor(FColor(59, 163, 227))));

	DataSource.Reset(NewObject<UContentBrowserFileDataSource>(GetTransientPackage(), ArcRmlUIEditorPrivate::DataSourceName));
	DataSource->Initialize(Config);

	for (const FArcRmlContentRoot& Root : FArcRmlUIModule::Get().GetContentRoots().Roots)
	{
		MountRoot(Root);
	}

	if (UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem())
	{
		ContentBrowserData->ActivateDataSource(ArcRmlUIEditorPrivate::DataSourceName);
	}
}

void FArcRmlUIEditorModule::DestroyDataSource()
{
	if (!DataSource.IsValid())
	{
		return;
	}

	if (UObjectInitialized())
	{
		if (FArcRmlUIModule* RmlUIModule = FModuleManager::GetModulePtr<FArcRmlUIModule>(ArcRmlUIEditorPrivate::RmlUIModuleName))
		{
			for (const FArcRmlContentRoot& Root : RmlUIModule->GetContentRoots().Roots)
			{
				const FName MountPath = ArcRmlUIEditorPrivate::MakeMountPath(Root);
				if (DataSource->HasFileMount(MountPath))
				{
					DataSource->RemoveFileMount(MountPath);
				}
			}
		}
		DataSource->Shutdown();
	}
	DataSource.Reset();
}

void FArcRmlUIEditorModule::RegisterStyle()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ArcRmlUIEditorPrivate::PluginName);
	if (!Plugin.IsValid())
	{
		return;
	}

	const FString ResourceDir = Plugin->GetBaseDir() / TEXT("Resources");
	const FVector2D Thumbnail64(64.0f, 64.0f);

	StyleSet = MakeShared<FSlateStyleSet>(ArcRmlUIEditorPrivate::StyleSetName);
	StyleSet->SetContentRoot(ResourceDir);
	StyleSet->Set(TEXT("ClassThumbnail.RmlDocument"), new FSlateVectorImageBrush(ResourceDir / TEXT("RmlDocument.svg"), Thumbnail64));
	StyleSet->Set(TEXT("ClassThumbnail.RmlStyleSheet"), new FSlateVectorImageBrush(ResourceDir / TEXT("RmlStyleSheet.svg"), Thumbnail64));
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FArcRmlUIEditorModule::UnregisterStyle()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}

void FArcRmlUIEditorModule::HandleDirectoryChanged(const TArray<FFileChangeData>& Changes)
{
	for (const FFileChangeData& Change : Changes)
	{
		const FString Extension = FPaths::GetExtension(Change.Filename).ToLower();
		const bool bSource = ArcRmlUIEditorPrivate::IsSourceExtension(Extension);
		if (!bSource && !ArcRmlUIEditorPrivate::IsDocumentResourceExtension(Extension))
		{
			continue;
		}
		if (bSource)
		{
			FString PackagePath;
			if (!ArcRmlPaths::ToPackagePath(FArcRmlUIModule::Get().GetContentRoots(), Change.Filename, PackagePath))
			{
				continue;
			}
			ChangedSources.Add(MoveTemp(PackagePath));
		}
		bReloadPending = true;
		LastChangeTime = FPlatformTime::Seconds();
	}
}

bool FArcRmlUIEditorModule::TickDebounce(float DeltaTime)
{
	if (bReloadPending && FPlatformTime::Seconds() - LastChangeTime >= ArcRmlUIEditorPrivate::DebounceSeconds)
	{
		bReloadPending = false;
		TArray<FString> Changed = ChangedSources.Array();
		ChangedSources.Reset();
		UArcRmlViewportSubsystem::CompileAndReloadOpenDocuments(MoveTemp(Changed));
	}
	return true;
}

void FArcRmlUIEditorModule::HandleCookStarted(UE::Cook::ICookInfo& CookInfo)
{
	if (CookInfo.GetProcessType() == UE::Cook::EProcessType::Worker)
	{
		if (!FArcRmlUIModule::Get().CaptureRegistryHash())
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: the cook worker cannot read the RmlUi registry hash");
		}
		return;
	}

	const FString ExcludedPrefixes[] = { ArcRmlPaths::CookExcludedPrefix };
	const double CompileStartSeconds = FPlatformTime::Seconds();
	FArcRmlCompileReport Report;
	if (!FArcRmlUIModule::Get().CompileAll(ExcludedPrefixes, Report))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cook CompileAll did not run; the cooked build has no compiled documents");
		return;
	}

	UE_LOGF(LogArcRmlUI, Display, "ArcRmlUI: cook CompileAll skipping '%ls': %d documents and %d style sheets, %d compiled, %d failed, %d binaries removed, %.2f ms",
		ArcRmlPaths::CookExcludedPrefix, Report.Documents, Report.StyleSheets, Report.Compiled.Num(), Report.Failed.Num(), Report.Removed.Num(),
		(FPlatformTime::Seconds() - CompileStartSeconds) * 1000.0);
	if (Report.Failed.Num() > 0)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cook CompileAll failed for %ls", *FString::Join(Report.Failed, TEXT(", ")));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcRmlUIEditorModule, ArcRmlUIEditor)
