// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Editing/ArcJsonRecordEditing.h"
#include "Modules/ModuleInterface.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/StrongObjectPtr.h"

class FSlateStyleSet;
class SArcJsonAssetEditor;
class SDockTab;
class UArcJsonAssetsFileDataSource;
struct FArcJsonAssetContentRoot;
struct FFileChangeData;

class FArcJsonAssetsEditorModule : public IModuleInterface
{
public:
	static FArcJsonAssetsEditorModule* GetIfLoaded();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool OpenEditor(const FString& Filename);

	UArcJsonRecordEditObject* AcquireRecord(const FString& Filename);
	void ReleaseRecord(UArcJsonRecordEditObject* Object);
	UArcJsonRecordEditObject* FindRecord(const FString& Filename) const;
	void RemoveRecord(const UArcJsonRecordEditObject& Object);
	void RegisterRecordEditor(const FString& Extension, const FArcJsonRecordEditorOpen& Open);
	void UnregisterRecordEditor(const FString& Extension);

private:
	static void HandleEditorTabClosed(TSharedRef<SDockTab> Tab, FString Filename, TWeakPtr<SArcJsonAssetEditor> Editor);
	static void HandleEditorRecordClosed(FString Filename);

	void HandlePostEngineInit();
	void HandleContentRootChanged(const FArcJsonAssetContentRoot& Root, bool bMounted);
	void MountRoot(const FArcJsonAssetContentRoot& Root);
	void DismountRoot(const FArcJsonAssetContentRoot& Root);
	void CreateDataSource();
	void DestroyDataSource();
	void RegisterStyle();
	void UnregisterStyle();
	void RegisterIdSources();
	void AddIdSource(const FPrimaryAssetType& Type);
	void HandleJsonRecordChanged(const FPrimaryAssetId& Id);
	void UnregisterIdSources();
	void HandleDirectoryChanged(const TArray<FFileChangeData>& Changes, FString ContentDir);
	bool TickDebounce(float DeltaTime);
	void ProcessPendingChanges();
	void NotifyRecord(const FString& Filename, bool bExists);

	TStrongObjectPtr<UArcJsonAssetsFileDataSource> DataSource;
	TSharedPtr<FSlateStyleSet> StyleSet;
	TArray<TPair<FString, FDelegateHandle>> WatchedDirectories;
	TArray<FPrimaryAssetType> IdSourceTypes;
	TSet<FString> DataSourceExtensions;
	TSet<FString> WarnedLateExtensions;
	TSet<FString> PendingFiles;
	TSet<FString> PendingDirectories;
	TMap<FString, TWeakPtr<SDockTab>> OpenEditors;
	TMap<FString, TStrongObjectPtr<UArcJsonRecordEditObject>> RecordObjects;
	TMap<FString, FArcJsonRecordEditorOpen> RecordEditors;
	FTSTicker::FDelegateHandle DebounceHandle;
	FDelegateHandle PostEngineInitHandle;
	FDelegateHandle ContentRootChangedHandle;
	FDelegateHandle JsonRecordChangedHandle;
	double LastChangeTime = 0.0;
	bool bChangesPending = false;
};
