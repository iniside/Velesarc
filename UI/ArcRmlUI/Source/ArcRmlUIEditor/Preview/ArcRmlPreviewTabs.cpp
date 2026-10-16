// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcRmlPreviewTabs.h"

#include "ArcRmlUILog.h"
#include "ArcRmlUIModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Misc/Paths.h"
#include "Preview/SArcRmlDocumentPreview.h"
#include "Widgets/Docking/SDockTab.h"

namespace ArcRmlPreviewTabsPrivate
{
	const FName PlaceholderTabId(TEXT("StandaloneToolkit"));
}

bool FArcRmlPreviewTabs::HandleEdit(const FName InFilePath, const FString& InFilename)
{
	FString DocumentPath;
	if (!ArcRmlPaths::ToPackagePath(FArcRmlUIModule::Get().GetContentRoots(), InFilename, DocumentPath))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' is not under an RML content root", *InFilename);
		return false;
	}
	Open(DocumentPath);
	return true;
}

void FArcRmlPreviewTabs::Open(const FString& DocumentPath)
{
	if (const FOpenTab* Existing = Tabs.Find(DocumentPath))
	{
		if (const TSharedPtr<SDockTab> ExistingTab = Existing->Tab.Pin())
		{
			FGlobalTabmanager::Get()->DrawAttention(ExistingTab.ToSharedRef());
			return;
		}
	}

	const TSharedRef<SArcRmlDocumentPreview> Preview = SNew(SArcRmlDocumentPreview, DocumentPath);
	const TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(FText::FromString(FPaths::GetCleanFilename(DocumentPath)))
		.ToolTipText(FText::FromString(DocumentPath))
		.OnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FArcRmlPreviewTabs::HandleTabClosed, DocumentPath))
		[
			Preview
		];

	FOpenTab& OpenTab = Tabs.Add(DocumentPath);
	OpenTab.Tab = Tab;
	OpenTab.Preview = Preview;
	FGlobalTabmanager::Get()->InsertNewDocumentTab(ArcRmlPreviewTabsPrivate::PlaceholderTabId, FTabManager::FLiveTabSearch(), Tab);
	if (const TSharedPtr<SWindow> Window = Tab->GetParentWindow())
	{
		Window->BringToFront();
	}
}

void FArcRmlPreviewTabs::CloseAll()
{
	TArray<FOpenTab> OpenTabs;
	Tabs.GenerateValueArray(OpenTabs);
	Tabs.Reset();

	for (const FOpenTab& OpenTab : OpenTabs)
	{
		if (const TSharedPtr<SArcRmlDocumentPreview> Preview = OpenTab.Preview.Pin())
		{
			Preview->Shutdown();
		}
		const TSharedPtr<SDockTab> Tab = OpenTab.Tab.Pin();
		if (Tab.IsValid())
		{
			Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback());
			if (FSlateApplication::IsInitialized())
			{
				Tab->RequestCloseTab();
			}
		}
	}
}

void FArcRmlPreviewTabs::HandleTabClosed(TSharedRef<SDockTab> ClosedTab, FString DocumentPath)
{
	FOpenTab OpenTab;
	if (!Tabs.RemoveAndCopyValue(DocumentPath, OpenTab))
	{
		return;
	}
	if (const TSharedPtr<SArcRmlDocumentPreview> Preview = OpenTab.Preview.Pin())
	{
		Preview->Shutdown();
	}
}
