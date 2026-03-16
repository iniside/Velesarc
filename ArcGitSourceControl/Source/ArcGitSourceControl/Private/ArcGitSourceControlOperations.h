// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IArcGitSourceControlWorker.h"
#include "ArcGitSourceControlState.h"
#include "ArcGitSourceControlUtils.h"

/** Called when first activated on a project, and then at project load time.
 *  Look for the root directory of the git repository (where the ".git/" subdirectory is located). */
class FArcGitConnectWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitConnectWorker() {}
	// IArcGitSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FArcGitSourceControlState> States;

	/** LFS tracking patterns parsed from .gitattributes */
	TArray<FString> LfsPatterns;
};

/** Commit (check-in) a set of file to the local depot. */
class FArcGitCheckInWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitCheckInWorker() {}
	// IArcGitSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FArcGitSourceControlState> States;
};

/** Add an untraked file to source control (so only a subset of the git add command). */
class FArcGitMarkForAddWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitMarkForAddWorker() {}
	// IArcGitSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FArcGitSourceControlState> States;
};

/** Delete a file and remove it from source control. */
class FArcGitDeleteWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitDeleteWorker() {}
	// IArcGitSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Map of filenames to Git state */
	TArray<FArcGitSourceControlState> States;
};

/** Revert any change to a file to its state on the local depot. */
class FArcGitRevertWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitRevertWorker() {}
	// IArcGitSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Map of filenames to Git state */
	TArray<FArcGitSourceControlState> States;
};

/** Git pull --rebase to update branch from its configure remote */
class FArcGitSyncWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitSyncWorker() {}
	// IArcGitSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/// Map of filenames to Git state
	TArray<FArcGitSourceControlState> States;
};

/** Get source control status of files on local working copy. */
class FArcGitUpdateStatusWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitUpdateStatusWorker() {}
	// IArcGitSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FArcGitSourceControlState> States;

	/** Map of filenames to history */
	TMap<FString, TArcGitSourceControlHistory> Histories;

	/** LFS lock info keyed by full file path */
	TMap<FString, FArcGitLockInfo> Locks;
};

/** Copy or Move operation on a single file */
class FArcGitCopyWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitCopyWorker() {}
	// IArcGitSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FArcGitSourceControlState> OutStates;
};

/** git add to mark a conflict as resolved */
class FArcGitResolveWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitResolveWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

private:
	/** Temporary states for results */
	TArray<FArcGitSourceControlState> States;
};

/** Lock files via git lfs lock */
class FArcGitCheckOutWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitCheckOutWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
public:
	TArray<FArcGitSourceControlState> States;
};

/** Create a new changelist */
class FArcGitNewChangelistWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitNewChangelistWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

	int32 NewChangelistId = 0;
	FString Description;
	FDateTime Created;
};

/** Delete a changelist */
class FArcGitDeleteChangelistWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitDeleteChangelistWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

	int32 DeletedChangelistId = 0;
};

/** Edit changelist description */
class FArcGitEditChangelistWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitEditChangelistWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

	int32 EditedChangelistId = 0;
	FString NewDescription;
};

/** Move files between changelists */
class FArcGitMoveToChangelistWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitMoveToChangelistWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

	int32 TargetChangelistId = 0;
	TArray<FString> MovedFiles;
	TSet<int32> AffectedChangelistIds;
};

/** Update changelist status and reconcile */
class FArcGitUpdateChangelistsStatusWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitUpdateChangelistsStatusWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

	/** Reconciled changelist data: maps CL ID -> {description, files, created} */
	struct FChangelistData
	{
		FString Description;
		TArray<FString> Files;
		FDateTime Created;
	};
	TMap<int32, FChangelistData> ReconciledChangelists;
};

/** Shelve changelist to remote ref */
class FArcGitShelveWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitShelveWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

	int32 ShelvedChangelistId = 0;
	TArray<FString> ShelvedFiles;
};

/** Unshelve from remote ref */
class FArcGitUnshelveWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitUnshelveWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

	int32 TargetChangelistId = 0;
	TArray<FString> AppliedFiles;
	TArray<FArcGitSourceControlState> States;
};

/** Delete shelved changelist */
class FArcGitDeleteShelvedWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitDeleteShelvedWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

	int32 DeletedChangelistId = 0;
};

/** Get revision info */
class FArcGitGetRevisionInfoWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitGetRevisionInfoWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
};

/** Download file from specific revision */
class FArcGitDownloadFileWorker : public IArcGitSourceControlWorker
{
public:
	virtual ~FArcGitDownloadFileWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcGitSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
};
