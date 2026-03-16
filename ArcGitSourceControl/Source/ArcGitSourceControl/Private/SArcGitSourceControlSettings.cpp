// Copyright Epic Games, Inc. All Rights Reserved.

#include "SArcGitSourceControlSettings.h"
#include "ArcGitSourceControlModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SArcGitSourceControlSettings"

void SArcGitSourceControlSettings::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		// Git Binary Path
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BinaryPathLabel", "Git Path"))
				.ToolTipText(LOCTEXT("BinaryPathTooltip", "Path to git executable. Leave empty to use git from PATH."))
			]
			+SHorizontalBox::Slot()
			.FillWidth(2.0f)
			[
				SNew(SEditableTextBox)
				.Text(this, &SArcGitSourceControlSettings::GetBinaryPathText)
				.HintText(LOCTEXT("BinaryPathHint", "git (from PATH)"))
				.OnTextCommitted(this, &SArcGitSourceControlSettings::OnBinaryPathTextCommitted)
			]
		]
		// Auto-push checkbox
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AutoPushLabel", "Auto-push on Check In"))
				.ToolTipText(LOCTEXT("AutoPushTooltip", "Automatically push to remote after each check-in operation."))
			]
			+SHorizontalBox::Slot()
			.FillWidth(2.0f)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SArcGitSourceControlSettings::GetAutoPushCheckState)
				.OnCheckStateChanged(this, &SArcGitSourceControlSettings::OnAutoPushCheckStateChanged)
			]
		]
		// Separator
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f, 8.0f, 2.0f, 2.0f)
		[
			SNew(SSeparator)
		]
		// Repository info header
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("RepositoryInfo", "Repository Info"))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]
		// Repository info (read-only)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SArcGitSourceControlSettings::GetStatusText)
		]
	];
}

FText SArcGitSourceControlSettings::GetBinaryPathText() const
{
	const FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	return FText::FromString(Module.AccessSettings().GetBinaryPath());
}

void SArcGitSourceControlSettings::OnBinaryPathTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	const bool bChanged = Module.AccessSettings().SetBinaryPath(InText.ToString());
	if (bChanged)
	{
		Module.GetProvider().CheckGitAvailability();
		if (Module.GetProvider().IsGitAvailable())
		{
			Module.SaveSettings();
		}
	}
}

ECheckBoxState SArcGitSourceControlSettings::GetAutoPushCheckState() const
{
	const FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	return Module.AccessSettings().GetAutoPushOnCheckIn()
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SArcGitSourceControlSettings::OnAutoPushCheckStateChanged(ECheckBoxState NewState)
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	Module.AccessSettings().SetAutoPushOnCheckIn(NewState == ECheckBoxState::Checked);
	Module.SaveSettings();
}

FText SArcGitSourceControlSettings::GetStatusText() const
{
	const FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	const FArcGitSourceControlProvider& Provider = Module.GetProvider();

	FString Status;
	Status += FString::Printf(TEXT("Repository: %s\n"), *Provider.GetPathToRepositoryRoot());
	Status += FString::Printf(TEXT("Branch: %s\n"), *Provider.GetBranchName());
	Status += FString::Printf(TEXT("Remote: %s\n"), Provider.GetRemoteUrl().IsEmpty() ? TEXT("none") : *Provider.GetRemoteUrl());
	Status += FString::Printf(TEXT("User: %s (%s)\n"), *Provider.GetUserName(), *Provider.GetUserEmail());

	const FArcGitVersion& Version = Provider.GetGitVersion();
	Status += FString::Printf(TEXT("Git: %d.%d"), Version.Major, Version.Minor);
	Status += FString::Printf(TEXT(" | LFS: %s"), Version.bHasGitLfs ? TEXT("Yes") : TEXT("No"));
	Status += FString::Printf(TEXT(" | LFS Locking: %s"), Version.bHasGitLfsLocking ? TEXT("Available") : TEXT("Not Available"));

	return FText::FromString(Status);
}

#undef LOCTEXT_NAMESPACE
