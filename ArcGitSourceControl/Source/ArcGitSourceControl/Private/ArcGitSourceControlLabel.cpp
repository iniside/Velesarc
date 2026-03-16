// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcGitSourceControlLabel.h"
#include "ArcGitSourceControlUtils.h"
#include "ArcGitSourceControlRevision.h"

bool FArcGitSourceControlLabel::GetFileRevisions(const FString& InFile, TArray<TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe>>& OutRevisions) const
{
	TArray<FString> Results;
	TArray<FString> Errors;
	TArray<FString> Parameters;
	Parameters.Add(TagName);
	Parameters.Add(TEXT("-1"));
	Parameters.Add(TEXT("--format=%H|%h|%an|%ai|%s"));
	Parameters.Add(TEXT("--"));
	Parameters.Add(InFile);

	bool bResult = ArcGitSourceControlUtils::RunCommand(TEXT("log"), PathToGitBinary, PathToRepositoryRoot, Parameters, TArray<FString>(), Results, Errors);

	if (bResult && Results.Num() > 0)
	{
		TArray<FString> Parts;
		Results[0].ParseIntoArray(Parts, TEXT("|"), false);
		if (Parts.Num() >= 5)
		{
			TSharedRef<FArcGitSourceControlRevision> Revision = MakeShared<FArcGitSourceControlRevision>();
			Revision->CommitId = Parts[0];
			Revision->ShortCommitId = Parts[1];
			Revision->CommitIdNumber = FCString::Strtoi(*Parts[1].Left(8), nullptr, 16);
			Revision->UserName = Parts[2];
			Revision->Description = Parts[4];
			Revision->Filename = InFile;
			Revision->RevisionNumber = 0;
			Revision->PathToGitBinary = PathToGitBinary;
			Revision->PathToRepositoryRoot = PathToRepositoryRoot;
			OutRevisions.Add(Revision);
		}
	}
	return bResult;
}

bool FArcGitSourceControlLabel::GetFileRevisions(const TArray<FString>& InFiles, TArray<TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe>>& OutRevisions) const
{
	bool bResult = true;
	for (const FString& File : InFiles)
	{
		bResult &= GetFileRevisions(File, OutRevisions);
	}
	return bResult;
}

bool FArcGitSourceControlLabel::Sync(const FString& InFilename) const
{
	TArray<FString> Results;
	TArray<FString> Errors;
	TArray<FString> Parameters;
	Parameters.Add(TagName);
	Parameters.Add(TEXT("--"));
	Parameters.Add(InFilename);

	return ArcGitSourceControlUtils::RunCommand(TEXT("checkout"), PathToGitBinary, PathToRepositoryRoot, Parameters, TArray<FString>(), Results, Errors);
}

bool FArcGitSourceControlLabel::Sync(const TArray<FString>& InFilenames) const
{
	bool bResult = true;
	for (const FString& Filename : InFilenames)
	{
		bResult &= Sync(Filename);
	}
	return bResult;
}
