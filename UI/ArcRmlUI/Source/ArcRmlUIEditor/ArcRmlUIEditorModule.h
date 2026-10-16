// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"
#include "Preview/ArcRmlPreviewTabs.h"
#include "UObject/StrongObjectPtr.h"

class FSlateStyleSet;
class UContentBrowserFileDataSource;
struct FArcRmlContentRoot;
struct FFileChangeData;

namespace UE::Cook
{
	class ICookInfo;
}

class FArcRmlUIEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void HandlePostEngineInit();
	void HandleContentRootChanged(const FArcRmlContentRoot& Root, bool bMounted);
	void MountRoot(const FArcRmlContentRoot& Root);
	void DismountRoot(const FArcRmlContentRoot& Root);
	void CreateDataSource();
	void DestroyDataSource();
	void RegisterStyle();
	void UnregisterStyle();
	void HandleDirectoryChanged(const TArray<FFileChangeData>& Changes);
	bool TickDebounce(float DeltaTime);
	void HandleCookStarted(UE::Cook::ICookInfo& CookInfo);

	FArcRmlPreviewTabs PreviewTabs;
	TStrongObjectPtr<UContentBrowserFileDataSource> DataSource;
	TSharedPtr<FSlateStyleSet> StyleSet;
	TArray<TPair<FString, FDelegateHandle>> WatchedDirectories;
	TSet<FString> ChangedSources;
	FTSTicker::FDelegateHandle DebounceHandle;
	FDelegateHandle CookStartedHandle;
	FDelegateHandle PostEngineInitHandle;
	FDelegateHandle ContentRootChangedHandle;
	double LastChangeTime = 0.0;
	bool bReloadPending = false;
};
