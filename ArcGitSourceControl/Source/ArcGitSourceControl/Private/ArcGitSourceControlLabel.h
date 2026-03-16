// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlLabel.h"

class FArcGitSourceControlLabel : public ISourceControlLabel
{
public:
	FString TagName;
	FString PathToGitBinary;
	FString PathToRepositoryRoot;

	FArcGitSourceControlLabel() = default;
	explicit FArcGitSourceControlLabel(const FString& InTagName)
		: TagName(InTagName)
	{
	}

	virtual const FString& GetName() const override { return TagName; }
	virtual bool GetFileRevisions(const FString& InFile, TArray<TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe>>& OutRevisions) const override;
	virtual bool GetFileRevisions(const TArray<FString>& InFiles, TArray<TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe>>& OutRevisions) const override;
	virtual bool Sync(const FString& InFilename) const override;
	virtual bool Sync(const TArray<FString>& InFilenames) const override;
};
