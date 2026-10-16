// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreSourceControlProvider.h"
#include "ArcLoreSourceControlState.h"
#include "ArcLoreNotificationSubscription.h"
#include "Misc/Paths.h"
#include "Misc/QueuedThreadPool.h"
#include "ArcLoreSourceControlCommand.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "ArcLoreSourceControlModule.h"
#include "ArcLoreSourceControlUtils.h"
#include "SArcLoreSourceControlSettings.h"
#include "SourceControlOperations.h"
#include "Logging/MessageLog.h"
#include "ScopedSourceControlProgress.h"

#define LOCTEXT_NAMESPACE "ArcLoreSourceControl"

static FName ProviderName("Lore");

namespace ArcLoreProviderInternal
{
	// Resolve incoming SCC paths to full absolute paths. The editor sends relatives
	// in TWO forms:
	//   * engine BaseDir-relative: "../../../ArcGame/Plugins/.../X.uasset" (the
	//     standard UE content path; the leading ../../../ climbs out of
	//     …/Engine/Binaries/Win64 up to the repo root), and
	//   * repo-root-relative:      "ArcGame/Plugins/.../X.uasset".
	// Resolving everything against one base breaks the other (repo-root resolution
	// turned "../../../ArcGame/…" into the garbage "G:/../../ArcGame/…", which hung
	// file_info). So resolve against the BaseDir first (the common case, and what
	// collapses ../../../ correctly); only if that does NOT name a real file, fall
	// back to the repo root. Absolute inputs pass through. Used by BOTH Execute() and
	// GetState() so the StateCache is keyed identically (else a lookup misses -> "?").
	static TArray<FString> ResolveAbsoluteFilenames(const TArray<FString>& InFiles, const FString& RepoRoot)
	{
		TArray<FString> Result;
		Result.Reserve(InFiles.Num());
		for (const FString& InFile : InFiles)
		{
			if (!FPaths::IsRelative(InFile))
			{
				Result.Add(FPaths::ConvertRelativePathToFull(InFile));
				continue;
			}

			// Standard resolution (relative to the process BaseDir).
			FString Abs = FPaths::ConvertRelativePathToFull(InFile);
			if (!FPaths::FileExists(Abs))
			{
				// Not a real file there — try the repo-root-relative interpretation.
				const FString FromRepoRoot = FPaths::ConvertRelativePathToFull(RepoRoot, InFile);
				if (FPaths::FileExists(FromRepoRoot))
				{
					Abs = FromRepoRoot;
				}
			}
			Result.Add(Abs);
		}
		return Result;
	}
}

void FArcLoreSourceControlProvider::Init(bool bForceConnection)
{
	// Init() is called multiple times at startup: only run discovery once.
	if(!bGitAvailable)
	{
		CheckGitAvailability();
	}

	// bForceConnection: not used anymore
}

void FArcLoreSourceControlProvider::CheckGitAvailability()
{
	// Lore discovery is pure filesystem: locating a .lore working copy is what makes the
	// provider "available". No git binary probe, no lore_* C calls here (the Connect worker
	// performs the lore_auth_local_user_info cross-check and sets BranchName — Step 6).
	CheckRepositoryStatus(FString());

	bGitAvailable = bGitRepositoryFound;

	UE_LOGF(LogSourceControl, Verbose,"ArcLore: availability check — LoreLibraryAvailable=%d, RepositoryFound=%d => IsEnabled=%d, IsAvailable=%d (root='%ls', user='%ls', remote='%ls')",
		bLoreLibraryAvailable ? 1 : 0, bGitRepositoryFound ? 1 : 0, IsEnabled() ? 1 : 0, IsAvailable() ? 1 : 0,
		*PathToRepositoryRoot, *UserName, *RemoteUrl);
}

void FArcLoreSourceControlProvider::CheckRepositoryStatus(const FString& /*InPathToGitBinary*/)
{
	// Reset discovered state.
	bGitRepositoryFound = false;
	PathToRepositoryRoot.Empty();
	UserName.Empty();
	RemoteUrl.Empty();
	BranchName.Empty();

	// Walk parents from the project dir up to the directory containing ".lore/".
	const FString PathToProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	bGitRepositoryFound = ArcLoreSourceControlUtils::FindLoreRepositoryRoot(PathToProjectDir, PathToRepositoryRoot);
	if(!bGitRepositoryFound)
	{
		UE_LOGF(LogSourceControl, Warning,"'%ls' is not part of a Lore working copy (no .lore directory found)", *FPaths::ProjectDir());
		return;
	}

	// Normalize to forward slashes for the stored root.
	FPaths::MakeStandardFilename(PathToRepositoryRoot);

	UE_LOGF(LogSourceControl, Verbose,"ArcLore: discovered Lore working copy at '%ls'", *PathToRepositoryRoot);

	// Parse .lore/config.toml for identity + remote_url (no TOML library). Missing keys are tolerated.
	const FString ConfigPath = PathToRepositoryRoot / TEXT(".lore") / TEXT("config.toml");
	if(!ArcLoreSourceControlUtils::ParseLoreConfig(ConfigPath, UserName, RemoteUrl))
	{
		UE_LOGF(LogSourceControl, Warning,"Lore working copy found at '%ls' but '%ls' could not be read", *PathToRepositoryRoot, *ConfigPath);
	}

	// BranchName intentionally left empty; the Connect worker (Step 6) populates it.
}

// Out-of-line ctor + dtor: FArcLoreNotificationSubscription is complete here, so the
// TUniquePtr member can be (default-)constructed and destroyed. Inline bodies in the
// header would instantiate the TUniquePtr dtor against the incomplete type (C4150).
FArcLoreSourceControlProvider::FArcLoreSourceControlProvider()
	: bGitAvailable(false)
	, bGitRepositoryFound(false)
	, bLoreLibraryAvailable(false)
{
}

FArcLoreSourceControlProvider::~FArcLoreSourceControlProvider() = default;

void FArcLoreSourceControlProvider::EnsureNotificationSubscription()
{
	check(IsInGameThread());
	if (bShuttingDown || bSubscriptionIntended || !IsAuthResolved() || GetPathToRepositoryRoot().IsEmpty())
	{
		return;
	}

	// Same identity (display name) the commands snapshot for their global args.
	Notification = MakeUnique<FArcLoreNotificationSubscription>(GetPathToRepositoryRoot(), GetUserName());
	Notification->Start();
	bSubscriptionIntended = true;
}

void FArcLoreSourceControlProvider::StopNotificationSubscription()
{
	check(IsInGameThread());
	if (!bSubscriptionIntended)
	{
		return;
	}

	// Stop() runs the SYNC unsubscribe (awaits lore's delivery task) — destroy the
	// object only AFTER it returns, so the queue outlives any in-flight callback.
	if (Notification)
	{
		Notification->Stop();
	}
	Notification.Reset();
	bSubscriptionIntended = false;
}

void FArcLoreSourceControlProvider::PrepareForNotificationShutdown()
{
	check(IsInGameThread());
	bShuttingDown = true;
	StopNotificationSubscription();
}

void FArcLoreSourceControlProvider::Close()
{
	// Stop the live-notification stream FIRST: set bShuttingDown (halts the Tick drain
	// dispatching new commands) and run the sync unsubscribe while lore is still up
	// (this runs at module shutdown BEFORE ArcLoreApi::Shutdown()). Idempotent — when
	// OnEnginePreExit already tore down, this is a bSubscriptionIntended==false no-op.
	PrepareForNotificationShutdown();

	// clear the cache
	StateCache.Empty();

	bGitAvailable = false;
	bGitRepositoryFound = false;
	UserName.Empty();
	UserEmail.Empty();

	// No ticker to unregister (the poll rides the provider's existing Tick()); just clear
	// the in-flight guard so a future Init() starts clean.
	bPollInFlight = false;
}

TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> FArcLoreSourceControlProvider::GetStateInternal(const FString& Filename)
{
	TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(Filename);
	if(State != NULL)
	{
		// found cached item
		return (*State);
	}
	else
	{
		// cache an unknown state for this item
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable( new FArcLoreSourceControlState(Filename) );
		StateCache.Add(Filename, NewState);
		return NewState;
	}
}

FText FArcLoreSourceControlProvider::GetStatusText() const
{
	FString StatusString = FString::Printf(
		TEXT("Lore | branch: %s | remote: %s"),
		*BranchName,
		RemoteUrl.IsEmpty() ? TEXT("none") : *RemoteUrl
	);

	if (CachedNumLocalChanges.IsSet())
	{
		StatusString += FString::Printf(TEXT(" | %d local changes"), CachedNumLocalChanges.GetValue());
	}

	return FText::FromString(StatusString);
}

TMap<ISourceControlProvider::EStatus, FString> FArcLoreSourceControlProvider::GetStatus() const
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
bool FArcLoreSourceControlProvider::IsEnabled() const
{
	// Enabled only when the lore library loaded AND a working copy was discovered.
	// If lore.dll failed to load, the provider must not dispatch ops that would
	// touch a lore_* import.
	return bLoreLibraryAvailable && bGitRepositoryFound;
}

/** Quick check if source control is available for use (useful for server-based providers) */
bool FArcLoreSourceControlProvider::IsAvailable() const
{
	// Available requires both the lore library (dll loaded) AND a discovered repo.
	return bLoreLibraryAvailable && bGitRepositoryFound;
}

const FName& FArcLoreSourceControlProvider::GetName(void) const
{
	return ProviderName;
}

ECommandResult::Type FArcLoreSourceControlProvider::GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage )
{
	if(!IsEnabled())
	{
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles = ArcLoreProviderInternal::ResolveAbsoluteFilenames(InFiles, PathToRepositoryRoot);

	// MVP cache policy: both Use and ForceUpdate serve the cached state (creating an
	// "unknown" state on miss via GetStateInternal). We deliberately do NOT fire a
	// synchronous backend query on ForceUpdate — the editor refreshes file states via the
	// async FUpdateStatus operation (Step 8), so blocking here on a per-file Lore query is
	// both unnecessary and undesirable. Revisit if a synchronous force-refresh is ever needed.
	for(const FString& AbsoluteFile : AbsoluteFiles)
	{
		OutState.Add(GetStateInternal(*AbsoluteFile));
	}

	return ECommandResult::Succeeded;
}

ECommandResult::Type FArcLoreSourceControlProvider::GetState(const TArray<FSourceControlChangelistRef>& InChangelists, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	// Changelists not supported in MVP; return empty success
	return ECommandResult::Succeeded;
}

TArray<FSourceControlStateRef> FArcLoreSourceControlProvider::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const
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

bool FArcLoreSourceControlProvider::RemoveFileFromCache(const FString& Filename)
{
	return StateCache.Remove(Filename) > 0;
}

FDelegateHandle FArcLoreSourceControlProvider::RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged )
{
	return OnSourceControlStateChanged.Add( SourceControlStateChanged );
}

void FArcLoreSourceControlProvider::UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle )
{
	OnSourceControlStateChanged.Remove( Handle );
}

ECommandResult::Type FArcLoreSourceControlProvider::Execute( const FSourceControlOperationRef& InOperation, FSourceControlChangelistPtr InChangelist, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate )
{
	UE_LOGF(LogSourceControl, Verbose,"ArcLore: Execute('%ls') — %d file(s), concurrency=%ls, IsEnabled=%d",
		*InOperation->GetName().ToString(), InFiles.Num(),
		InConcurrency == EConcurrency::Synchronous ? TEXT("Sync") : TEXT("Async"), IsEnabled() ? 1 : 0);

	if(!IsEnabled() && !(InOperation->GetName() == "Connect")) // Only Connect operation allowed while not Enabled (Connected)
	{
		UE_LOGF(LogSourceControl, Warning,"ArcLore: Execute('%ls') rejected — provider not enabled (LoreLibraryAvailable=%d, RepositoryFound=%d).",
			*InOperation->GetName().ToString(), bLoreLibraryAvailable ? 1 : 0, bGitRepositoryFound ? 1 : 0);
		InOperationCompleteDelegate.ExecuteIfBound(InOperation, ECommandResult::Failed);
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles = ArcLoreProviderInternal::ResolveAbsoluteFilenames(InFiles, PathToRepositoryRoot);

	// [diag] The MarkForAdd stage failed with an "Invalid path" carrying a spurious
	// Engine/Binaries/Win64 prefix — i.e. an incoming path was relative and got made
	// absolute against the editor BaseDir. Log raw input vs resolved absolute (and
	// the repo root) to confirm whether the editor hands us a relative path or our
	// AbsoluteFilenames resolution is wrong.
	for (int32 FileIndex = 0; FileIndex < InFiles.Num(); ++FileIndex)
	{
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: Execute('%ls') path[%d]: in='%ls' -> abs='%ls' (repoRoot='%ls')",
			*InOperation->GetName().ToString(), FileIndex, *InFiles[FileIndex], *AbsoluteFiles[FileIndex], *PathToRepositoryRoot);
	}

	// Query to see if we allow this operation
	TSharedPtr<IArcLoreSourceControlWorker, ESPMode::ThreadSafe> Worker = CreateWorker(InOperation->GetName());
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

	FArcLoreSourceControlCommand* Command = new FArcLoreSourceControlCommand(InOperation, Worker.ToSharedRef());
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

bool FArcLoreSourceControlProvider::CanExecuteOperation( const FSourceControlOperationRef& InOperation ) const
{
	return WorkersMap.Find(InOperation->GetName()) != nullptr;
}

bool FArcLoreSourceControlProvider::CanCancelOperation( const FSourceControlOperationRef& InOperation ) const
{
	return true;
}

void FArcLoreSourceControlProvider::CancelOperation( const FSourceControlOperationRef& InOperation )
{
	for (int32 i = 0; i < CommandQueue.Num(); ++i)
	{
		FArcLoreSourceControlCommand& Command = *CommandQueue[i];
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

TOptional<bool> FArcLoreSourceControlProvider::HasChangesToSync() const
{
	if (bCachedIsAtLatest.IsSet())
	{
		return TOptional<bool>(!bCachedIsAtLatest.GetValue());
	}
	return TOptional<bool>();
}

TOptional<bool> FArcLoreSourceControlProvider::HasChangesToCheckIn() const
{
	if (CachedNumLocalChanges.IsSet())
	{
		return TOptional<bool>(CachedNumLocalChanges.GetValue() > 0);
	}
	return TOptional<bool>();
}

TSharedPtr<IArcLoreSourceControlWorker, ESPMode::ThreadSafe> FArcLoreSourceControlProvider::CreateWorker(const FName& InOperationName) const
{
	const FGetArcLoreSourceControlWorker* Operation = WorkersMap.Find(InOperationName);
	if(Operation != nullptr)
	{
		return Operation->Execute();
	}

	return nullptr;
}

void FArcLoreSourceControlProvider::RegisterWorker( const FName& InName, const FGetArcLoreSourceControlWorker& InDelegate )
{
	WorkersMap.Add( InName, InDelegate );
}

void FArcLoreSourceControlProvider::OutputCommandMessages(const FArcLoreSourceControlCommand& InCommand) const
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

void FArcLoreSourceControlProvider::Tick()
{
	bool bStatesUpdated = false;
	for(int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FArcLoreSourceControlCommand& Command = *CommandQueue[CommandIndex];
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

	// Live-notification drain (game thread; sole consumer of the subscription's MPSC
	// queue). Any push event marks a refresh pending. The flag is PERSISTENT, not a
	// per-tick bool: events arriving while a refresh/poll is in flight still get
	// serviced once idle, and a burst coalesces to a single FUpdateStatus.
	if(Notification.IsValid())
	{
		Notification->DrainInto([this](const FArcLoreNotificationEvent& /*Event*/)
		{
			bNotificationRefreshPending = true;
		});
	}

	// Dispatch the coalesced refresh when idle, under the SAME guard as the 20s poll
	// (no command in flight, no poll in flight) so refreshes never stack. FUpdateStatus
	// refreshes file status AND per-file lock badges; its completion broadcasts
	// OnSourceControlStateChanged. Suppressed while shutting down.
	if(bNotificationRefreshPending && !bShuttingDown && IsAvailable() && CommandQueue.Num() == 0 && !bPollInFlight)
	{
		bNotificationRefreshPending = false;
		Execute(
			ISourceControlOperation::Create<FUpdateStatus>(),
			nullptr,
			TArray<FString>({ GetPathToRepositoryRoot() }),
			EConcurrency::Asynchronous,
			FSourceControlOperationComplete());
	}

	// Throttled background refresh of ahead/behind state. Runs on the game thread from
	// this Tick(), independently of any panel — it broadcasts via OnSourceControlStateChanged
	// (through the normal Tick path of the dispatched Connect worker), so there is no
	// panel pointer and no use-after-free when the panel is closed.
	//
	// Dispatched AFTER the command-processing loop + broadcast above (never inside the
	// loop), so it can never modify CommandQueue mid-iteration. The lightweight op is
	// FConnect, which runs lore_repository_status(revision_only=1) + lore_auth_local_user_info
	// (cheap — no repo-wide discover). Its UpdateStates refreshes ahead/behind + identity
	// and returns true, so the broadcast happens on a later Tick via the normal drain.
	//
	// Gating: only when connected (IsAvailable()), idle (no command in flight), no poll
	// already in flight, and the throttle interval has elapsed. Execute() merely enqueues
	// the async command onto CommandQueue (IssueCommand → GThreadPool->AddQueuedWork);
	// the actual worker runs on a pool thread and is drained on a later Tick — it does not
	// synchronously re-enter Tick() (only the synchronous path loops on Tick(), and this
	// dispatch is Asynchronous).
	const double NowSeconds = FPlatformTime::Seconds();
	if(IsAvailable() && CommandQueue.Num() == 0 && !bPollInFlight && (NowSeconds - LastPollSeconds) >= PollIntervalSeconds)
	{
		LastPollSeconds = NowSeconds;
		bPollInFlight = true;
		Execute(
			ISourceControlOperation::Create<FConnect>(),
			nullptr,
			TArray<FString>(),
			EConcurrency::Asynchronous,
			FSourceControlOperationComplete::CreateRaw(this, &FArcLoreSourceControlProvider::OnPollComplete));
	}
}

void FArcLoreSourceControlProvider::OnPollComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	bPollInFlight = false;
}

void FArcLoreSourceControlProvider::SetCachedRemoteState(bool bRemoteAhead, bool bLocalAhead, TOptional<int> NumUnpushed)
{
	bCachedLocalAhead = bLocalAhead;
	CachedNumUnpushedCommits = NumUnpushed;
	// Keep HasChangesToSync() working: "at latest" iff the remote is not ahead.
	bCachedIsAtLatest = !bRemoteAhead;
}

TArray< TSharedRef<ISourceControlLabel> > FArcLoreSourceControlProvider::GetLabels( const FString& InMatchingSpec ) const
{
	return TArray<TSharedRef<ISourceControlLabel>>();
}

TArray<FSourceControlChangelistRef> FArcLoreSourceControlProvider::GetChangelists( EStateCacheUsage::Type InStateCacheUsage )
{
	// Changelists not supported in MVP
	return TArray<FSourceControlChangelistRef>();
}

#if SOURCE_CONTROL_WITH_SLATE
TSharedRef<class SWidget> FArcLoreSourceControlProvider::MakeSettingsWidget() const
{
	return SNew(SArcLoreSourceControlSettings);
}
#endif

ECommandResult::Type FArcLoreSourceControlProvider::ExecuteSynchronousCommand(FArcLoreSourceControlCommand& InCommand, const FText& Task)
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

ECommandResult::Type FArcLoreSourceControlProvider::IssueCommand(FArcLoreSourceControlCommand& InCommand)
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
