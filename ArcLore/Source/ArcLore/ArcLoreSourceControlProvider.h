// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlProvider.h"
#include "IArcLoreSourceControlWorker.h"
#include "Templates/UniquePtr.h"

class FArcLoreSourceControlState;

class FArcLoreSourceControlCommand;

// Heap-owned live-notification stream for the discovered repo. Held by TUniquePtr;
// the type is incomplete here (full def in ArcLoreNotificationSubscription.h,
// pulled into the .cpp), so the provider declares an out-of-line destructor that
// is defined where the type is complete.
class FArcLoreNotificationSubscription;

DECLARE_DELEGATE_RetVal(FArcLoreSourceControlWorkerRef, FGetArcLoreSourceControlWorker)


class FArcLoreSourceControlProvider : public ISourceControlProvider
{
public:
	/** Constructor + destructor defined out-of-line in the .cpp, where
	 *  FArcLoreNotificationSubscription is complete: an inline body would instantiate
	 *  the TUniquePtr destructor here against the incomplete type (C4150). */
	FArcLoreSourceControlProvider();
	virtual ~FArcLoreSourceControlProvider();

	/* ISourceControlProvider implementation */
	virtual void Init(bool bForceConnection = true) override;
	virtual void Close() override;
	virtual FText GetStatusText() const override;
	virtual TMap<EStatus, FString> GetStatus() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsAvailable() const override;
	virtual const FName& GetName(void) const override;
	virtual bool QueryStateBranchConfig(const FString& ConfigSrc, const FString& ConfigDest) override { return false; }
	virtual void RegisterStateBranches(const TArray<FString>& BranchNames, const FString& ContentRoot) override {}
	virtual int32 GetStateBranchIndex(const FString& InBranchName) const override { return INDEX_NONE; }
	virtual bool GetStateBranchAtIndex(int32 BranchIndex, FString& OutBranchName) const override { return false; }
	virtual ECommandResult::Type GetState( const TArray<FString>& InFiles, TArray<FSourceControlStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage ) override;
	virtual ECommandResult::Type GetState(const TArray<FSourceControlChangelistRef>& InChangelists, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage) override;
	virtual TArray<FSourceControlStateRef> GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const override;
	virtual FDelegateHandle RegisterSourceControlStateChanged_Handle(const FSourceControlStateChanged::FDelegate& SourceControlStateChanged) override;
	virtual void UnregisterSourceControlStateChanged_Handle(FDelegateHandle Handle) override;
	virtual ECommandResult::Type Execute(const FSourceControlOperationRef& InOperation, FSourceControlChangelistPtr InChangelist, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete()) override;
	virtual bool CanExecuteOperation( const FSourceControlOperationRef& InOperation ) const override;
	virtual bool CanCancelOperation( const FSourceControlOperationRef& InOperation ) const override;
	virtual void CancelOperation( const FSourceControlOperationRef& InOperation ) override;
	virtual bool UsesLocalReadOnlyState() const override { return false; }
	virtual bool UsesChangelists() const override { return false; }
	virtual bool UsesUncontrolledChangelists() const override { return false; }
	virtual bool UsesCheckout() const override { return false; }
	virtual bool UsesFileRevisions() const override { return true; }
	virtual bool UsesSnapshots() const override { return false; }
	// Previously false because history population and ISourceControlRevision::Get() were not yet implemented.
	// Now true: revision_info metadata populates history entries (Step 1), and ISourceControlRevision::Get()
	// materializes the prior .uasset via lore_file_write, so the engine's native "Diff Against Depot" in the
	// Content Browser and history-window diff entries are fully functional.
	virtual bool AllowsDiffAgainstDepot() const override { return true; }
	virtual bool UsesSoftRevertOnDelete() const override { return false; }
	virtual TOptional<bool> HasChangesToSync() const override;
	virtual TOptional<bool> HasChangesToCheckIn() const override;
	virtual void Tick() override;
	virtual TArray< TSharedRef<class ISourceControlLabel> > GetLabels( const FString& InMatchingSpec ) const override;
	virtual TArray<FSourceControlChangelistRef> GetChangelists( EStateCacheUsage::Type InStateCacheUsage ) override;
#if SOURCE_CONTROL_WITH_SLATE
	virtual TSharedRef<class SWidget> MakeSettingsWidget() const override;
#endif

	using ISourceControlProvider::Execute;

	/**
	 * Check configuration, else standard paths, and run a Git "version" command to check the availability of the binary.
	 */
	void CheckGitAvailability();

	/**
	 * Find the .git/ repository and check it's status.
	 */
	void CheckRepositoryStatus(const FString& InPathToGitBinary);

	/** Is git binary found and working. */
	inline bool IsGitAvailable() const
	{
		return bGitAvailable;
	}

	/** Set by the module at startup from ArcLoreApi::Init(); gates IsAvailable()/IsEnabled() so a failed dll load degrades the provider to UNAVAILABLE. */
	inline void SetLoreLibraryAvailable(bool bInAvailable)
	{
		bLoreLibraryAvailable = bInAvailable;
	}

	/** True when lore.dll loaded successfully (ArcLoreApi::Init() succeeded). */
	inline bool IsLoreLibraryAvailable() const
	{
		return bLoreLibraryAvailable;
	}

	/** Get the path to the root of the Git repository: can be the ProjectDir itself, or any parent directory */
	inline const FString& GetPathToRepositoryRoot() const
	{
		return PathToRepositoryRoot;
	}

	/** Git config user.name */
	inline const FString& GetUserName() const
	{
		return UserName;
	}

	/** Set the resolved Lore identity (display name) — called from the Connect worker's UpdateStates on the game thread */
	inline void SetUserName(const FString& In)
	{
		UserName = In;
	}

	/** Resolved Lore user id (stable identity used for lock owner-id comparison) */
	inline const FString& GetUserId() const
	{
		return LoreUserId;
	}

	/** Set the resolved Lore user id — called from the auth workers' UpdateStates on the game thread */
	inline void SetUserId(const FString& In)
	{
		LoreUserId = In;
	}

	/** True once an auth op resolved a Lore identity (login / Connect auth). Cleared on logout. */
	inline bool IsAuthResolved() const
	{
		return bAuthResolved;
	}

	/** Set the auth-resolved flag — called from the auth workers' UpdateStates on the game thread */
	inline void SetAuthResolved(bool bIn)
	{
		bAuthResolved = bIn;
	}

	/** Git config user.email */
	inline const FString& GetUserEmail() const
	{
		return UserEmail;
	}

	/** Git remote origin url */
	inline const FString& GetRemoteUrl() const
	{
		return RemoteUrl;
	}

	/** Current branch name */
	inline const FString& GetBranchName() const
	{
		return BranchName;
	}

	/** Set the current branch name — called from the Connect worker's UpdateStates on the game thread */
	inline void SetBranchName(const FString& In)
	{
		BranchName = In;
	}

	/** Ensure the live-notification subscription is running. Game-thread only.
	 *  Idempotent: short-circuits on bSubscriptionIntended, so the 20s FConnect poll
	 *  re-entry (which re-runs the Connect worker's UpdateStates) never re-subscribes.
	 *  No-op unless auth is resolved and a repo root is known, and never starts a new
	 *  subscription while shutting down. Built with the SAME identity (GetUserName())
	 *  the commands use, keyed by GetPathToRepositoryRoot(). */
	void EnsureNotificationSubscription();

	/** Stop + destroy the live-notification subscription. Game-thread only.
	 *  Idempotent (guarded by bSubscriptionIntended). Stop() runs the SYNC lore
	 *  unsubscribe (awaits lore's delivery task) FIRST; only then is the object
	 *  destroyed — the subscription + its queue outlive any in-flight callback. */
	void StopNotificationSubscription();

	/** Single idempotent teardown entry, called from BOTH OnEnginePreExit and Close().
	 *  Sets bShuttingDown (stops the Tick drain dispatching new commands) then
	 *  synchronously unsubscribes. The bSubscriptionIntended guard makes the actual
	 *  lore unsubscribe fire exactly once across the two callers. */
	void PrepareForNotificationShutdown();

	/** Completion delegate for the periodic background poll (see Tick()). Clears the
	 *  in-flight guard; the Connect worker's UpdateStates already broadcasts ahead/behind
	 *  via the normal Tick path, so no extra refresh is done here. */
	void OnPollComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	/** Helper function used to update state cache */
	TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> GetStateInternal(const FString& Filename);

	/**
	 * Register a worker with the provider.
	 * This is used internally so the provider can maintain a map of all available operations.
	 */
	void RegisterWorker( const FName& InName, const FGetArcLoreSourceControlWorker& InDelegate );

	/** Remove a named file from the state cache */
	bool RemoveFileFromCache(const FString& Filename);

	/** Get LFS tracked file patterns */
	const TArray<FString>& GetLfsPatterns() const { return LfsPatterns; }
	TArray<FString>& AccessLfsPatterns() { return LfsPatterns; }

	/** Set the cached local changes count (called by UpdateStatus worker) */
	void SetCachedNumLocalChanges(int InCount) { CachedNumLocalChanges = InCount; }

	/** Set the cached staged-file count (called by UpdateStatus worker). Drives the
	 *  panel's pre-switch warning (a staged revision hard-errors a non-reset switch). */
	void SetCachedStagedCount(int In) { CachedStagedCount = In; }

	/** Number of staged files at the last status, when known; unset before the first
	 *  status pass. */
	TOptional<int> GetCachedStagedCount() const { return CachedStagedCount; }

	/** Set the cached remote/local-ahead state from a Lore status_revision event
	 *  (called by the UpdateStatus worker's UpdateStates on the game thread).
	 *  Also writes bCachedIsAtLatest = !bRemoteAhead so HasChangesToSync() keeps
	 *  working now that the git-shell remote poll is gone. NumUnpushed is unset
	 *  unless a reliable count is available (see count caveat in the worker). */
	void SetCachedRemoteState(bool bRemoteAhead, bool bLocalAhead, TOptional<int> NumUnpushed);

	/** True when the local branch holds revisions not on the remote (committed,
	 *  not pushed). Drives the Push affordance. */
	bool IsLocalAhead() const { return bCachedLocalAhead; }

	/** Number of local commits not yet pushed, when a reliable count is known;
	 *  unset otherwise (rely on IsLocalAhead() for the boolean affordance). */
	TOptional<int> GetCachedNumUnpushedCommits() const { return CachedNumUnpushedCommits; }

private:

	/** Is git binary found and working. */
	bool bGitAvailable;

	/** Is git repository found. */
	bool bGitRepositoryFound;

	/** True when lore.dll loaded successfully (set by the module from ArcLoreApi::Init()). */
	bool bLoreLibraryAvailable;

	/** Helper function for Execute() */
	TSharedPtr<class IArcLoreSourceControlWorker, ESPMode::ThreadSafe> CreateWorker(const FName& InOperationName) const;

	/** Helper function for running command synchronously. */
	ECommandResult::Type ExecuteSynchronousCommand(class FArcLoreSourceControlCommand& InCommand, const FText& Task);
	/** Issue a command asynchronously if possible. */
	ECommandResult::Type IssueCommand(class FArcLoreSourceControlCommand& InCommand);

	/** Output any messages this command holds */
	void OutputCommandMessages(const class FArcLoreSourceControlCommand& InCommand) const;

	/** Path to the root of the Git repository: can be the ProjectDir itself, or any parent directory (found by the "Connect" operation) */
	FString PathToRepositoryRoot;

	/** Git config user.name (from local repository, else globally) */
	FString UserName;

	/** Resolved Lore user id (auth identity), persisted by the auth workers. Used for lock owner-id comparison. */
	FString LoreUserId;

	/** True once an auth op resolved a Lore identity; cleared on logout. */
	bool bAuthResolved = false;

	/** Git config user.email (from local repository, else globally) */
	FString UserEmail;

	/** Name of the current branch */
	FString BranchName;

	/** URL of the "origin" default remote server */
	FString RemoteUrl;

	/** State cache */
	TMap<FString, TSharedRef<class FArcLoreSourceControlState, ESPMode::ThreadSafe> > StateCache;

	/** The currently registered source control operations */
	TMap<FName, FGetArcLoreSourceControlWorker> WorkersMap;

	/** Queue for commands given by the main thread */
	TArray < FArcLoreSourceControlCommand* > CommandQueue;

	/** For notifying when the source control states in the cache have changed */
	FSourceControlStateChanged OnSourceControlStateChanged;

	/** LFS tracking patterns from .gitattributes */
	TArray<FString> LfsPatterns;

	/** Remote tracking (cached) */
	TOptional<bool> bCachedIsAtLatest;
	TOptional<int> CachedNumLocalChanges;

	/** Staged-file count at the last status pass; unset until the first UpdateStatus. */
	TOptional<int> CachedStagedCount;

	/** Local branch ahead of remote (committed-but-not-pushed); from status_revision. */
	bool bCachedLocalAhead = false;

	/** Unpushed-commit count when reliably known; unset otherwise. */
	TOptional<int> CachedNumUnpushedCommits;

	/** Throttle for the periodic background refresh (see Tick()): wall-clock seconds
	 *  (FPlatformTime::Seconds()) of the last poll dispatch. */
	double LastPollSeconds = 0.0;

	/** True while a background poll op is in flight; prevents overlapping polls. */
	bool bPollInFlight = false;

	/** Minimum interval between background ahead/behind polls. */
	static constexpr double PollIntervalSeconds = 20.0;

	/** Live-notification stream for the discovered repo. Constructed by
	 *  EnsureNotificationSubscription(), destroyed by StopNotificationSubscription()
	 *  (after its sync Stop() returns). Game-thread-only access. */
	TUniquePtr<FArcLoreNotificationSubscription> Notification;

	/** Game-thread-only intent flag: true between EnsureNotificationSubscription()
	 *  and StopNotificationSubscription(). The idempotency gate against the 20s
	 *  FConnect re-entry; NOT the subscription's atomic enqueue flag. */
	bool bSubscriptionIntended = false;

	/** Set once teardown begins (Close()/OnEnginePreExit): blocks new notification
	 *  subscriptions and stops the Tick drain dispatching new refresh commands. */
	bool bShuttingDown = false;

	/** Persistent pending-refresh flag: set when the drain sees any notification
	 *  event, cleared only when a coalesced FUpdateStatus is actually dispatched. A
	 *  persistent flag (not a per-tick local) so events arriving while a refresh/poll
	 *  is in flight still trigger a refresh once idle — the signal is never lost, and
	 *  a burst coalesces to one FUpdateStatus. */
	bool bNotificationRefreshPending = false;
};
