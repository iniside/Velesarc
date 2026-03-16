// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlChangelistState.h"
#include "ArcGitSourceControlChangelist.h"

class FArcGitSourceControlChangelistState : public ISourceControlChangelistState
{
public:
	FArcGitSourceControlChangelist Changelist;
	FString Description;
	TArray<FSourceControlStateRef> Files;
	TMap<FString, FSourceControlStateRef> ShelvedFiles;
	FDateTime Timestamp;

	FArcGitSourceControlChangelistState() = default;
	explicit FArcGitSourceControlChangelistState(int32 InId)
		: Changelist(InId)
		, Timestamp(FDateTime::Now())
	{
	}

	virtual FName GetIconName() const override;
	virtual FName GetSmallIconName() const override;
	virtual FText GetDisplayText() const override;
	virtual FText GetDescriptionText() const override;
	virtual bool SupportsPersistentDescription() const override { return true; }
	virtual FText GetDisplayTooltip() const override;
	virtual const FDateTime& GetTimeStamp() const override { return Timestamp; }
	virtual const TArray<FSourceControlStateRef> GetFilesStates() const override { return Files; }
	virtual int32 GetFilesStatesNum() const override { return Files.Num(); }
	virtual const TArray<FSourceControlStateRef> GetShelvedFilesStates() const override;
	virtual int32 GetShelvedFilesStatesNum() const override;
	virtual FSourceControlChangelistRef GetChangelist() const override;
};
