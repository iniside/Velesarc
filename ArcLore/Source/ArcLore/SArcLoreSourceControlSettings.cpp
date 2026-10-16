// Copyright Epic Games, Inc. All Rights Reserved.

#include "SArcLoreSourceControlSettings.h"
#include "ArcLoreSourceControlModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SArcLoreSourceControlSettings"

void SArcLoreSourceControlSettings::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		// Repository info header
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("RepositoryInfo", "Lore Repository"))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]
		// Separator
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f, 2.0f, 2.0f, 8.0f)
		[
			SNew(SSeparator)
		]
		// Repository info (read-only)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SArcLoreSourceControlSettings::GetStatusText)
		]
	];
}

FText SArcLoreSourceControlSettings::GetStatusText() const
{
	const FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	const FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	FString Status;
	Status += FString::Printf(TEXT("Repository: %s\n"), *Provider.GetPathToRepositoryRoot());
	Status += FString::Printf(TEXT("Identity: %s\n"), *Provider.GetUserName());
	Status += FString::Printf(TEXT("Remote: %s\n"), Provider.GetRemoteUrl().IsEmpty() ? TEXT("none") : *Provider.GetRemoteUrl());
	Status += FString::Printf(TEXT("Branch: %s"), Provider.GetBranchName().IsEmpty() ? TEXT("(unknown)") : *Provider.GetBranchName());

	return FText::FromString(Status);
}

#undef LOCTEXT_NAMESPACE
