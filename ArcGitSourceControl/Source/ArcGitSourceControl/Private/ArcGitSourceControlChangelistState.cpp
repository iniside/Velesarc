// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcGitSourceControlChangelistState.h"

#define LOCTEXT_NAMESPACE "ArcGitSourceControl.ChangelistState"

FName FArcGitSourceControlChangelistState::GetIconName() const
{
	return FName("RevisionControl.Changelist");
}

FName FArcGitSourceControlChangelistState::GetSmallIconName() const
{
	return GetIconName();
}

FText FArcGitSourceControlChangelistState::GetDisplayText() const
{
	if (Changelist.IsDefault())
	{
		return LOCTEXT("DefaultChangelist", "Default Changelist");
	}
	return FText::FromString(FString::Printf(TEXT("CL %d: %s"), Changelist.Id, *Description));
}

FText FArcGitSourceControlChangelistState::GetDescriptionText() const
{
	return FText::FromString(Description);
}

FText FArcGitSourceControlChangelistState::GetDisplayTooltip() const
{
	if (Changelist.IsDefault())
	{
		return LOCTEXT("DefaultChangelistTooltip", "Files not assigned to any changelist");
	}
	return FText::FromString(FString::Printf(TEXT("Changelist %d - %s (%d files)"), Changelist.Id, *Description, Files.Num()));
}

const TArray<FSourceControlStateRef> FArcGitSourceControlChangelistState::GetShelvedFilesStates() const
{
	TArray<FSourceControlStateRef> Result;
	for (const auto& Pair : ShelvedFiles)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}

int32 FArcGitSourceControlChangelistState::GetShelvedFilesStatesNum() const
{
	return ShelvedFiles.Num();
}

FSourceControlChangelistRef FArcGitSourceControlChangelistState::GetChangelist() const
{
	return MakeShared<FArcGitSourceControlChangelist>(Changelist.Id);
}

#undef LOCTEXT_NAMESPACE
