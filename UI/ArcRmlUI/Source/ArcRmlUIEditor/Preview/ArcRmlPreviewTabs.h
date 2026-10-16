// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SArcRmlDocumentPreview;
class SDockTab;

class FArcRmlPreviewTabs
{
public:
	bool HandleEdit(const FName InFilePath, const FString& InFilename);
	void Open(const FString& DocumentPath);
	void CloseAll();

private:
	struct FOpenTab
	{
		TWeakPtr<SDockTab> Tab;
		TWeakPtr<SArcRmlDocumentPreview> Preview;
	};

	void HandleTabClosed(TSharedRef<SDockTab> ClosedTab, FString DocumentPath);

	TMap<FString, FOpenTab> Tabs;
};
