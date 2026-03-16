// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcGitSourceControlOperations.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "ArcGitSourceControlModule.h"
#include "ArcGitSourceControlCommand.h"
#include "ArcGitSourceControlUtils.h"
#include "ArcGitSourceControlChangelist.h"
#include "ArcGitSourceControlChangelistState.h"

#define LOCTEXT_NAMESPACE "ArcGitSourceControl"

FName FArcGitConnectWorker::GetName() const
{
	return "Connect";
}

bool FArcGitConnectWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	// Check Git Availability
	if (0 < InCommand.PathToGitBinary.Len() && ArcGitSourceControlUtils::CheckGitAvailability(InCommand.PathToGitBinary))
	{
		// Now update the status of assets in Content/ directory and also Config files
		TArray<FString> ProjectDirs;
		ProjectDirs.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
		ProjectDirs.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir()));
		InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, ProjectDirs, InCommand.ErrorMessages, States);
		if(!InCommand.bCommandSuccessful || InCommand.ErrorMessages.Num() > 0)
		{
			StaticCastSharedRef<FConnect>(InCommand.Operation)->SetErrorText(LOCTEXT("NotAGitRepository", "Failed to enable Git revision control. You need to initialize the project as a Git repository first."));
			InCommand.bCommandSuccessful = false;
		}
		else
		{
			// Parse .gitattributes for LFS tracking patterns
			ArcGitSourceControlUtils::ParseGitAttributesForLfs(InCommand.PathToRepositoryRoot, LfsPatterns);
		}
	}
	else
	{
		StaticCastSharedRef<FConnect>(InCommand.Operation)->SetErrorText(LOCTEXT("GitNotFound", "Failed to enable Git revision control. You need to install Git and specify a valid path to git executable."));
		InCommand.bCommandSuccessful = false;
	}

	return InCommand.bCommandSuccessful;
}

bool FArcGitConnectWorker::UpdateStates() const
{
	bool bUpdated = ArcGitSourceControlUtils::UpdateCachedStates(States);

	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	Module.GetProvider().AccessLfsPatterns() = LfsPatterns;

	return bUpdated;
}

static FText ParseCommitResults(const TArray<FString>& InResults)
{
	if(InResults.Num() >= 1)
	{
		const FString& FirstLine = InResults[0];
		return FText::Format(LOCTEXT("CommitMessage", "Commited {0}."), FText::FromString(FirstLine));
	}
	return LOCTEXT("CommitMessageUnknown", "Submitted revision.");
}

FName FArcGitCheckInWorker::GetName() const
{
	return "CheckIn";
}

bool FArcGitCheckInWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	TSharedRef<FCheckIn, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCheckIn>(InCommand.Operation);

	// make a temp file to place our commit message in
	FArcGitScopedTempFile CommitMsgFile(Operation->GetDescription());
	if(CommitMsgFile.GetFilename().Len() > 0)
	{
		TArray<FString> Parameters;
		FString ParamCommitMsgFilename = TEXT("--file=\"");
		ParamCommitMsgFilename += FPaths::ConvertRelativePathToFull(CommitMsgFile.GetFilename());
		ParamCommitMsgFilename += TEXT("\"");
		Parameters.Add(ParamCommitMsgFilename);

		InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunCommit(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, Parameters, InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
		if(InCommand.bCommandSuccessful)
		{
			// Remove any deleted files from status cache
			FArcGitSourceControlModule& GitSourceControl = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
			FArcGitSourceControlProvider& Provider = GitSourceControl.GetProvider();

			TArray<TSharedRef<ISourceControlState, ESPMode::ThreadSafe>> LocalStates;
			Provider.GetState(InCommand.Files, LocalStates, EStateCacheUsage::Use);
			for (const auto& State : LocalStates)
			{
				if (State->IsDeleted())
				{
					Provider.RemoveFileFromCache(State->GetFilename());
				}
			}

			Operation->SetSuccessMessage(ParseCommitResults(InCommand.InfoMessages));
			const FString Message = (InCommand.InfoMessages.Num() > 0) ? InCommand.InfoMessages[0] : TEXT("");
			UE_LOG(LogSourceControl, Log, TEXT("commit successful: %s"), *Message);

			// Auto-push if configured
			if (GitSourceControl.AccessSettings().GetAutoPushOnCheckIn())
			{
				TArray<FString> PushResults;
				TArray<FString> PushErrors;
				ArcGitSourceControlUtils::RunCommand(TEXT("push"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, TArray<FString>(), TArray<FString>(), PushResults, PushErrors);
				if (PushErrors.Num() > 0)
				{
					InCommand.InfoMessages.Add(TEXT("Push failed - commits are ahead of remote"));
					InCommand.ErrorMessages.Append(PushErrors);
					// Don't fail the overall operation - commit succeeded
				}
			}

			// Unlock LFS files after commit
			for (const FString& File : InCommand.Files)
			{
				TArray<FString> UnlockResults;
				TArray<FString> UnlockErrors;
				TArray<FString> UnlockParams;
				UnlockParams.Add(TEXT("unlock"));
				UnlockParams.Add(File);
				ArcGitSourceControlUtils::RunCommand(TEXT("lfs"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, UnlockParams, TArray<FString>(), UnlockResults, UnlockErrors);
			}

			// Delete CL metadata if a specific CL was committed (not default)
			if (InCommand.Changelist.IsValid())
			{
				TSharedRef<FArcGitSourceControlChangelist> ArcCL = StaticCastSharedRef<FArcGitSourceControlChangelist>(InCommand.Changelist.ToSharedRef());
				if (!ArcCL->IsDefault())
				{
					ArcGitSourceControlUtils::DeleteChangelistFile(InCommand.PathToRepositoryRoot, ArcCL->Id);
				}
			}
		}
	}

	// now update the status of our files
	ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FArcGitCheckInWorker::UpdateStates() const
{
	return ArcGitSourceControlUtils::UpdateCachedStates(States);
}

FName FArcGitMarkForAddWorker::GetName() const
{
	return "MarkForAdd";
}

bool FArcGitMarkForAddWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunCommand(TEXT("add"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);

	// Assign files to changelist if specified
	if (InCommand.bCommandSuccessful && InCommand.Changelist.IsValid())
	{
		TSharedRef<FArcGitSourceControlChangelist> ArcCL = StaticCastSharedRef<FArcGitSourceControlChangelist>(InCommand.Changelist.ToSharedRef());
		if (!ArcCL->IsDefault())
		{
			FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
			FScopeLock ScopeLock(&Module.GetProvider().GetChangelistMutex());
			FString Desc;
			TArray<FString> CLFiles;
			FDateTime Created;
			if (ArcGitSourceControlUtils::LoadChangelist(InCommand.PathToRepositoryRoot, ArcCL->Id, Desc, CLFiles, Created))
			{
				for (const FString& File : InCommand.Files)
				{
					CLFiles.AddUnique(File);
				}
				ArcGitSourceControlUtils::SaveChangelist(InCommand.PathToRepositoryRoot, ArcCL->Id, Desc, CLFiles, Created);
			}
		}
	}

	// now update the status of our files
	ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FArcGitMarkForAddWorker::UpdateStates() const
{
	return ArcGitSourceControlUtils::UpdateCachedStates(States);
}

FName FArcGitDeleteWorker::GetName() const
{
	return "Delete";
}

bool FArcGitDeleteWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunCommand(TEXT("rm"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);

	// Assign files to changelist if specified
	if (InCommand.bCommandSuccessful && InCommand.Changelist.IsValid())
	{
		TSharedRef<FArcGitSourceControlChangelist> ArcCL = StaticCastSharedRef<FArcGitSourceControlChangelist>(InCommand.Changelist.ToSharedRef());
		if (!ArcCL->IsDefault())
		{
			FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
			FScopeLock ScopeLock(&Module.GetProvider().GetChangelistMutex());
			FString Desc;
			TArray<FString> CLFiles;
			FDateTime Created;
			if (ArcGitSourceControlUtils::LoadChangelist(InCommand.PathToRepositoryRoot, ArcCL->Id, Desc, CLFiles, Created))
			{
				for (const FString& File : InCommand.Files)
				{
					CLFiles.AddUnique(File);
				}
				ArcGitSourceControlUtils::SaveChangelist(InCommand.PathToRepositoryRoot, ArcCL->Id, Desc, CLFiles, Created);
			}
		}
	}

	// now update the status of our files
	ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FArcGitDeleteWorker::UpdateStates() const
{
	return ArcGitSourceControlUtils::UpdateCachedStates(States);
}


// Get lists of Missing files (ie "deleted"), Existing files, and "other than Added" Existing files
void GetMissingVsExistingFiles(const TArray<FString>& InFiles, TArray<FString>& OutMissingFiles, TArray<FString>& OutAllExistingFiles, TArray<FString>& OutOtherThanAddedExistingFiles)
{
	FArcGitSourceControlModule& GitSourceControl = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = GitSourceControl.GetProvider();

	TArray<TSharedRef<ISourceControlState, ESPMode::ThreadSafe>> LocalStates;
	Provider.GetState(InFiles, LocalStates, EStateCacheUsage::Use);
	for(const auto& State : LocalStates)
	{
		if(FPaths::FileExists(State->GetFilename()))
		{
			if(State->IsAdded())
			{
				OutAllExistingFiles.Add(State->GetFilename());
			}
			else
			{
				OutOtherThanAddedExistingFiles.Add(State->GetFilename());
				OutAllExistingFiles.Add(State->GetFilename());
			}
		}
		else
		{
			OutMissingFiles.Add(State->GetFilename());
		}
	}
}

FName FArcGitRevertWorker::GetName() const
{
	return "Revert";
}

bool FArcGitRevertWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	// Filter files by status to use the right "revert" commands on them
	TArray<FString> MissingFiles;
	TArray<FString> AllExistingFiles;
	TArray<FString> OtherThanAddedExistingFiles;
	GetMissingVsExistingFiles(InCommand.Files, MissingFiles, AllExistingFiles, OtherThanAddedExistingFiles);

	if(MissingFiles.Num() > 0)
	{
		// "Added" files that have been deleted needs to be removed from source control
		InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunCommand(TEXT("rm"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, TArray<FString>(), MissingFiles, InCommand.InfoMessages, InCommand.ErrorMessages);
	}
	if(AllExistingFiles.Num() > 0)
	{
		// reset any changes already added to the index
		InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunCommand(TEXT("reset"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, TArray<FString>(), AllExistingFiles, InCommand.InfoMessages, InCommand.ErrorMessages);
	}
	if(OtherThanAddedExistingFiles.Num() > 0)
	{
		// revert any changes in working copy (this would fails if the asset was in "Added" state, since after "reset" it is now "untracked")
		InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunCommand(TEXT("checkout"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, TArray<FString>(), OtherThanAddedExistingFiles, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	// now update the status of our files
	ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FArcGitRevertWorker::UpdateStates() const
{
	return ArcGitSourceControlUtils::UpdateCachedStates(States);
}

FName FArcGitSyncWorker::GetName() const
{
   return "Sync";
}

bool FArcGitSyncWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
   // pull the branch to get remote changes by rebasing any local commits (not merging them to avoid complex graphs)
   // (this cannot work if any local files are modified but not commited)
   {
      TArray<FString> Parameters;
      Parameters.Add(TEXT("--rebase origin HEAD"));
      InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunCommand(TEXT("pull"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, Parameters, TArray<FString>(), InCommand.InfoMessages, InCommand.ErrorMessages);
   }

   // now update the status of our files
   ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, InCommand.Files, InCommand.ErrorMessages, States);

   return InCommand.bCommandSuccessful;
}

bool FArcGitSyncWorker::UpdateStates() const
{
   return ArcGitSourceControlUtils::UpdateCachedStates(States);
}

FName FArcGitUpdateStatusWorker::GetName() const
{
	return "UpdateStatus";
}

bool FArcGitUpdateStatusWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FUpdateStatus>(InCommand.Operation);

	if(InCommand.Files.Num() > 0)
	{
		InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, InCommand.Files, InCommand.ErrorMessages, States);
		ArcGitSourceControlUtils::RemoveRedundantErrors(InCommand, TEXT("' is outside repository"));

		if(Operation->ShouldUpdateHistory())
		{
			for(int32 Index = 0; Index < States.Num(); Index++)
			{
				FString& File = InCommand.Files[Index];
				TArcGitSourceControlHistory History;

				if(States[Index].IsConflicted())
				{
					// In case of a merge conflict, we first need to get the tip of the "remote branch" (MERGE_HEAD)
					ArcGitSourceControlUtils::RunGetHistory(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, File, true, InCommand.ErrorMessages, History);
				}
				// Get the history of the file in the current branch
				InCommand.bCommandSuccessful &= ArcGitSourceControlUtils::RunGetHistory(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, File, false, InCommand.ErrorMessages, History);
				Histories.Add(*File, History);
			}
		}
	}
	else
	{
		// Perforce "opened files" are those that have been modified (or added/deleted): that is what we get with a simple Git status from the root
		if(Operation->ShouldGetOpenedOnly())
		{
			TArray<FString> Files;
			Files.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
			InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, Files, InCommand.ErrorMessages, States);
		}
	}

	// don't use the ShouldUpdateModifiedState() hint here as it is specific to Perforce: the above normal Git status has already told us this information (like Git and Mercurial)

	// Get LFS lock status
	ArcGitSourceControlUtils::RunGetLocks(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, Locks, InCommand.ErrorMessages);

	return InCommand.bCommandSuccessful;
}

bool FArcGitUpdateStatusWorker::UpdateStates() const
{
	bool bUpdated = ArcGitSourceControlUtils::UpdateCachedStates(States);

	FArcGitSourceControlModule& GitSourceControl = FModuleManager::LoadModuleChecked<FArcGitSourceControlModule>( "ArcGitSourceControl" );
	FArcGitSourceControlProvider& Provider = GitSourceControl.GetProvider();

	// add history, if any
	for(const auto& History : Histories)
	{
		TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe> State = Provider.GetStateInternal(History.Key);
		State->History = History.Value;
		State->TimeStamp = FDateTime::Now();
		bUpdated = true;
	}

	// Check if .gitattributes changed and refresh LFS patterns
	for (const auto& StateItem : States)
	{
		if (StateItem.LocalFilename.EndsWith(TEXT(".gitattributes")) && StateItem.WorkingCopyState != EArcGitState::Unchanged)
		{
			ArcGitSourceControlUtils::ParseGitAttributesForLfs(Provider.GetPathToRepositoryRoot(), Provider.AccessLfsPatterns());
			break;
		}
	}

	// Update LFS tracking info on file states
	const TArray<FString>& CurrentLfsPatterns = Provider.GetLfsPatterns();
	for (const auto& StateItem : States)
	{
		TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe> State = Provider.GetStateInternal(StateItem.LocalFilename);
		State->bIsLfsTracked = ArcGitSourceControlUtils::IsFileLfsTracked(StateItem.LocalFilename, CurrentLfsPatterns);
	}

	// Update lock states
	for (const auto& StateItem : States)
	{
		TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe> State = Provider.GetStateInternal(StateItem.LocalFilename);

		const FArcGitLockInfo* LockInfo = Locks.Find(StateItem.LocalFilename);
		if (LockInfo)
		{
			if (LockInfo->Owner == Provider.GetUserName())
			{
				State->LockState = EArcGitLockState::LockedByMe;
			}
			else
			{
				State->LockState = EArcGitLockState::LockedByOther;
				State->LockUser = LockInfo->Owner;
			}
		}
		else
		{
			State->LockState = EArcGitLockState::NotLocked;
			State->LockUser.Empty();
		}
		bUpdated = true;
	}

	// Update cached local changes count
	{
		int32 Count = 0;
		for (const auto& StateItem : States)
		{
			if (StateItem.WorkingCopyState != EArcGitState::Unknown
				&& StateItem.WorkingCopyState != EArcGitState::Unchanged
				&& StateItem.WorkingCopyState != EArcGitState::Ignored)
			{
				Count++;
			}
		}
		Provider.SetCachedNumLocalChanges(Count);
	}

	return bUpdated;
}

FName FArcGitCopyWorker::GetName() const
{
	return "Copy";
}

bool FArcGitCopyWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	// Copy or Move operation on a single file : Git does not need an explicit copy nor move,
	// but after a Move the Editor create a redirector file with the old asset name that points to the new asset.
	// The redirector needs to be commited with the new asset to perform a real rename.
	// => the following is to "MarkForAdd" the redirector, but it still need to be committed by selecting the whole directory and "check-in"
	InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunCommand(TEXT("add"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);

	return InCommand.bCommandSuccessful;
}

bool FArcGitCopyWorker::UpdateStates() const
{
	return ArcGitSourceControlUtils::UpdateCachedStates(OutStates);
}

FName FArcGitResolveWorker::GetName() const
{
	return "Resolve";
}

bool FArcGitResolveWorker::Execute( class FArcGitSourceControlCommand& InCommand )
{
	check(InCommand.Operation->GetName() == GetName());

	// mark the conflicting files as resolved:
	{
		TArray<FString> Results;
		InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunCommand(TEXT("add"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, TArray<FString>(), InCommand.Files, Results, InCommand.ErrorMessages);
	}

	// now update the status of our files
	ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FArcGitResolveWorker::UpdateStates() const
{
	return ArcGitSourceControlUtils::UpdateCachedStates(States);
}

// --- New worker stub implementations ---

FName FArcGitCheckOutWorker::GetName() const { return "CheckOut"; }

bool FArcGitCheckOutWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	InCommand.bCommandSuccessful = true;

	for (const FString& File : InCommand.Files)
	{
		TArray<FString> Results;
		TArray<FString> Errors;
		TArray<FString> Parameters;
		Parameters.Add(TEXT("lock"));
		Parameters.Add(File);

		const bool bResult = ArcGitSourceControlUtils::RunCommand(
			TEXT("lfs"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
			Parameters, TArray<FString>(), Results, Errors);

		if (!bResult)
		{
			InCommand.ErrorMessages.Append(Errors);
			InCommand.bCommandSuccessful = false;
		}
		else
		{
			// Make file writable on disk
			FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*File, false);
		}
	}

	// Update status after locking
	ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FArcGitCheckOutWorker::UpdateStates() const
{
	bool bUpdated = ArcGitSourceControlUtils::UpdateCachedStates(States);

	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	// Mark locked files as LockedByMe
	for (const auto& State : States)
	{
		TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(State.LocalFilename);
		CachedState->LockState = EArcGitLockState::LockedByMe;
		bUpdated = true;
	}

	return bUpdated;
}

FName FArcGitNewChangelistWorker::GetName() const { return "NewChangelist"; }

bool FArcGitNewChangelistWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	TSharedRef<FNewChangelist, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FNewChangelist>(InCommand.Operation);
	Description = Operation->GetDescription().ToString();
	Created = FDateTime::Now();

	FScopeLock ScopeLock(&Provider.GetChangelistMutex());
	NewChangelistId = ArcGitSourceControlUtils::AllocateNextChangelistId(InCommand.PathToRepositoryRoot);
	InCommand.bCommandSuccessful = ArcGitSourceControlUtils::SaveChangelist(InCommand.PathToRepositoryRoot, NewChangelistId, Description, TArray<FString>(), Created);

	return InCommand.bCommandSuccessful;
}

bool FArcGitNewChangelistWorker::UpdateStates() const
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	TSharedRef<FArcGitSourceControlChangelistState> NewState = MakeShared<FArcGitSourceControlChangelistState>(NewChangelistId);
	NewState->Description = Description;
	NewState->Timestamp = Created;
	Provider.GetChangelistStateCache().Add(NewChangelistId, NewState);

	return true;
}

FName FArcGitDeleteChangelistWorker::GetName() const { return "DeleteChangelist"; }

bool FArcGitDeleteChangelistWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	if (!InCommand.Changelist.IsValid())
	{
		InCommand.bCommandSuccessful = false;
		return false;
	}

	TSharedRef<FArcGitSourceControlChangelist> ArcCL = StaticCastSharedRef<FArcGitSourceControlChangelist>(InCommand.Changelist.ToSharedRef());
	DeletedChangelistId = ArcCL->Id;

	FScopeLock ScopeLock(&Provider.GetChangelistMutex());

	FString Desc;
	TArray<FString> CLFiles;
	FDateTime CreatedTime;
	if (ArcGitSourceControlUtils::LoadChangelist(InCommand.PathToRepositoryRoot, DeletedChangelistId, Desc, CLFiles, CreatedTime))
	{
		if (CLFiles.Num() > 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("Cannot delete changelist %d: it still contains %d file(s)"), DeletedChangelistId, CLFiles.Num()));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	InCommand.bCommandSuccessful = ArcGitSourceControlUtils::DeleteChangelistFile(InCommand.PathToRepositoryRoot, DeletedChangelistId);
	return InCommand.bCommandSuccessful;
}

bool FArcGitDeleteChangelistWorker::UpdateStates() const
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	Provider.GetChangelistStateCache().Remove(DeletedChangelistId);
	return true;
}

FName FArcGitEditChangelistWorker::GetName() const { return "EditChangelist"; }

bool FArcGitEditChangelistWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	if (!InCommand.Changelist.IsValid())
	{
		InCommand.bCommandSuccessful = false;
		return false;
	}

	TSharedRef<FArcGitSourceControlChangelist> ArcCL = StaticCastSharedRef<FArcGitSourceControlChangelist>(InCommand.Changelist.ToSharedRef());
	EditedChangelistId = ArcCL->Id;

	TSharedRef<FEditChangelist, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FEditChangelist>(InCommand.Operation);
	NewDescription = Operation->GetDescription().ToString();

	FScopeLock ScopeLock(&Provider.GetChangelistMutex());

	FString OldDesc;
	TArray<FString> CLFiles;
	FDateTime CreatedTime;
	if (!ArcGitSourceControlUtils::LoadChangelist(InCommand.PathToRepositoryRoot, EditedChangelistId, OldDesc, CLFiles, CreatedTime))
	{
		InCommand.bCommandSuccessful = false;
		return false;
	}

	InCommand.bCommandSuccessful = ArcGitSourceControlUtils::SaveChangelist(InCommand.PathToRepositoryRoot, EditedChangelistId, NewDescription, CLFiles, CreatedTime);
	return InCommand.bCommandSuccessful;
}

bool FArcGitEditChangelistWorker::UpdateStates() const
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	TSharedRef<FArcGitSourceControlChangelistState>* Found = Provider.GetChangelistStateCache().Find(EditedChangelistId);
	if (Found)
	{
		(*Found)->Description = NewDescription;
	}
	return true;
}

FName FArcGitMoveToChangelistWorker::GetName() const { return "MoveToChangelist"; }

bool FArcGitMoveToChangelistWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	if (!InCommand.Changelist.IsValid())
	{
		InCommand.bCommandSuccessful = false;
		return false;
	}

	TSharedRef<FArcGitSourceControlChangelist> TargetCL = StaticCastSharedRef<FArcGitSourceControlChangelist>(InCommand.Changelist.ToSharedRef());
	TargetChangelistId = TargetCL->Id;
	MovedFiles = InCommand.Files;

	FScopeLock ScopeLock(&Provider.GetChangelistMutex());

	// Load all changelists to find which CL each file belongs to
	TArray<int32> AllIds = ArcGitSourceControlUtils::GetAllChangelistIds(InCommand.PathToRepositoryRoot);

	struct FLoadedCL
	{
		FString Description;
		TArray<FString> Files;
		FDateTime Created;
		bool bDirty = false;
	};
	TMap<int32, FLoadedCL> LoadedCLs;

	for (int32 Id : AllIds)
	{
		FLoadedCL& CL = LoadedCLs.FindOrAdd(Id);
		ArcGitSourceControlUtils::LoadChangelist(InCommand.PathToRepositoryRoot, Id, CL.Description, CL.Files, CL.Created);
	}

	// Ensure target CL is loaded (might be default CL 0 which has no file on disk yet)
	if (!LoadedCLs.Contains(TargetChangelistId))
	{
		FLoadedCL& TargetData = LoadedCLs.FindOrAdd(TargetChangelistId);
		TargetData.Created = FDateTime::Now();
	}

	for (const FString& File : InCommand.Files)
	{
		// Remove from any old CL
		for (auto& Pair : LoadedCLs)
		{
			if (Pair.Key == TargetChangelistId)
			{
				continue;
			}
			int32 Removed = Pair.Value.Files.Remove(File);
			if (Removed > 0)
			{
				Pair.Value.bDirty = true;
				AffectedChangelistIds.Add(Pair.Key);
			}
		}

		// Add to target CL
		FLoadedCL& TargetData = LoadedCLs.FindOrAdd(TargetChangelistId);
		TargetData.Files.AddUnique(File);
		TargetData.bDirty = true;
	}

	AffectedChangelistIds.Add(TargetChangelistId);

	// Save dirty CLs
	InCommand.bCommandSuccessful = true;
	for (auto& Pair : LoadedCLs)
	{
		if (Pair.Value.bDirty && Pair.Key != 0) // Don't persist default CL to disk
		{
			if (!ArcGitSourceControlUtils::SaveChangelist(InCommand.PathToRepositoryRoot, Pair.Key, Pair.Value.Description, Pair.Value.Files, Pair.Value.Created))
			{
				InCommand.bCommandSuccessful = false;
			}
		}
	}

	return InCommand.bCommandSuccessful;
}

bool FArcGitMoveToChangelistWorker::UpdateStates() const
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	// Rebuild file lists in affected CL states
	for (int32 CLId : AffectedChangelistIds)
	{
		TSharedRef<FArcGitSourceControlChangelistState>* Found = Provider.GetChangelistStateCache().Find(CLId);
		if (Found)
		{
			// Remove moved files from this CL's file state list
			if (CLId != TargetChangelistId)
			{
				(*Found)->Files.RemoveAll([this](const FSourceControlStateRef& State)
				{
					return MovedFiles.Contains(State->GetFilename());
				});
			}
		}
	}

	// Add files to the target CL state
	TSharedRef<FArcGitSourceControlChangelistState>* TargetFound = Provider.GetChangelistStateCache().Find(TargetChangelistId);
	if (TargetFound)
	{
		for (const FString& File : MovedFiles)
		{
			TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe> FileState = Provider.GetStateInternal(File);
			(*TargetFound)->Files.AddUnique(FileState);
		}
	}

	return true;
}

FName FArcGitUpdateChangelistsStatusWorker::GetName() const { return "UpdateChangelistsStatus"; }

bool FArcGitUpdateChangelistsStatusWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	// Run git status to get all modified files
	TArray<FArcGitSourceControlState> GitStates;
	TArray<FString> StatusFiles;
	StatusFiles.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, StatusFiles, InCommand.ErrorMessages, GitStates);

	// Collect all currently modified files
	TSet<FString> ModifiedFiles;
	for (const FArcGitSourceControlState& State : GitStates)
	{
		if (State.WorkingCopyState != EArcGitState::Unchanged && State.WorkingCopyState != EArcGitState::NotControlled)
		{
			ModifiedFiles.Add(State.LocalFilename);
		}
	}

	FScopeLock ScopeLock(&Provider.GetChangelistMutex());

	// Load all CLs from disk
	TArray<int32> AllIds = ArcGitSourceControlUtils::GetAllChangelistIds(InCommand.PathToRepositoryRoot);
	TSet<FString> AssignedFiles;

	for (int32 Id : AllIds)
	{
		FChangelistData& CLData = ReconciledChangelists.FindOrAdd(Id);
		ArcGitSourceControlUtils::LoadChangelist(InCommand.PathToRepositoryRoot, Id, CLData.Description, CLData.Files, CLData.Created);

		// Remove files that are no longer modified
		CLData.Files.RemoveAll([&ModifiedFiles](const FString& File)
		{
			return !ModifiedFiles.Contains(File);
		});

		for (const FString& File : CLData.Files)
		{
			AssignedFiles.Add(File);
		}

		// Save updated CL back to disk
		if (Id != 0)
		{
			ArcGitSourceControlUtils::SaveChangelist(InCommand.PathToRepositoryRoot, Id, CLData.Description, CLData.Files, CLData.Created);
		}
	}

	// Add newly modified unassigned files to default CL (id=0)
	FChangelistData& DefaultCL = ReconciledChangelists.FindOrAdd(0);
	if (DefaultCL.Description.IsEmpty())
	{
		DefaultCL.Description = TEXT("Default");
		DefaultCL.Created = FDateTime::Now();
	}

	for (const FString& File : ModifiedFiles)
	{
		if (!AssignedFiles.Contains(File))
		{
			DefaultCL.Files.AddUnique(File);
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcGitUpdateChangelistsStatusWorker::UpdateStates() const
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	// Rebuild ChangelistStateCache from reconciled data
	Provider.GetChangelistStateCache().Empty();

	for (const auto& Pair : ReconciledChangelists)
	{
		TSharedRef<FArcGitSourceControlChangelistState> CLState = MakeShared<FArcGitSourceControlChangelistState>(Pair.Key);
		CLState->Description = Pair.Value.Description;
		CLState->Timestamp = Pair.Value.Created;

		for (const FString& File : Pair.Value.Files)
		{
			TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe> FileState = Provider.GetStateInternal(File);
			CLState->Files.Add(FileState);
		}

		Provider.GetChangelistStateCache().Add(Pair.Key, CLState);
	}

	// Ensure default CL always exists
	if (!Provider.GetChangelistStateCache().Contains(0))
	{
		TSharedRef<FArcGitSourceControlChangelistState> DefaultState = MakeShared<FArcGitSourceControlChangelistState>(0);
		DefaultState->Description = TEXT("Default");
		DefaultState->Timestamp = FDateTime::Now();
		Provider.GetChangelistStateCache().Add(0, DefaultState);
	}

	return true;
}

// --- Shelve worker ---

FName FArcGitShelveWorker::GetName() const { return "Shelve"; }

bool FArcGitShelveWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	if (!InCommand.Changelist.IsValid())
	{
		InCommand.ErrorMessages.Add(TEXT("Shelve requires a valid changelist"));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	TSharedRef<FArcGitSourceControlChangelist> ArcCL = StaticCastSharedRef<FArcGitSourceControlChangelist>(InCommand.Changelist.ToSharedRef());
	ShelvedChangelistId = ArcCL->Id;

	const FString UserName = Provider.GetUserName();
	const FString ShelvePrefix = Module.AccessSettings().GetShelveRefPrefix();
	const FString RefPath = FString::Printf(TEXT("%s/%s/%d"), *ShelvePrefix, *UserName, ShelvedChangelistId);

	// Load CL files from metadata
	FString Description;
	FDateTime Created;
	{
		FScopeLock ScopeLock(&Provider.GetChangelistMutex());
		if (!ArcGitSourceControlUtils::LoadChangelist(InCommand.PathToRepositoryRoot, ShelvedChangelistId, Description, ShelvedFiles, Created))
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("Failed to load changelist %d"), ShelvedChangelistId));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	if (ShelvedFiles.Num() == 0)
	{
		InCommand.ErrorMessages.Add(TEXT("Changelist has no files to shelve"));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Create the shelve commit
	FString CommitSha;
	if (!ArcGitSourceControlUtils::CreateShelveCommit(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
		ShelvedFiles, Description, CommitSha, InCommand.ErrorMessages))
	{
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Update local ref to point at the shelve commit
	{
		TArray<FString> Results;
		if (!ArcGitSourceControlUtils::RunCommand(TEXT("update-ref"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
			{ RefPath, CommitSha }, TArray<FString>(), Results, InCommand.ErrorMessages))
		{
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// Force-push the ref to origin
	{
		TArray<FString> Results;
		if (!ArcGitSourceControlUtils::RunCommand(TEXT("push"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
			{ TEXT("origin"), TEXT("+") + RefPath }, TArray<FString>(), Results, InCommand.ErrorMessages))
		{
			// Push failure is non-fatal — local shelve still succeeded
			InCommand.InfoMessages.Add(TEXT("Shelve created locally but failed to push to remote"));
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcGitShelveWorker::UpdateStates() const
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	TSharedRef<FArcGitSourceControlChangelistState>* Found = Provider.GetChangelistStateCache().Find(ShelvedChangelistId);
	if (Found)
	{
		// Populate the ShelvedFiles map on the CL state
		(*Found)->ShelvedFiles.Empty();
		for (const FString& File : ShelvedFiles)
		{
			TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe> FileState = Provider.GetStateInternal(File);
			(*Found)->ShelvedFiles.Add(File, FileState);
		}
	}

	return true;
}

// --- Unshelve worker ---

FName FArcGitUnshelveWorker::GetName() const { return "Unshelve"; }

bool FArcGitUnshelveWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	if (!InCommand.Changelist.IsValid())
	{
		InCommand.ErrorMessages.Add(TEXT("Unshelve requires a valid changelist"));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	TSharedRef<FArcGitSourceControlChangelist> ArcCL = StaticCastSharedRef<FArcGitSourceControlChangelist>(InCommand.Changelist.ToSharedRef());
	TargetChangelistId = ArcCL->Id;

	const FString UserName = Provider.GetUserName();
	const FString ShelvePrefix = Module.AccessSettings().GetShelveRefPrefix();
	const FString RefPath = FString::Printf(TEXT("%s/%s/%d"), *ShelvePrefix, *UserName, TargetChangelistId);

	// Fetch the ref from origin
	{
		TArray<FString> Results;
		ArcGitSourceControlUtils::RunCommand(TEXT("fetch"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
			{ TEXT("origin"), RefPath + TEXT(":") + RefPath }, TArray<FString>(), Results, InCommand.ErrorMessages);
		// Fetch may fail if ref is already local — continue anyway
	}

	// Resolve the shelf commit SHA
	FString CommitSha;
	{
		TArray<FString> Results;
		if (!ArcGitSourceControlUtils::RunCommand(TEXT("rev-parse"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
			{ RefPath }, TArray<FString>(), Results, InCommand.ErrorMessages) || Results.Num() == 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("Failed to resolve shelve ref: %s"), *RefPath));
			InCommand.bCommandSuccessful = false;
			return false;
		}
		CommitSha = Results[0].TrimStartAndEnd();
	}

	// Resolve the parent commit SHA
	FString ParentSha;
	{
		TArray<FString> Results;
		if (!ArcGitSourceControlUtils::RunCommand(TEXT("rev-parse"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
			{ RefPath + TEXT("^") }, TArray<FString>(), Results, InCommand.ErrorMessages) || Results.Num() == 0)
		{
			InCommand.ErrorMessages.Add(TEXT("Failed to resolve shelve parent commit"));
			InCommand.bCommandSuccessful = false;
			return false;
		}
		ParentSha = Results[0].TrimStartAndEnd();
	}

	// Generate a binary diff patch between parent and shelve commit
	TArray<FString> PatchLines;
	{
		if (!ArcGitSourceControlUtils::RunCommand(TEXT("diff"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
			{ TEXT("--binary"), ParentSha + TEXT("..") + CommitSha }, TArray<FString>(), PatchLines, InCommand.ErrorMessages))
		{
			InCommand.ErrorMessages.Add(TEXT("Failed to generate shelve diff"));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// Write the patch to a temp file
	const FString TempPatchFile = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("ArcGitUnshelve"), TEXT(".patch"));
	{
		FString PatchContent;
		for (const FString& Line : PatchLines)
		{
			PatchContent += Line + TEXT("\n");
		}
		if (!FFileHelper::SaveStringToFile(PatchContent, *TempPatchFile))
		{
			InCommand.ErrorMessages.Add(TEXT("Failed to write temporary patch file"));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// Apply the patch with 3-way merge
	{
		TArray<FString> Results;
		InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunCommand(TEXT("apply"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
			{ TEXT("--3way"), TempPatchFile }, TArray<FString>(), Results, InCommand.ErrorMessages);
	}

	// Clean up temp file
	IFileManager::Get().Delete(*TempPatchFile);

	if (!InCommand.bCommandSuccessful)
	{
		return false;
	}

	// Load CL files so we know what was applied
	{
		FString Description;
		FDateTime Created;
		FScopeLock ScopeLock(&Provider.GetChangelistMutex());
		ArcGitSourceControlUtils::LoadChangelist(InCommand.PathToRepositoryRoot, TargetChangelistId, Description, AppliedFiles, Created);
	}

	// Update status on applied files
	ArcGitSourceControlUtils::RunUpdateStatus(InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, AppliedFiles, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FArcGitUnshelveWorker::UpdateStates() const
{
	bool bUpdated = ArcGitSourceControlUtils::UpdateCachedStates(States);

	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	// Assign applied files to target CL state
	TSharedRef<FArcGitSourceControlChangelistState>* Found = Provider.GetChangelistStateCache().Find(TargetChangelistId);
	if (Found)
	{
		for (const FString& File : AppliedFiles)
		{
			TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe> FileState = Provider.GetStateInternal(File);
			(*Found)->Files.AddUnique(FileState);
		}
		bUpdated = true;
	}

	return bUpdated;
}

// --- DeleteShelved worker ---

FName FArcGitDeleteShelvedWorker::GetName() const { return "DeleteShelved"; }

bool FArcGitDeleteShelvedWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	if (!InCommand.Changelist.IsValid())
	{
		InCommand.ErrorMessages.Add(TEXT("DeleteShelved requires a valid changelist"));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	TSharedRef<FArcGitSourceControlChangelist> ArcCL = StaticCastSharedRef<FArcGitSourceControlChangelist>(InCommand.Changelist.ToSharedRef());
	DeletedChangelistId = ArcCL->Id;

	const FString UserName = Provider.GetUserName();
	const FString ShelvePrefix = Module.AccessSettings().GetShelveRefPrefix();
	const FString RefPath = FString::Printf(TEXT("%s/%s/%d"), *ShelvePrefix, *UserName, DeletedChangelistId);

	// Delete the local ref
	{
		TArray<FString> Results;
		ArcGitSourceControlUtils::RunCommand(TEXT("update-ref"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
			{ TEXT("-d"), RefPath }, TArray<FString>(), Results, InCommand.ErrorMessages);
		// Local ref may not exist — continue anyway
	}

	// Delete the remote ref
	{
		TArray<FString> Results;
		ArcGitSourceControlUtils::RunCommand(TEXT("push"), InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot,
			{ TEXT("origin"), TEXT("--delete"), RefPath }, TArray<FString>(), Results, InCommand.ErrorMessages);
		// Remote ref may not exist — continue anyway
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcGitDeleteShelvedWorker::UpdateStates() const
{
	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FArcGitSourceControlProvider& Provider = Module.GetProvider();

	TSharedRef<FArcGitSourceControlChangelistState>* Found = Provider.GetChangelistStateCache().Find(DeletedChangelistId);
	if (Found)
	{
		(*Found)->ShelvedFiles.Empty();
	}

	return true;
}

FName FArcGitGetRevisionInfoWorker::GetName() const { return "GetSourceControlRevisionInfo"; }
bool FArcGitGetRevisionInfoWorker::Execute(FArcGitSourceControlCommand& InCommand) { InCommand.bCommandSuccessful = true; return true; }
bool FArcGitGetRevisionInfoWorker::UpdateStates() const { return false; }

FName FArcGitDownloadFileWorker::GetName() const { return "DownloadFile"; }

bool FArcGitDownloadFileWorker::Execute(FArcGitSourceControlCommand& InCommand)
{
	// Use git show to download file at specific revision
	TSharedRef<FDownloadFile, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FDownloadFile>(InCommand.Operation);

	for (const FString& File : InCommand.Files)
	{
		// git show HEAD:<file> > target
		const FString Parameter = FString::Printf(TEXT("HEAD:%s"), *File);
		const FString TargetFile = FPaths::Combine(Operation->GetTargetDirectory(), FPaths::GetCleanFilename(File));

		InCommand.bCommandSuccessful = ArcGitSourceControlUtils::RunDumpToFile(
			InCommand.PathToGitBinary, InCommand.PathToRepositoryRoot, Parameter, TargetFile);

		if (!InCommand.bCommandSuccessful)
		{
			break;
		}
	}

	return InCommand.bCommandSuccessful;
}

bool FArcGitDownloadFileWorker::UpdateStates() const { return false; }

#undef LOCTEXT_NAMESPACE
