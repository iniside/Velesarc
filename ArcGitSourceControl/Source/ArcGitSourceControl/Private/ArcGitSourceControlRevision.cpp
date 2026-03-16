// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcGitSourceControlRevision.h"

#include "ISourceControlModule.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ArcGitSourceControlModule.h"
#include "ArcGitSourceControlUtils.h"

#define LOCTEXT_NAMESPACE "ArcGitSourceControl"

bool FArcGitSourceControlRevision::Get(FString& InOutFilename, EConcurrency::Type InConcurrency) const
{
	if (InConcurrency != EConcurrency::Synchronous)
	{
		UE_LOG(LogSourceControl, Warning, TEXT("Only EConcurrency::Synchronous is tested/supported for this operation."));
	}

	FArcGitSourceControlModule& GitSourceControl = FModuleManager::LoadModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	const FString GitBinary = GitSourceControl.AccessSettings().GetBinaryPath();
	const FString RepoRoot = GitSourceControl.GetProvider().GetPathToRepositoryRoot();

	// if a filename for the temp file wasn't supplied generate a unique-ish one
	if(InOutFilename.Len() == 0)
	{
		// create the diff dir if we don't already have it (Git wont)
		IFileManager::Get().MakeDirectory(*FPaths::DiffDir(), true);
		// create a unique temp file name based on the unique commit Id
		const FString TempFileName = FString::Printf(TEXT("%stemp-%s-%s"), *FPaths::DiffDir(), *CommitId, *FPaths::GetCleanFilename(Filename));
		InOutFilename = FPaths::ConvertRelativePathToFull(TempFileName);
	}

	// Diff against the revision
	const FString Parameter = FString::Printf(TEXT("%s:%s"), *CommitId, *Filename);

	bool bCommandSuccessful;
	if(FPaths::FileExists(InOutFilename))
	{
		bCommandSuccessful = true; // if the temp file already exists, reuse it directly
	}
	else
	{
		bCommandSuccessful = ArcGitSourceControlUtils::RunDumpToFile(GitBinary, RepoRoot, Parameter, InOutFilename);
	}
	return bCommandSuccessful;
}

bool FArcGitSourceControlRevision::GetAnnotated( TArray<FAnnotationLine>& OutLines ) const
{
	FString Binary = PathToGitBinary;
	FString Root = PathToRepositoryRoot;
	if (Binary.IsEmpty())
	{
		FArcGitSourceControlModule& Module = FModuleManager::LoadModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
		Binary = Module.AccessSettings().GetBinaryPath();
		Root = Module.GetProvider().GetPathToRepositoryRoot();
	}
	TArray<FString> Errors;
	return ArcGitSourceControlUtils::RunBlame(Binary, Root, CommitId, Filename, OutLines, Errors);
}

bool FArcGitSourceControlRevision::GetAnnotated( FString& InOutFilename ) const
{
	FArcGitSourceControlModule& Module = FModuleManager::LoadModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	const FString Binary = Module.AccessSettings().GetBinaryPath();
	const FString Root = Module.GetProvider().GetPathToRepositoryRoot();
	TArray<FString> Errors;
	return ArcGitSourceControlUtils::RunBlameToFile(Binary, Root, CommitId, Filename, InOutFilename, Errors);
}

const FString& FArcGitSourceControlRevision::GetFilename() const
{
	return Filename;
}

int32 FArcGitSourceControlRevision::GetRevisionNumber() const
{
	return RevisionNumber;
}

const FString& FArcGitSourceControlRevision::GetRevision() const
{
	return ShortCommitId;
}

const FString& FArcGitSourceControlRevision::GetDescription() const
{
	return Description;
}

const FString& FArcGitSourceControlRevision::GetUserName() const
{
	return UserName;
}

const FString& FArcGitSourceControlRevision::GetClientSpec() const
{
	static FString EmptyString(TEXT(""));
	return EmptyString;
}

const FString& FArcGitSourceControlRevision::GetAction() const
{
	return Action;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FArcGitSourceControlRevision::GetBranchSource() const
{
	// if this revision was copied/moved from some other revision
	return BranchSource;
}

const FDateTime& FArcGitSourceControlRevision::GetDate() const
{
	return Date;
}

int32 FArcGitSourceControlRevision::GetCheckInIdentifier() const
{
	return CommitIdNumber;
}

int32 FArcGitSourceControlRevision::GetFileSize() const
{
	return FileSize;
}

#undef LOCTEXT_NAMESPACE
