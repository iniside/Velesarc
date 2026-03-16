// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcGitSourceControlProvider.h"
#include "ArcGitSourceControlState.h"
#include "ArcGitSourceControlChangelist.h"
#include "Misc/Paths.h"
#include "Misc/QueuedThreadPool.h"
#include "ArcGitSourceControlCommand.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "ArcGitSourceControlModule.h"
#include "ArcGitSourceControlUtils.h"
#include "ArcGitSourceControlLabel.h"
#include "SArcGitSourceControlSettings.h"
#include "SourceControlOperations.h"
#include "Logging/MessageLog.h"
#include "ScopedSourceControlProgress.h"

#define LOCTEXT_NAMESPACE "ArcGitSourceControl"

static FName ProviderName("Git (Arc)");

void FArcGitSourceControlProvider::Init(bool bForceConnection)
{
	// Init() is called multiple times at startup: do not check git each time
	if(!bGitAvailable)
	{
		CheckGitAvailability();
	}

	// bForceConnection: not used anymore
}

void FArcGitSourceControlProvider::CheckGitAvailability()
{
	FArcGitSourceControlModule& GitSourceControl = FModuleManager::LoadModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	FString PathToGitBinary = GitSourceControl.AccessSettings().GetBinaryPath();
	if(PathToGitBinary.IsEmpty())
	{
		// Try to find Git binary, and update settings accordingly
		PathToGitBinary = ArcGitSourceControlUtils::FindGitBinaryPath();
		if(!PathToGitBinary.IsEmpty())
		{
			GitSourceControl.AccessSettings().SetBinaryPath(PathToGitBinary);
		}
	}

	if(!PathToGitBinary.IsEmpty())
	{
		bGitAvailable = ArcGitSourceControlUtils::CheckGitAvailability(PathToGitBinary, &GitVersion);
		if(bGitAvailable)
		{
			CheckRepositoryStatus(PathToGitBinary);
		}
	}
	else
	{
		bGitAvailable = false;
	}
}

void FArcGitSourceControlProvider::CheckRepositoryStatus(const FString& InPathToGitBinary)
{
	// Find the path to the root Git directory (if any)
	const FString PathToProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	bGitRepositoryFound = ArcGitSourceControlUtils::FindRootDirectory(PathToProjectDir, PathToRepositoryRoot);
	if(bGitRepositoryFound)
	{
		// Get branch name
		bGitRepositoryFound = ArcGitSourceControlUtils::GetBranchName(InPathToGitBinary, PathToRepositoryRoot, BranchName);
		if(bGitRepositoryFound)
		{
			ArcGitSourceControlUtils::GetRemoteUrl(InPathToGitBinary, PathToRepositoryRoot, RemoteUrl);
		}
		else
		{
			UE_LOG(LogSourceControl, Error, TEXT("'%s' is not a valid Git repository"), *PathToRepositoryRoot);
		}
	}
	else
	{
		UE_LOG(LogSourceControl, Warning, TEXT("'%s' is not part of a Git repository"), *FPaths::ProjectDir());
	}

	// Get user name & email (of the repository, else from the global Git config)
	ArcGitSourceControlUtils::GetUserConfig(InPathToGitBinary, PathToRepositoryRoot, UserName, UserEmail);
}

void FArcGitSourceControlProvider::Close()
{
	// clear the cache
	StateCache.Empty();

	bGitAvailable = false;
	bGitRepositoryFound = false;
	UserName.Empty();
	UserEmail.Empty();
}

TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe> FArcGitSourceControlProvider::GetStateInternal(const FString& Filename)
{
	TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(Filename);
	if(State != NULL)
	{
		// found cached item
		return (*State);
	}
	else
	{
		// cache an unknown state for this item
		TSharedRef<FArcGitSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable( new FArcGitSourceControlState(Filename) );
		StateCache.Add(Filename, NewState);
		return NewState;
	}
}

FText FArcGitSourceControlProvider::GetStatusText() const
{
	FString LfsLockingStatus = GitVersion.bHasGitLfsLocking ? TEXT("On") : TEXT("Off");

	FString StatusString = FString::Printf(
		TEXT("Git (Arc) | branch: %s | remote: %s | LFS Locking: %s"),
		*BranchName,
		RemoteUrl.IsEmpty() ? TEXT("none") : *RemoteUrl,
		*LfsLockingStatus
	);

	if (CachedNumLocalChanges.IsSet())
	{
		StatusString += FString::Printf(TEXT(" | %d local changes"), CachedNumLocalChanges.GetValue());
	}

	return FText::FromString(StatusString);
}

TMap<ISourceControlProvider::EStatus, FString> FArcGitSourceControlProvider::GetStatus() const
{
	TMap<EStatus, FString> Result;
	Result.Add(EStatus::Enabled, IsEnabled() ? TEXT("Yes") : TEXT("No") );
	Result.Add(EStatus::Connected, (IsEnabled() && IsAvailable()) ? TEXT("Yes") : TEXT("No") );
	Result.Add(EStatus::User, UserName);
	Result.Add(EStatus::Repository, PathToRepositoryRoot);
	Result.Add(EStatus::Remote, RemoteUrl);
	Result.Add(EStatus::Branch, BranchName);
	Result.Add(EStatus::Email, UserEmail);
	return Result;
}

/** Quick check if source control is enabled */
bool FArcGitSourceControlProvider::IsEnabled() const
{
	return bGitRepositoryFound;
}

/** Quick check if source control is available for use (useful for server-based providers) */
bool FArcGitSourceControlProvider::IsAvailable() const
{
	return bGitRepositoryFound;
}

const FName& FArcGitSourceControlProvider::GetName(void) const
{
	return ProviderName;
}

ECommandResult::Type FArcGitSourceControlProvider::GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage )
{
	if(!IsEnabled())
	{
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles = SourceControlHelpers::AbsoluteFilenames(InFiles);

	if(InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		Execute(ISourceControlOperation::Create<FUpdateStatus>(), AbsoluteFiles);
	}

	for(const auto& AbsoluteFile : AbsoluteFiles)
	{
		OutState.Add(GetStateInternal(*AbsoluteFile));
	}

	return ECommandResult::Succeeded;
}

ECommandResult::Type FArcGitSourceControlProvider::GetState(const TArray<FSourceControlChangelistRef>& InChangelists, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	for (const auto& CLRef : InChangelists)
	{
		TSharedRef<FArcGitSourceControlChangelist> ArcCL = StaticCastSharedRef<FArcGitSourceControlChangelist>(CLRef);
		TSharedRef<FArcGitSourceControlChangelistState>* Found = ChangelistStateCache.Find(ArcCL->Id);
		if (Found)
		{
			OutState.Add(*Found);
		}
	}
	return ECommandResult::Succeeded;
}

TArray<FSourceControlStateRef> FArcGitSourceControlProvider::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const
{
	TArray<FSourceControlStateRef> Result;
	for(const auto& CacheItem : StateCache)
	{
		FSourceControlStateRef State = CacheItem.Value;
		if(Predicate(State))
		{
			Result.Add(State);
		}
	}
	return Result;
}

bool FArcGitSourceControlProvider::RemoveFileFromCache(const FString& Filename)
{
	return StateCache.Remove(Filename) > 0;
}

FDelegateHandle FArcGitSourceControlProvider::RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged )
{
	return OnSourceControlStateChanged.Add( SourceControlStateChanged );
}

void FArcGitSourceControlProvider::UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle )
{
	OnSourceControlStateChanged.Remove( Handle );
}

ECommandResult::Type FArcGitSourceControlProvider::Execute( const FSourceControlOperationRef& InOperation, FSourceControlChangelistPtr InChangelist, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate )
{
	if(!IsEnabled() && !(InOperation->GetName() == "Connect")) // Only Connect operation allowed while not Enabled (Connected)
	{
		// Note that IsEnabled() always returns true so unless it is changed, this code will never be executed
		InOperationCompleteDelegate.ExecuteIfBound(InOperation, ECommandResult::Failed);
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles = SourceControlHelpers::AbsoluteFilenames(InFiles);

	// Query to see if we allow this operation
	TSharedPtr<IArcGitSourceControlWorker, ESPMode::ThreadSafe> Worker = CreateWorker(InOperation->GetName());
	if(!Worker.IsValid())
	{
		// this operation is unsupported by this source control provider
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("OperationName"), FText::FromName(InOperation->GetName()) );
		Arguments.Add( TEXT("ProviderName"), FText::FromName(GetName()) );
		FText Message(FText::Format(LOCTEXT("UnsupportedOperation", "Operation '{OperationName}' not supported by revision control provider '{ProviderName}'"), Arguments));
		FMessageLog("SourceControl").Error(Message);
		InOperation->AddErrorMessge(Message);

		InOperationCompleteDelegate.ExecuteIfBound(InOperation, ECommandResult::Failed);
		return ECommandResult::Failed;
	}

	FArcGitSourceControlCommand* Command = new FArcGitSourceControlCommand(InOperation, Worker.ToSharedRef());
	Command->Files = AbsoluteFiles;
	Command->Changelist = InChangelist;
	Command->OperationCompleteDelegate = InOperationCompleteDelegate;

	// fire off operation
	if(InConcurrency == EConcurrency::Synchronous)
	{
		Command->bAutoDelete = false;
		return ExecuteSynchronousCommand(*Command, InOperation->GetInProgressString());
	}
	else
	{
		Command->bAutoDelete = true;
		return IssueCommand(*Command);
	}
}

bool FArcGitSourceControlProvider::CanExecuteOperation( const FSourceControlOperationRef& InOperation ) const
{
	return WorkersMap.Find(InOperation->GetName()) != nullptr;
}

bool FArcGitSourceControlProvider::CanCancelOperation( const FSourceControlOperationRef& InOperation ) const
{
	return true;
}

void FArcGitSourceControlProvider::CancelOperation( const FSourceControlOperationRef& InOperation )
{
	for (int32 i = 0; i < CommandQueue.Num(); ++i)
	{
		FArcGitSourceControlCommand& Command = *CommandQueue[i];
		if (Command.Operation == InOperation)
		{
			Command.bCancelRequested.Store(true);
			if (Command.ProcessHandle.IsValid())
			{
				FPlatformProcess::TerminateProc(Command.ProcessHandle);
			}
		}
	}
}

bool FArcGitSourceControlProvider::UsesLocalReadOnlyState() const
{
	return GitVersion.bHasGitLfsLocking;
}

bool FArcGitSourceControlProvider::UsesChangelists() const
{
	return true;
}

bool FArcGitSourceControlProvider::UsesUncontrolledChangelists() const
{
	return true;
}

bool FArcGitSourceControlProvider::UsesCheckout() const
{
	return GitVersion.bHasGitLfsLocking;
}

bool FArcGitSourceControlProvider::UsesFileRevisions() const
{
	return true;
}

bool FArcGitSourceControlProvider::UsesSnapshots() const
{
	return false;
}

bool FArcGitSourceControlProvider::AllowsDiffAgainstDepot() const
{
	return true;
}

TOptional<bool> FArcGitSourceControlProvider::IsAtLatestRevision() const
{
	return bCachedIsAtLatest;
}

TOptional<int> FArcGitSourceControlProvider::GetNumLocalChanges() const
{
	return CachedNumLocalChanges;
}

TSharedPtr<IArcGitSourceControlWorker, ESPMode::ThreadSafe> FArcGitSourceControlProvider::CreateWorker(const FName& InOperationName) const
{
	const FGetArcGitSourceControlWorker* Operation = WorkersMap.Find(InOperationName);
	if(Operation != nullptr)
	{
		return Operation->Execute();
	}

	return nullptr;
}

void FArcGitSourceControlProvider::RegisterWorker( const FName& InName, const FGetArcGitSourceControlWorker& InDelegate )
{
	WorkersMap.Add( InName, InDelegate );
}

void FArcGitSourceControlProvider::OutputCommandMessages(const FArcGitSourceControlCommand& InCommand) const
{
	FMessageLog SourceControlLog("SourceControl");

	for(int32 ErrorIndex = 0; ErrorIndex < InCommand.ErrorMessages.Num(); ++ErrorIndex)
	{
		SourceControlLog.Error(FText::FromString(InCommand.ErrorMessages[ErrorIndex]));
	}

	for(int32 InfoIndex = 0; InfoIndex < InCommand.InfoMessages.Num(); ++InfoIndex)
	{
		SourceControlLog.Info(FText::FromString(InCommand.InfoMessages[InfoIndex]));
	}
}

void FArcGitSourceControlProvider::Tick()
{
	bool bStatesUpdated = false;
	for(int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FArcGitSourceControlCommand& Command = *CommandQueue[CommandIndex];
		if(Command.bExecuteProcessed)
		{
			// Remove command from the queue
			CommandQueue.RemoveAt(CommandIndex);

			// let command update the states of any files
			bStatesUpdated |= Command.Worker->UpdateStates();

			// dump any messages to output log
			OutputCommandMessages(Command);

			Command.ReturnResults();

			// commands that are left in the array during a tick need to be deleted
			if(Command.bAutoDelete)
			{
				// Only delete commands that are not running 'synchronously'
				delete &Command;
			}

			// only do one command per tick loop, as we dont want concurrent modification
			// of the command queue (which can happen in the completion delegate)
			break;
		}
	}

	if(bStatesUpdated)
	{
		OnSourceControlStateChanged.Broadcast();
	}

	// Throttled remote status check
	if (bGitRepositoryFound)
	{
		FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
		const float CheckInterval = Module.AccessSettings().GetRemoteStatusIntervalSeconds();
		const double CurrentTime = FPlatformTime::Seconds();
		if ((CurrentTime - LastRemoteCheckTime) > CheckInterval)
		{
			LastRemoteCheckTime = CurrentTime;

			const FString PathToGitBinary = Module.AccessSettings().GetBinaryPath();
			TArray<FString> Results;
			TArray<FString> Errors;
			TArray<FString> Parameters;
			Parameters.Add(TEXT("HEAD..@{upstream}"));
			Parameters.Add(TEXT("--count"));

			if (ArcGitSourceControlUtils::RunCommand(TEXT("rev-list"), PathToGitBinary, PathToRepositoryRoot, Parameters, TArray<FString>(), Results, Errors))
			{
				if (Results.Num() > 0)
				{
					int32 BehindCount = FCString::Atoi(*Results[0]);
					bCachedIsAtLatest = (BehindCount == 0);
				}
			}
		}
	}
}

TArray< TSharedRef<ISourceControlLabel> > FArcGitSourceControlProvider::GetLabels( const FString& InMatchingSpec ) const
{
	TArray< TSharedRef<ISourceControlLabel> > Labels;

	FArcGitSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	const FString PathToGitBinary = Module.AccessSettings().GetBinaryPath();

	TArray<FString> Results;
	TArray<FString> Errors;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("-l"));
	if (!InMatchingSpec.IsEmpty())
	{
		Parameters.Add(InMatchingSpec);
	}

	ArcGitSourceControlUtils::RunCommand(TEXT("tag"), PathToGitBinary, PathToRepositoryRoot, Parameters, TArray<FString>(), Results, Errors);

	for (const FString& TagName : Results)
	{
		FString Trimmed = TagName.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			TSharedRef<FArcGitSourceControlLabel> Label = MakeShared<FArcGitSourceControlLabel>(Trimmed);
			Label->PathToGitBinary = PathToGitBinary;
			Label->PathToRepositoryRoot = PathToRepositoryRoot;
			Labels.Add(Label);
		}
	}

	return Labels;
}

TArray<FSourceControlChangelistRef> FArcGitSourceControlProvider::GetChangelists( EStateCacheUsage::Type InStateCacheUsage )
{
	TArray<FSourceControlChangelistRef> Result;
	// Always include default changelist
	Result.Add(MakeShared<FArcGitSourceControlChangelist>(0));
	for (auto& Pair : ChangelistStateCache)
	{
		if (Pair.Key != 0) // Skip default, already added
		{
			Result.Add(MakeShared<FArcGitSourceControlChangelist>(Pair.Key));
		}
	}
	return Result;
}

#if SOURCE_CONTROL_WITH_SLATE
TSharedRef<class SWidget> FArcGitSourceControlProvider::MakeSettingsWidget() const
{
	return SNew(SArcGitSourceControlSettings);
}
#endif

ECommandResult::Type FArcGitSourceControlProvider::ExecuteSynchronousCommand(FArcGitSourceControlCommand& InCommand, const FText& Task)
{
	ECommandResult::Type Result = ECommandResult::Failed;

	// Display the progress dialog if a string was provided
	{
		FScopedSourceControlProgress Progress(Task);

		// Issue the command asynchronously...
		IssueCommand( InCommand );

		// ... then wait for its completion (thus making it synchronous)
		while(!InCommand.bExecuteProcessed)
		{
			// Tick the command queue and update progress.
			Tick();

			Progress.Tick();

			// Sleep for a bit so we don't busy-wait so much.
			FPlatformProcess::Sleep(0.01f);
		}

		// always do one more Tick() to make sure the command queue is cleaned up.
		Tick();

		if(InCommand.bCommandSuccessful)
		{
			Result = ECommandResult::Succeeded;
		}
	}

	// Delete the command now (asynchronous commands are deleted in the Tick() method)
	check(!InCommand.bAutoDelete);

	// ensure commands that are not auto deleted do not end up in the command queue
	if ( CommandQueue.Contains( &InCommand ) )
	{
		CommandQueue.Remove( &InCommand );
	}
	delete &InCommand;

	return Result;
}

ECommandResult::Type FArcGitSourceControlProvider::IssueCommand(FArcGitSourceControlCommand& InCommand)
{
	if(GThreadPool != nullptr)
	{
		// Queue this to our worker thread(s) for resolving
		GThreadPool->AddQueuedWork(&InCommand);
		CommandQueue.Add(&InCommand);
		return ECommandResult::Succeeded;
	}
	else
	{
		FText Message(LOCTEXT("NoSCCThreads", "There are no threads available to process the revision control command."));

		FMessageLog("SourceControl").Error(Message);
		InCommand.bCommandSuccessful = false;
		InCommand.Operation->AddErrorMessge(Message);

		return InCommand.ReturnResults();
	}
}
#undef LOCTEXT_NAMESPACE
