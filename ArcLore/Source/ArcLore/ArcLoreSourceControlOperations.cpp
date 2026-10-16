// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreSourceControlOperations.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "ArcLoreSourceControlModule.h"
#include "ArcLoreSourceControlCommand.h"
#include "ArcLoreSourceControlUtils.h"
#include "ArcLoreApi.h"

#define LOCTEXT_NAMESPACE "ArcLoreSourceControl"

// Shared path-conversion helpers used by multiple workers.
// Defined before any worker that needs them so they are visible at point of use.
namespace ArcLorePathHelpers
{
	// abs -> repo-relative, forward-slash normalized (what the Lore args want).
	FString ToRepoRelative(const FString& AbsFilename, const FString& RepoRoot)
	{
		FString Relative = FPaths::ConvertRelativePathToFull(AbsFilename);
		FString Root = RepoRoot;
		FPaths::NormalizeDirectoryName(Root);
		FPaths::MakePathRelativeTo(Relative, *(Root / TEXT("")));
		FPaths::NormalizeFilename(Relative); // forward slashes
		return Relative;
	}

	// repo-relative -> abs (the StateCache key, matching UE's InCommand.Files).
	FString ToAbsolute(const FString& RepoRelative, const FString& RepoRoot)
	{
		return FPaths::ConvertRelativePathToFull(RepoRoot, RepoRelative);
	}
}

FName FArcLoreConnectWorker::GetName() const
{
	return "Connect";
}

// ----------------------------------------------------------------------------
// Connect — the CANONICAL Lore-op worker (Execute / collector / UpdateStates).
// Every later Lore worker copies this shape:
//   * Execute() runs on a thread-pool thread (never the game thread); it builds
//     global args ONCE, then drives each op through ArcLoreApi::RunLoreOp.
//   * Each op pairs an INVOKER (calls the concrete lore_x with the built config)
//     with a COLLECTOR (copies POD/strings from the lore-owned callback thread
//     into worker-owned scratch — never touches the provider/UObjects/GT state).
//   * UpdateStates() runs on the game thread and is the ONLY place the worker
//     reaches the provider, applying the scratch it gathered in Execute().
// ----------------------------------------------------------------------------
bool FArcLoreConnectWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	// The Lore library is loaded at module init (delay-load target). Calling any
	// lore_* import before that crashes the delay-load stub, so gate here.
	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		StaticCastSharedRef<FConnect>(InCommand.Operation)->SetErrorText(LOCTEXT("LoreNotReady", "Failed to enable Lore revision control. The Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Build the global args ONCE for the whole Execute. The scope owns the UTF-8
	// backing buffers that the lore_string_t views point into, so it MUST outlive
	// every RunLoreOp call below.
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	const FString RemoteUrlForLog = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore").GetProvider().GetRemoteUrl();
	UE_LOGF(LogSourceControl, Verbose,"ArcLore: Connect — attempting to reach Lore server (remote='%ls', repo root='%ls', identity='%ls')",
		RemoteUrlForLog.IsEmpty() ? TEXT("<none>") : *RemoteUrlForLog,
		*InCommand.PathToRepositoryRoot,
		InCommand.Identity.IsEmpty() ? TEXT("<none>") : *InCommand.Identity);

	// ---- AUTH op (soft) --------------------------------------------------------
	// Resolve the local user's identity. A failure here is NOT fatal — the user
	// may simply not be logged in — so it only records an info message.
	{
		lore_auth_local_user_info_args_t AuthArgs;
		FMemory::Memzero(&AuthArgs, sizeof(AuthArgs));
		AuthArgs.auth_endpoint = ArcLoreApi::EmptyLoreString();

		const ArcLoreApi::FLoreOpInvoker AuthInvoker =
			[&GlobalArgs, &AuthArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_auth_local_user_info(&GlobalArgs.Args(), &AuthArgs, Config);
			};

		// COLLECTOR TEMPLATE (runs on the LORE worker thread):
		//  - copies POD/strings into worker-owned scratch ONLY (never touches the
		//    provider, UObjects, or any game-thread state);
		//  - switches on lore_event_type() for the one domain event it needs;
		//  - silently ignores every other event (RunLoreOp handles ERROR/COMPLETE/END).
		const ArcLoreApi::FLoreEventCollector AuthCollector =
			[this](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				switch (Tag)
				{
				case LORE_EVENT_AUTH_USER_INFO:
					InfoUserName = ArcLoreApi::LoreStringToFString(Event.auth_user_info.name);
					InfoUserId = ArcLoreApi::LoreStringToFString(Event.auth_user_info.id);
					bAuthResolved = true;
					break;
				default:
					break;
				}
			};

		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [Connect/auth] args: auth_endpoint='%ls' (empty => resolve from repo remote env); globals: %ls",
			*ArcLoreApi::LoreStringToFString(AuthArgs.auth_endpoint),
			*ArcLoreApi::DescribeGlobalArgs(GlobalArgs.Args()));

		FString AuthError;
		const int32 AuthResult = ArcLoreApi::RunLoreOp(AuthInvoker, AuthCollector, AuthError, TEXT("Connect/auth"));
		if (AuthResult != 0)
		{
			// Soft: the user may not be logged in. Record, do not fail Connect.
			InCommand.InfoMessages.Add(FString::Printf(TEXT("Lore identity not resolved: %s"), *AuthError));
			UE_LOGF(LogSourceControl, Warning,"ArcLore: Connect — identity NOT resolved (user may not be logged in): %ls", *AuthError);
		}
		else
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: Connect — identity resolved as '%ls'", *InfoUserName);
		}
	}

	// ---- STATUS op (authoritative for reachability) ----------------------------
	// revision_only=1 skips file diffs and the filesystem scan — Connect only
	// needs the branch/revision info to confirm the repo is reachable.
	{
		lore_repository_status_args_t StatusArgs;
		FMemory::Memzero(&StatusArgs, sizeof(StatusArgs));
		StatusArgs.revision_only = 1;

		const ArcLoreApi::FLoreOpInvoker StatusInvoker =
			[&GlobalArgs, &StatusArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_status(&GlobalArgs.Args(), &StatusArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StatusCollector =
			[this](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				switch (Tag)
				{
				case LORE_EVENT_REPOSITORY_STATUS_REVISION:
					InfoBranchName = ArcLoreApi::LoreStringToFString(Event.repository_status_revision.branch_name);
					break;
				default:
					break;
				}
			};

		FString StatusError;
		const int32 StatusResult = ArcLoreApi::RunLoreOp(StatusInvoker, StatusCollector, StatusError, TEXT("Connect/status"));

		// Repo reachable == status op succeeded. This is the authoritative result.
		InCommand.bCommandSuccessful = (StatusResult == 0);
		if (!InCommand.bCommandSuccessful)
		{
			InCommand.ErrorMessages.Add(StatusError);
			StaticCastSharedRef<FConnect>(InCommand.Operation)->SetErrorText(LOCTEXT("LoreConnectFailed", "Failed to enable Lore revision control. The repository could not be reached."));
			UE_LOGF(LogSourceControl, Error,"ArcLore: Connect — FAILED to reach Lore server (remote='%ls'): %ls",
				RemoteUrlForLog.IsEmpty() ? TEXT("<none>") : *RemoteUrlForLog,
				StatusError.IsEmpty() ? TEXT("(no error message)") : *StatusError);
		}
		else
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: Connect — CONNECTED to Lore server (remote='%ls', branch='%ls')",
				RemoteUrlForLog.IsEmpty() ? TEXT("<none>") : *RemoteUrlForLog,
				InfoBranchName.IsEmpty() ? TEXT("<unknown>") : *InfoBranchName);
		}
	}

	return InCommand.bCommandSuccessful;
}

bool FArcLoreConnectWorker::UpdateStates() const
{
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	bool bUpdated = false;
	if (!InfoBranchName.IsEmpty())
	{
		Provider.SetBranchName(InfoBranchName);
		bUpdated = true;
	}
	if (bAuthResolved && !InfoUserName.IsEmpty())
	{
		// Persist the full resolved identity so the lock owner-id comparison (Phase 3)
		// has the stable user id, not just the display name.
		Provider.SetUserName(InfoUserName);
		Provider.SetUserId(InfoUserId);
		Provider.SetAuthResolved(bAuthResolved);

		// Start the live-notification stream now that auth + repo root are known.
		// Idempotent: re-runs every 20s via the FConnect poll, but the provider's
		// bSubscriptionIntended gate short-circuits all but the first.
		Provider.EnsureNotificationSubscription();
		bUpdated = true;
	}
	return bUpdated;
}

FName FArcLoreCheckInWorker::GetName() const
{
	return "CheckIn";
}

// ----------------------------------------------------------------------------
// CheckIn — LOCAL-COMMIT worker: stage -> commit -> re-query ahead/behind.
//
//   1. lore_file_stage(paths)   — stage the working-copy changes. Fail => nothing
//      committed; error + false.
//   2. lore_revision_commit     — message = FCheckIn::GetDescription(). The new
//      revision arrives on the REVISION_COMMIT_REVISION event (captured as a hex
//      hash for the success message). Fail => error + false.
//   3. lore_repository_status (revision_only=1) — cheap re-query of ahead/behind
//      so the new local commit immediately shows as "to push". Soft: a failed
//      re-query just leaves the cached state stale; commit already succeeded.
//
// Push and lock-release are NO LONGER part of CheckIn — they live in the separate
// FArcLorePushWorker. Committing locally must RETAIN locks: releasing on a still-
// unpushed commit would let another user lock+diverge the asset. Engine Submit now
// commits-without-push by design; the user pushes explicitly via the Push op.
//
// All ops share one FLoreGlobalArgsScope and the single absolute path list.
// Execute + the status collector run on the POOL thread (worker-local members
// only); UpdateStates runs on the GAME thread and is the only place the provider
// is touched.
// ----------------------------------------------------------------------------
namespace ArcLoreCheckInInternal
{
	// Format a lore_hash_t as a short hex prefix for the success message.
	FString HashToHexPrefix(const lore_hash_t& Hash)
	{
		FString Hex;
		const int32 PrefixBytes = 6; // 12 hex chars — enough to identify the revision.
		for (int32 ByteIndex = 0; ByteIndex < PrefixBytes; ++ByteIndex)
		{
			Hex += FString::Printf(TEXT("%02x"), Hash.data[ByteIndex]);
		}
		return Hex;
	}
}

bool FArcLoreCheckInWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	TSharedRef<FCheckIn, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCheckIn>(InCommand.Operation);

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Build the path list for lore file-mutation ops. Unlike status/file_info
	// (which match a repo-relative string against the tracked index), lore_file_stage
	// resolves a RELATIVE path against the process working dir (…/Engine/Binaries/Win64)
	// — sending "ArcGame/Plugins/…" produced "Invalid path Engine/Binaries/Win64/ArcGame/…".
	// So feed stage/release the ABSOLUTE path; lore derives the repo-relative form from
	// repository_path itself.
	// CommittedFiles tracks the absolute paths so UpdateStates can key the StateCache.
	TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
	PathBuffers.Reserve(InCommand.Files.Num());
	TArray<lore_string_t> PathViews;
	PathViews.Reserve(InCommand.Files.Num());
	TArray<FString> AbsFiles;
	AbsFiles.Reserve(InCommand.Files.Num());
	for (const FString& AbsFile : InCommand.Files)
	{
		const int32 BufIndex = PathBuffers.Emplace(AbsFile);
		PathViews.Add(PathBuffers[BufIndex].View());
		AbsFiles.Add(AbsFile);
	}

	// [diag] What the panel asked to commit (the staged set it passed). If the resulting
	// revision contains more than these, the over-inclusion is at stage- or commit-time.
	UE_LOGF(LogSourceControl, Verbose,"ArcLore: [CheckIn/diag] panel passed %d file(s) to commit:", InCommand.Files.Num());
	for (const FString& DiagFile : InCommand.Files)
	{
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [CheckIn/diag]   to-commit -> %ls", *DiagFile);
	}

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// ---- 0) RESET STAGING TO THE INTENDED SET ---------------------------------
	// Root cause of the over-inclusion bug: persistent dirty/staged pollution from
	// past scan=1 ops meant the staged set at commit time held far more than the
	// panel's intended files, and lore_revision_commit commits the staged set. So
	// before staging+committing, query what lore currently considers STAGED and
	// UNSTAGE every staged file that is NOT in InCommand.Files. unstage keeps the
	// file changes on disk (they stay dirty), it just drops them from this commit.
	// The intended files are left untouched (the STAGE step below re-asserts them).
	{
		// 0a) Query the currently-staged set (read-only: scan=0).
		lore_repository_status_args_t ResetStatusArgs;
		FMemory::Memzero(&ResetStatusArgs, sizeof(ResetStatusArgs));
		ResetStatusArgs.staged = 1;
		ResetStatusArgs.scan = 0;

		TArray<FString> CurrentlyStaged;
		const ArcLoreApi::FLoreOpInvoker ResetStatusInvoker =
			[&GlobalArgs, &ResetStatusArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_status(&GlobalArgs.Args(), &ResetStatusArgs, Config);
			};
		const ArcLoreApi::FLoreEventCollector ResetStatusCollector =
			[&CurrentlyStaged, &RepoRoot](const lore_event_t& Event)
			{
				if (lore_event_type(&Event) != LORE_EVENT_REPOSITORY_STATUS_FILE)
				{
					return;
				}
				const lore_repository_status_file_event_data_t& F = Event.repository_status_file;
				if (F.flag_staged != 0 && F.type == LORE_NODE_TYPE_FILE)
				{
					const FString RelPath = ArcLoreApi::LoreStringToFString(F.path);
					CurrentlyStaged.Add(FPaths::ConvertRelativePathToFull(RepoRoot, RelPath));
				}
			};
		FString ResetStatusError;
		const int32 ResetStatusResult = ArcLoreApi::RunLoreOp(ResetStatusInvoker, ResetStatusCollector, ResetStatusError, TEXT("CheckIn/reset-staged-query"));
		if (ResetStatusResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_repository_status (staged) failed (nothing committed): %s"), *ResetStatusError));
			InCommand.bCommandSuccessful = false;
			return false;
		}

		// 0b) The extra staged files = currently-staged minus the intended set.
		// Compare absolute paths case-insensitively (FPaths::IsSamePath).
		TArray<FString> ExtraStaged;
		ExtraStaged.Reserve(CurrentlyStaged.Num());
		for (const FString& StagedAbs : CurrentlyStaged)
		{
			bool bIsIntended = false;
			for (const FString& IntendedAbs : AbsFiles)
			{
				if (FPaths::IsSamePath(StagedAbs, IntendedAbs))
				{
					bIsIntended = true;
					break;
				}
			}
			if (!bIsIntended)
			{
				ExtraStaged.Add(StagedAbs);
			}
		}

		// 0c) Unstage the extras (if any). unstage KEEPS the on-disk changes; it only
		// removes them from the staged changeset this commit will record. Build the
		// UTF-8 buffers before the invoker so the lore_string_t views stay valid.
		if (ExtraStaged.Num() > 0)
		{
			TArray<ArcLoreApi::FLoreUtf8String> UnstageBuffers;
			UnstageBuffers.Reserve(ExtraStaged.Num());
			TArray<lore_string_t> UnstageViews;
			UnstageViews.Reserve(ExtraStaged.Num());
			for (const FString& ExtraAbs : ExtraStaged)
			{
				const int32 BufIndex = UnstageBuffers.Emplace(ExtraAbs);
				UnstageViews.Add(UnstageBuffers[BufIndex].View());
			}

			lore_file_unstage_args_t ResetUnstageArgs;
			FMemory::Memzero(&ResetUnstageArgs, sizeof(ResetUnstageArgs));
			ResetUnstageArgs.paths.ptr   = UnstageViews.GetData();
			ResetUnstageArgs.paths.count = static_cast<uintptr_t>(UnstageViews.Num());

			const ArcLoreApi::FLoreOpInvoker ResetUnstageInvoker =
				[&GlobalArgs, &ResetUnstageArgs](const lore_event_callback_config_t& Config) -> int32
				{
					return lore_file_unstage(&GlobalArgs.Args(), &ResetUnstageArgs, Config);
				};
			const ArcLoreApi::FLoreEventCollector ResetUnstageCollector =
				[](const lore_event_t& /*Event*/)
				{
				};

			FString ResetUnstageError;
			const int32 ResetUnstageResult = ArcLoreApi::RunLoreOp(ResetUnstageInvoker, ResetUnstageCollector, ResetUnstageError, TEXT("CheckIn/reset-unstage"));
			if (ResetUnstageResult != 0)
			{
				// Fatal: committing after a partial reset risks the same over-inclusion.
				InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_file_unstage (reset extras) failed (nothing committed): %s"), *ResetUnstageError));
				InCommand.bCommandSuccessful = false;
				return false;
			}
		}
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [CheckIn/diag] unstaged %d extra file(s) before commit", ExtraStaged.Num());
	}

	// ---- 1) STAGE op -----------------------------------------------------------
	{
		lore_file_stage_args_t StageArgs;
		FMemory::Memzero(&StageArgs, sizeof(StageArgs));
		StageArgs.paths.ptr   = PathViews.GetData();
		StageArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());
		// scan=0: InCommand.Files are FILE paths, so the directory-recursive scan does
		// nothing useful here and risks re-dirtying the tree. Stage exactly the intended
		// files; the reset step above already dropped everything else from the changeset.
		StageArgs.scan        = 0;
		StageArgs.case_change = 0;

		const ArcLoreApi::FLoreOpInvoker StageInvoker =
			[&GlobalArgs, &StageArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_file_stage(&GlobalArgs.Args(), &StageArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StageCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString StageError;
		const int32 StageResult = ArcLoreApi::RunLoreOp(StageInvoker, StageCollector, StageError, TEXT("CheckIn/stage"));
		if (StageResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_file_stage failed (nothing committed): %s"), *StageError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// [diag] What does Lore consider STAGED right before commit? lore_revision_commit
	// commits the staged set; if this logs more than the panel passed above, staging is
	// over-including (e.g. persistent dirty-flag pollution). scan=0 so it only reads.
	{
		lore_repository_status_args_t DiagArgs;
		FMemory::Memzero(&DiagArgs, sizeof(DiagArgs));
		DiagArgs.staged = 1;
		DiagArgs.scan = 0;

		int32 DiagStagedCount = 0;
		const ArcLoreApi::FLoreOpInvoker DiagInvoker =
			[&GlobalArgs, &DiagArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_status(&GlobalArgs.Args(), &DiagArgs, Config);
			};
		const ArcLoreApi::FLoreEventCollector DiagCollector =
			[&DiagStagedCount](const lore_event_t& Event)
			{
				if (lore_event_type(&Event) == LORE_EVENT_REPOSITORY_STATUS_FILE)
				{
					const lore_repository_status_file_event_data_t& F = Event.repository_status_file;
					if (F.flag_staged != 0)
					{
						++DiagStagedCount;
						UE_LOGF(LogSourceControl, Verbose,"ArcLore: [CheckIn/diag]   lore-staged -> %ls (type=%d action=%d dirty=%u)",
							*ArcLoreApi::LoreStringToFString(F.path), (int32)F.type, (int32)F.action, F.flag_dirty);
					}
				}
			};
		FString DiagError;
		ArcLoreApi::RunLoreOp(DiagInvoker, DiagCollector, DiagError, TEXT("CheckIn/diag-staged"));
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [CheckIn/diag] Lore reports %d staged file(s) at commit time", DiagStagedCount);
	}

	// ---- 2) COMMIT op (message = FCheckIn::GetDescription()) --------------------
	FString CommittedRevisionHex;
	{
		// The commit message string must outlive the lore_revision_commit call.
		ArcLoreApi::FLoreUtf8String MessageBuffer(Operation->GetDescription().ToString());

		lore_revision_commit_args_t CommitArgs;
		FMemory::Memzero(&CommitArgs, sizeof(CommitArgs));
		CommitArgs.link = ArcLoreApi::EmptyLoreString();
		CommitArgs.layer = ArcLoreApi::EmptyLoreString();
		CommitArgs.message = MessageBuffer.View();
		// link / layer fields left empty => commit the default (root) changeset.

		int32 FragmentWriteCount = 0;
		const ArcLoreApi::FLoreOpInvoker CommitInvoker =
			[&GlobalArgs, &CommitArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_revision_commit(&GlobalArgs.Args(), &CommitArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector CommitCollector =
			[&CommittedRevisionHex, &FragmentWriteCount](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REVISION_COMMIT_REVISION)
				{
					CommittedRevisionHex = ArcLoreCheckInInternal::HashToHexPrefix(Event.revision_commit_revision.revision);
				}
				else if (Tag == LORE_EVENT_REVISION_COMMIT_END)
				{
					// [diag] The decisive number: how many files this commit actually
					// processed. A 1-file panel commit reporting file_total in the
					// thousands proves commit committed far more than was staged.
					const lore_revision_commit_count_data_t& C = Event.revision_commit_end.count;
					UE_LOGF(LogSourceControl, Verbose,"ArcLore: [CheckIn/diag] COMMIT_END file_total=%llu file_modify=%llu file_delete=%llu dir_total=%llu",
						(unsigned long long)C.file_total, (unsigned long long)C.file_modify_count,
						(unsigned long long)C.file_delete_count, (unsigned long long)C.directory_total);
				}
				else if (Tag == LORE_EVENT_FRAGMENT_WRITE)
				{
					++FragmentWriteCount;
				}
			};

		FString CommitError;
		const int32 CommitResult = ArcLoreApi::RunLoreOp(CommitInvoker, CommitCollector, CommitError, TEXT("CheckIn/commit"));
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [CheckIn/diag] commit wrote %d fragment(s)", FragmentWriteCount);
		if (CommitResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_revision_commit failed (nothing committed): %s"), *CommitError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// Commit succeeded — the check-in is durable LOCALLY (not pushed). Locks are
	// RETAINED: push and lock-release are the separate Push op's responsibility.
	bCheckInSucceeded = true;
	CommittedFiles = AbsFiles;

	// ---- 3) RE-QUERY ahead/behind (revision_only=1, cheap) ---------------------
	// The new local commit should immediately show as "to push" in the panel, so
	// re-read is_remote_ahead/is_local_ahead and stamp them in UpdateStates. Mirror
	// of FArcLorePushWorker's post-op re-query.
	{
		lore_repository_status_args_t StatusArgs;
		FMemory::Memzero(&StatusArgs, sizeof(StatusArgs));
		// revision_only=1 skips file diffs and the filesystem scan — we only want the
		// status_revision event's ahead/behind flags after the commit.
		StatusArgs.revision_only = 1;

		const ArcLoreApi::FLoreOpInvoker StatusInvoker =
			[&GlobalArgs, &StatusArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_status(&GlobalArgs.Args(), &StatusArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StatusCollector =
			[this](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REPOSITORY_STATUS_REVISION)
				{
					const lore_repository_status_revision_event_data_t& RevEvent = Event.repository_status_revision;
					bIsNewerOnRemote = (RevEvent.is_remote_ahead != 0);
					bIsLocalAhead = (RevEvent.is_local_ahead != 0);
				}
			};

		FString StatusError;
		const int32 StatusResult = ArcLoreApi::RunLoreOp(StatusInvoker, StatusCollector, StatusError, TEXT("CheckIn/status"));
		if (StatusResult != 0)
		{
			// Soft: the commit is durable. A failed re-query just leaves the cached
			// ahead/behind stale; record a warning but the commit still succeeded.
			InCommand.InfoMessages.Add(FString::Printf(TEXT("Ahead/behind re-query after commit failed (commit is durable): %s"), *StatusError));
		}
	}

	// ---- Success message (LOCAL-commit semantics) ------------------------------
	if (CommittedRevisionHex.IsEmpty())
	{
		Operation->SetSuccessMessage(LOCTEXT("LoreCommitMessageUnknown", "Committed locally — use Push to publish."));
	}
	else
	{
		Operation->SetSuccessMessage(FText::Format(LOCTEXT("LoreCommitMessage", "Committed revision {0} locally — use Push to publish."), FText::FromString(CommittedRevisionHex)));
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreCheckInWorker::UpdateStates() const
{
	// On commit failure nothing succeeded: leave states untouched (still locked/modified).
	if (!bCheckInSucceeded)
	{
		return false;
	}

	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	// Apply the re-queried ahead/behind so the new local commit shows as "to push".
	// NumUnpushed stays unset (Step-1 decision: ahead/behind are divergent-DAG flags,
	// not a shared-lineage unpushed-commit count).
	Provider.SetCachedRemoteState(bIsNewerOnRemote, bIsLocalAhead, TOptional<int>());

	const FDateTime Now = FDateTime::Now();
	for (const FString& AbsFile : CommittedFiles)
	{
		// Committed files are clean locally (their changes are in the new local
		// revision); they are NOT pushed. Locks are RETAINED — leave lock fields as-is
		// (still locked-by-me). Lock release happens only in the Push op.
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(AbsFile);
		CachedState->WorkingCopyState = EArcLoreState::Unchanged;
		// Committed: the change left the staged changeset.
		CachedState->bStaged = false;
		CachedState->TimeStamp = Now;
	}

	// Return true so OnSourceControlStateChanged broadcasts (states + ahead/behind changed).
	return true;
}

FName FArcLoreMarkForAddWorker::GetName() const
{
	return "MarkForAdd";
}

// ----------------------------------------------------------------------------
// MarkForAdd — dirty + stage (lore_file_dirty then lore_file_stage).
// Both ops use the same repo-relative path list built once from InCommand.Files.
// ----------------------------------------------------------------------------
namespace ArcLoreMarkForAddInternal
{
	// Null collector: MarkForAdd emits no domain events we need to capture.
	void NullCollect(const lore_event_t& /*Event*/)
	{
	}
}

bool FArcLoreMarkForAddWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Build the path list for the lore file-mutation ops. Unlike status/file_info
	// (which match a repo-relative string against the tracked index), lore_file_stage
	// resolves a RELATIVE path against the process working dir (…/Engine/Binaries/Win64)
	// — sending "ArcGame/Plugins/…" produced "Invalid path Engine/Binaries/Win64/ArcGame/…".
	// So feed stage/dirty the ABSOLUTE path; lore derives the repo-relative form from
	// repository_path itself.
	TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
	PathBuffers.Reserve(InCommand.Files.Num());
	TArray<lore_string_t> PathViews;
	PathViews.Reserve(InCommand.Files.Num());
	for (const FString& AbsFile : InCommand.Files)
	{
		const int32 BufIndex = PathBuffers.Emplace(AbsFile);
		PathViews.Add(PathBuffers[BufIndex].View());
		States.Add(FArcLoreSourceControlState(AbsFile));
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [MarkForAdd] sending path to lore: '%ls'", *AbsFile);
	}

	// GUARD: an empty path list makes lore_file_dirty / lore_file_stage treat paths
	// as "all" (empty == every file), so a no-file MarkForAdd would dirty + stage
	// --scan the ENTIRE repo — the ~190k whole-repo "mark for add" footgun. No-op out.
	if (PathViews.Num() == 0)
	{
		UE_LOGF(LogSourceControl, Warning,"ArcLore: [MarkForAdd] empty file list — skipping (refusing to scan/stage the whole repo).");
		InCommand.bCommandSuccessful = true;
		return true;
	}

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// ---- DIRTY op --------------------------------------------------------------
	{
		lore_file_dirty_args_t DirtyArgs;
		FMemory::Memzero(&DirtyArgs, sizeof(DirtyArgs));
		DirtyArgs.paths.ptr   = PathViews.GetData();
		DirtyArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());

		const ArcLoreApi::FLoreOpInvoker DirtyInvoker =
			[&GlobalArgs, &DirtyArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_file_dirty(&GlobalArgs.Args(), &DirtyArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector DirtyCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString DirtyError;
		const int32 DirtyResult = ArcLoreApi::RunLoreOp(DirtyInvoker, DirtyCollector, DirtyError, TEXT("MarkForAdd/dirty"));
		if (DirtyResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_file_dirty failed: %s"), *DirtyError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// ---- STAGE op --------------------------------------------------------------
	{
		lore_file_stage_args_t StageArgs;
		FMemory::Memzero(&StageArgs, sizeof(StageArgs));
		StageArgs.paths.ptr   = PathViews.GetData();
		StageArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());
		// scan=1: reconcile against the filesystem so the content read+stage actually
		// persists for a MODIFIED tracked file (scan=0 left the stage non-persistent).
		// Mirrors the CheckIn worker's stage step.
		StageArgs.scan        = 1;
		StageArgs.case_change = 0;

		const ArcLoreApi::FLoreOpInvoker StageInvoker =
			[&GlobalArgs, &StageArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_file_stage(&GlobalArgs.Args(), &StageArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StageCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString StageError;
		const int32 StageResult = ArcLoreApi::RunLoreOp(StageInvoker, StageCollector, StageError, TEXT("MarkForAdd/stage"));
		if (StageResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_file_stage failed: %s"), *StageError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreMarkForAddWorker::UpdateStates() const
{
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	bool bUpdated = false;
	for (const FArcLoreSourceControlState& StateItem : States)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(StateItem.LocalFilename);
		// "Staged" is a first-class bit now — set it instead of overloading Added. Leave
		// WorkingCopyState alone for a tracked file (a staged MODIFY keeps Modified); the
		// follow-up Refresh sets the accurate enum from lore. Only promote to Added when the
		// entry is a genuine new add (was NotControlled/Unknown), never clobber a real state.
		if (CachedState->WorkingCopyState == EArcLoreState::NotControlled ||
			CachedState->WorkingCopyState == EArcLoreState::Unknown)
		{
			CachedState->WorkingCopyState = EArcLoreState::Added;
		}
		CachedState->bStaged = true;
		CachedState->TimeStamp = FDateTime::Now();
		bUpdated = true;
	}
	return bUpdated;
}

FName FArcLoreCopyWorker::GetName() const
{
	return "Copy";
}

// ----------------------------------------------------------------------------
// Copy — handle the engine FCopy (rename flow) as a Lore dirty_move so the
// asset's commit history follows the new path. The destination is reachable
// ONLY by casting InCommand.Operation -> FCopy and reading GetDestination();
// it is a full package filename (USourceControlHelpers::PackageFilename ==
// FPaths::ConvertRelativePathToFull) but we re-normalize it to match the source's
// form exactly. Like stage/dirty, dirty_move resolves a RELATIVE path against the
// process working dir, so both endpoints are sent ABSOLUTE; lore derives the
// repo-relative form from repository_path.
// ----------------------------------------------------------------------------
bool FArcLoreCopyWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	if (InCommand.Files.Num() == 0 || InCommand.Files[0].IsEmpty())
	{
		InCommand.ErrorMessages.Add(TEXT("Copy: no source file supplied."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Source: the renamed-from asset, already resolved to absolute by the provider.
	SourceAbs = InCommand.Files[0];

	// Destination: NOT in InCommand.Files — read it off the FCopy operation and
	// resolve to absolute exactly like the source (idempotent on an already-full path).
	const TSharedRef<FCopy, ESPMode::ThreadSafe> CopyOp = StaticCastSharedRef<FCopy>(InCommand.Operation);
	DestAbs = FPaths::ConvertRelativePathToFull(CopyOp->GetDestination());

	UE_LOGF(LogSourceControl, Verbose,"ArcLore: [movediag] Copy source (abs): '%ls'", *SourceAbs);
	UE_LOGF(LogSourceControl, Verbose,"ArcLore: [movediag] Copy dest   (abs): '%ls'", *DestAbs);

	// Only CopyMethod::Branch is driven by the editor's rename flow. CopyMethod::Add
	// is duplication (no history relationship) and is NOT exercised by rename — make it
	// a defensive no-op (success, not error) so a future Add caller doesn't break.
	if (CopyOp->CopyMethod != FCopy::ECopyMethod::Branch)
	{
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [movediag] Copy(Add) not supported as move; skipping (no-op).");
		InCommand.bCommandSuccessful = true;
		return true;
	}

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	ArcLoreApi::FLoreUtf8String FromBuffer(SourceAbs);
	ArcLoreApi::FLoreUtf8String ToBuffer(DestAbs);

	lore_file_dirty_move_args_t MoveArgs;
	FMemory::Memzero(&MoveArgs, sizeof(MoveArgs));
	MoveArgs.from_path = FromBuffer.View();
	MoveArgs.to_path   = ToBuffer.View();

	const ArcLoreApi::FLoreOpInvoker MoveInvoker =
		[&GlobalArgs, &MoveArgs](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_file_dirty_move(&GlobalArgs.Args(), &MoveArgs, Config);
		};

	const ArcLoreApi::FLoreEventCollector MoveCollector =
		[](const lore_event_t& /*Event*/)
		{
		};

	FString MoveError;
	const int32 MoveResult = ArcLoreApi::RunLoreOp(MoveInvoker, MoveCollector, MoveError, TEXT("Copy/dirty_move"));
	if (MoveResult != 0)
	{
		InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_file_dirty_move failed: %s"), *MoveError));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	bMoved = true;
	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreCopyWorker::UpdateStates() const
{
	// Nothing to stamp for the Add no-op / a failed move.
	if (!bMoved)
	{
		return false;
	}

	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	// Mark the destination as a staged pending change. The dedicated Renamed enum is
	// Step 2; for now Modified is the closest pending-change state. The move OWNS both
	// endpoints, so do NOT stamp the old path Deleted — the next status refresh reflects
	// the move (and the old path surfaces as a redirector add, which is expected).
	TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(DestAbs);
	CachedState->WorkingCopyState = EArcLoreState::Modified;
	CachedState->bStaged = true;
	CachedState->TimeStamp = FDateTime::Now();
	return true;
}

FName FArcLoreUnstageWorker::GetName() const
{
	return "Unstage";
}

// ----------------------------------------------------------------------------
// Unstage — remove files from the staged changeset (lore_file_unstage). Inverse
// of MarkForAdd's stage step. Like stage, unstage resolves a RELATIVE path
// against the process working dir (…/Engine/Binaries/Win64), so feed it the
// ABSOLUTE path; lore derives the repo-relative form from repository_path.
// ----------------------------------------------------------------------------
bool FArcLoreUnstageWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Build the ABSOLUTE path list for lore_file_unstage (see comment above).
	TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
	PathBuffers.Reserve(InCommand.Files.Num());
	TArray<lore_string_t> PathViews;
	PathViews.Reserve(InCommand.Files.Num());
	for (const FString& AbsFile : InCommand.Files)
	{
		const int32 BufIndex = PathBuffers.Emplace(AbsFile);
		PathViews.Add(PathBuffers[BufIndex].View());
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [Unstage] sending path to lore: '%ls'", *AbsFile);
	}

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	lore_file_unstage_args_t UnstageArgs;
	FMemory::Memzero(&UnstageArgs, sizeof(UnstageArgs));
	UnstageArgs.paths.ptr   = PathViews.GetData();
	UnstageArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());

	const ArcLoreApi::FLoreOpInvoker UnstageInvoker =
		[&GlobalArgs, &UnstageArgs](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_file_unstage(&GlobalArgs.Args(), &UnstageArgs, Config);
		};

	const ArcLoreApi::FLoreEventCollector UnstageCollector =
		[this, &RepoRoot](const lore_event_t& Event)
		{
			const uint32_t Tag = lore_event_type(&Event);
			if (Tag == LORE_EVENT_FILE_UNSTAGE_FILE)
			{
				const FString RelPath = ArcLoreApi::LoreStringToFString(Event.file_unstage_file.path);
				FUnstagedFile Record;
				Record.AbsPath = ArcLorePathHelpers::ToAbsolute(RelPath, RepoRoot);
				Record.bWasAdd = (Event.file_unstage_file.action == LORE_FILE_ACTION_ADD);
				UnstagedFiles.Add(MoveTemp(Record));
			}
		};

	FString UnstageError;
	const int32 UnstageResult = ArcLoreApi::RunLoreOp(UnstageInvoker, UnstageCollector, UnstageError, TEXT("Unstage"));
	if (UnstageResult != 0)
	{
		InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_file_unstage failed: %s"), *UnstageError));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Fallback: if lore emitted no per-file events, treat every requested path as a
	// former staged-add (the common case for the panel's Unstage action). UpdateStates
	// then marks them NotControlled, and the panel's follow-up Refresh reconciles.
	if (UnstagedFiles.Num() == 0)
	{
		for (const FString& AbsFile : InCommand.Files)
		{
			FUnstagedFile Record;
			Record.AbsPath = AbsFile;
			Record.bWasAdd = true;
			UnstagedFiles.Add(MoveTemp(Record));
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreUnstageWorker::UpdateStates() const
{
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	bool bUpdated = false;
	const FDateTime Now = FDateTime::Now();
	for (const FUnstagedFile& Record : UnstagedFiles)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(Record.AbsPath);
		// Inverse of MarkForAdd (which stamps Added): a former staged-add reverts to
		// NotControlled (untracked again); a former staged-modify reverts to Modified
		// (still tracked, still has local edits — just no longer staged).
		CachedState->WorkingCopyState = Record.bWasAdd ? EArcLoreState::NotControlled : EArcLoreState::Modified;
		// No longer in the staged changeset.
		CachedState->bStaged = false;
		CachedState->TimeStamp = Now;
		bUpdated = true;
	}
	return bUpdated;
}

FName FArcLoreDeleteWorker::GetName() const
{
	return "Delete";
}

// ----------------------------------------------------------------------------
// Delete — delete file on disk, then dirty + stage the (now-removed) path so
// Lore records the deletion in the staged changeset.
// lore_file_stage of a missing path records the deletion (file_delete action).
// ----------------------------------------------------------------------------
namespace ArcLoreDeleteInternal
{
	// Null collector: Delete emits no domain events we need to capture.
	void NullCollect(const lore_event_t& /*Event*/)
	{
	}
}

bool FArcLoreDeleteWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Build the path list for lore file-mutation ops. Unlike status/file_info
	// (which match a repo-relative string against the tracked index), lore_file_dirty
	// and lore_file_stage resolve a RELATIVE path against the process working dir
	// (…/Engine/Binaries/Win64) — sending "ArcGame/Plugins/…" produced
	// "Invalid path Engine/Binaries/Win64/ArcGame/…".
	// So feed dirty/stage the ABSOLUTE path; lore derives the repo-relative form from
	// repository_path itself.
	TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
	PathBuffers.Reserve(InCommand.Files.Num());
	TArray<lore_string_t> PathViews;
	PathViews.Reserve(InCommand.Files.Num());
	for (const FString& AbsFile : InCommand.Files)
	{
		const int32 BufIndex = PathBuffers.Emplace(AbsFile);
		PathViews.Add(PathBuffers[BufIndex].View());
		States.Add(FArcLoreSourceControlState(AbsFile));
	}

	// ---- Delete from disk (the provider is responsible for removing the file) --
	// The editor has already closed the asset; we unlink the file on disk.
	for (const FString& AbsFile : InCommand.Files)
	{
		if (FPaths::FileExists(AbsFile))
		{
			const bool bDeleted = IFileManager::Get().Delete(*AbsFile, /*RequireExists=*/false, /*EvenReadOnly=*/true);
			if (!bDeleted)
			{
				InCommand.ErrorMessages.Add(FString::Printf(TEXT("Failed to delete file from disk: %s"), *AbsFile));
				InCommand.bCommandSuccessful = false;
				return false;
			}
		}
	}

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// ---- DIRTY op (marks the removal visible to the staged state) --------------
	{
		lore_file_dirty_args_t DirtyArgs;
		FMemory::Memzero(&DirtyArgs, sizeof(DirtyArgs));
		DirtyArgs.paths.ptr   = PathViews.GetData();
		DirtyArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());

		const ArcLoreApi::FLoreOpInvoker DirtyInvoker =
			[&GlobalArgs, &DirtyArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_file_dirty(&GlobalArgs.Args(), &DirtyArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector DirtyCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString DirtyError;
		const int32 DirtyResult = ArcLoreApi::RunLoreOp(DirtyInvoker, DirtyCollector, DirtyError);
		if (DirtyResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_file_dirty failed: %s"), *DirtyError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// ---- STAGE op (records deletion of the now-missing path) -------------------
	{
		lore_file_stage_args_t StageArgs;
		FMemory::Memzero(&StageArgs, sizeof(StageArgs));
		StageArgs.paths.ptr   = PathViews.GetData();
		StageArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());
		StageArgs.scan        = 0;
		StageArgs.case_change = 0;

		const ArcLoreApi::FLoreOpInvoker StageInvoker =
			[&GlobalArgs, &StageArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_file_stage(&GlobalArgs.Args(), &StageArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StageCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString StageError;
		const int32 StageResult = ArcLoreApi::RunLoreOp(StageInvoker, StageCollector, StageError);
		if (StageResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_file_stage (delete) failed: %s"), *StageError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreDeleteWorker::UpdateStates() const
{
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	bool bUpdated = false;
	for (const FArcLoreSourceControlState& StateItem : States)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(StateItem.LocalFilename);
		CachedState->WorkingCopyState = EArcLoreState::Deleted;
		CachedState->TimeStamp = FDateTime::Now();
		bUpdated = true;
	}
	return bUpdated;
}


FName FArcLoreRevertWorker::GetName() const
{
	return "Revert";
}

// ----------------------------------------------------------------------------
// Revert — restore files to the current revision (discard local changes).
// lore_file_reset with empty revision resets to the tracked revision, restoring
// the on-disk content. It handles both staged and unstaged changes in one call —
// no separate unstage step is needed.
// ----------------------------------------------------------------------------
namespace ArcLoreRevertInternal
{
	// Null collector: Revert emits no domain events we need to capture.
	void NullCollect(const lore_event_t& /*Event*/)
	{
	}
}

bool FArcLoreRevertWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Build the path list for lore file-mutation ops. Unlike status/file_info
	// (which match a repo-relative string against the tracked index), lore_file_reset
	// resolves a RELATIVE path against the process working dir (…/Engine/Binaries/Win64)
	// — sending "ArcGame/Plugins/…" produced "Invalid path Engine/Binaries/Win64/ArcGame/…".
	// So feed reset the ABSOLUTE path; lore derives the repo-relative form from
	// repository_path itself.
	TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
	PathBuffers.Reserve(InCommand.Files.Num());
	TArray<lore_string_t> PathViews;
	PathViews.Reserve(InCommand.Files.Num());
	for (const FString& AbsFile : InCommand.Files)
	{
		const int32 BufIndex = PathBuffers.Emplace(AbsFile);
		PathViews.Add(PathBuffers[BufIndex].View());
		States.Add(FArcLoreSourceControlState(AbsFile));
	}

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// ---- RESET op (restores on-disk content to the tracked revision) -----------
	// revision left zero/empty => reset to the current tracked revision.
	// purge=0 => do not remove untracked files.
	{
		lore_file_reset_args_t ResetArgs;
		FMemory::Memzero(&ResetArgs, sizeof(ResetArgs));
		ResetArgs.revision = ArcLoreApi::EmptyLoreString();
		ResetArgs.paths.ptr   = PathViews.GetData();
		ResetArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());
		ResetArgs.purge       = 0;

		const ArcLoreApi::FLoreOpInvoker ResetInvoker =
			[&GlobalArgs, &ResetArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_file_reset(&GlobalArgs.Args(), &ResetArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector ResetCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString ResetError;
		const int32 ResetResult = ArcLoreApi::RunLoreOp(ResetInvoker, ResetCollector, ResetError);
		if (ResetResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_file_reset failed: %s"), *ResetError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreRevertWorker::UpdateStates() const
{
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	bool bUpdated = false;
	for (const FArcLoreSourceControlState& StateItem : States)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(StateItem.LocalFilename);
		CachedState->WorkingCopyState = EArcLoreState::Unchanged;
		CachedState->TimeStamp = FDateTime::Now();
		bUpdated = true;
	}
	return bUpdated;
}

FName FArcLoreSyncWorker::GetName() const
{
	return "Sync";
}

// ----------------------------------------------------------------------------
// Sync — bring the working copy up to the branch tip via lore_revision_sync.
//
// args: empty revision => sync to branch tip ("latest"); reset=0 => do NOT
// clobber local modified files. A sync that produces a merge with conflicts is
// NOT a hard failure: the op returns 0, leaves the conflicted files staged for
// resolution, and Lore emits one branch_merge_conflict_file per such path.
//
// Collector (lore worker thread) copies into worker scratch only:
//   * revision_sync_file        -> TouchedFiles (rel->abs)
//   * branch_merge_conflict_file-> ConflictedFiles (rel->abs)
//   * revision_sync_revision    -> bFlagMerge / bFlagConflict
// UpdateStates (game thread) stamps the conflicted files Conflicted so the
// editor's IsConflicted()-driven resolution UI lights up.
// ----------------------------------------------------------------------------
bool FArcLoreSyncWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// ---- SYNC op ---------------------------------------------------------------
	{
		// Read the requested revision from the engine FSync operation.
		// When Rev is empty we sync to branch tip (current behavior).
		// When Rev is non-empty we sync to that specific revision (rewind).
		const FString Rev = StaticCastSharedRef<FSync>(InCommand.Operation)->GetRevision();

		// RevBuf MUST be declared BEFORE SyncInvoker so its lifetime covers the
		// lore_revision_sync call.  SyncArgs.revision.View() points into RevBuf.
		ArcLoreApi::FLoreUtf8String RevBuf(Rev);

		lore_revision_sync_args_t SyncArgs;
		FMemory::Memzero(&SyncArgs, sizeof(SyncArgs));
		SyncArgs.revision = ArcLoreApi::EmptyLoreString();

		if (!Rev.IsEmpty())
		{
			// Target a specific revision: set the revision string and ask Lore to
			// fast-forward while keeping local changes (forward_changes=1, reset=0).
			SyncArgs.revision        = RevBuf.View();
			SyncArgs.forward_changes = 1;
			SyncArgs.reset           = 0;
		}
		else
		{
			// No target revision — sync to branch tip.
			SyncArgs.forward_changes = 0;
			SyncArgs.reset           = 0; // keep local modified files; merge instead of clobber.
		}

		const ArcLoreApi::FLoreOpInvoker SyncInvoker =
			[&GlobalArgs, &SyncArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_revision_sync(&GlobalArgs.Args(), &SyncArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector SyncCollector =
			[this, &RepoRoot](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				switch (Tag)
				{
				case LORE_EVENT_REVISION_SYNC_FILE:
					{
						const FString RelPath = ArcLoreApi::LoreStringToFString(Event.revision_sync_file.path);
						TouchedFiles.Add(ArcLorePathHelpers::ToAbsolute(RelPath, RepoRoot));
						break;
					}
				case LORE_EVENT_BRANCH_MERGE_CONFLICT_FILE:
					{
						const FString RelPath = ArcLoreApi::LoreStringToFString(Event.branch_merge_conflict_file.path);
						ConflictedFiles.Add(ArcLorePathHelpers::ToAbsolute(RelPath, RepoRoot));
						break;
					}
				case LORE_EVENT_REVISION_SYNC_REVISION:
					{
						bFlagMerge    = (Event.revision_sync_revision.flag_merge != 0);
						bFlagConflict = (Event.revision_sync_revision.flag_conflict != 0);
						break;
					}
				default:
					break;
				}
			};

		FString SyncError;
		const int32 SyncResult = ArcLoreApi::RunLoreOp(SyncInvoker, SyncCollector, SyncError);

		// A sync that leaves conflicts still SUCCEEDS (op returned 0); only a
		// non-zero op result is a hard failure.
		InCommand.bCommandSuccessful = (SyncResult == 0);
		if (!InCommand.bCommandSuccessful)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_revision_sync failed: %s"), *SyncError));
			return false;
		}
	}

	// Surface a non-fatal info message when the sync staged a conflicting merge.
	if (bFlagConflict)
	{
		InCommand.InfoMessages.Add(FString::Printf(
			TEXT("Sync produced a merge with %d conflicted file(s); resolve them before committing."),
			ConflictedFiles.Num()));
	}

	return InCommand.bCommandSuccessful;
}

bool FArcLoreSyncWorker::UpdateStates() const
{
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	bool bUpdated = false;
	const FDateTime Now = FDateTime::Now();

	// Touched (cleanly-synced) files: optimistically mark Unchanged. The editor
	// follows up with an explicit UpdateStatus that reconciles against disk.
	for (const FString& AbsFile : TouchedFiles)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(AbsFile);
		CachedState->WorkingCopyState = EArcLoreState::Unchanged;
		CachedState->TimeStamp = Now;
		bUpdated = true;
	}

	// Conflicted files: stamp Conflicted — this is the load-bearing part. The
	// editor reads IsConflicted() to drive its resolution UI. Done after the
	// touched-file pass so a path that is both touched AND conflicted ends up
	// Conflicted.
	for (const FString& AbsFile : ConflictedFiles)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(AbsFile);
		CachedState->WorkingCopyState = EArcLoreState::Conflicted;
		CachedState->TimeStamp = Now;
		bUpdated = true;
	}

	return bUpdated;
}

FName FArcLoreUpdateStatusWorker::GetName() const
{
	return "UpdateStatus";
}

// ----------------------------------------------------------------------------
// UpdateStatus — the core read path. Three Lore ops, all driven through the
// Connect-worker template (FLoreGlobalArgsScope built once, each op an
// invoker+collector pair on RunLoreOp; collectors copy POD/strings into
// worker-owned scratch on the lore worker thread, never touching the provider).
//
//   1. lore_repository_status — working-copy state per file. Clean files emit
//      NO status_file event, so we seed every queried file Unchanged and
//      overwrite from any event that arrives. The status_revision event drives
//      bIsNewerOnRemote (is_remote_ahead).
//   2. lore_file_history (only when ShouldUpdateHistory) — per file, a
//      file_history event opens a revision record; the trailing METADATA events
//      (message/created-by/timestamp) belong to THAT record until the next
//      file_history. Soft: failures warn, do not fail the command.
//   3. lore_lock_file_query — branch-wide locks (owner id + locked_at per locked
//      path), auth-gated on the provider's resolved identity. Soft.
//
// Paths: UE hands ABSOLUTE filenames; Lore args want repo-relative forward-slash
// paths. We convert abs->rel for every Lore call and rel->abs (against the repo
// root) when keying scratch, so UpdateStates can look the StateCache up by the
// original absolute filename.
// ----------------------------------------------------------------------------
namespace ArcLoreUpdateStatusInternal
{
	// abs -> repo-relative, forward-slash normalized (what the Lore args want).
	FString ToRepoRelative(const FString& AbsFilename, const FString& RepoRoot)
	{
		FString Relative = FPaths::ConvertRelativePathToFull(AbsFilename);
		FString Root = RepoRoot;
		FPaths::NormalizeDirectoryName(Root);
		FPaths::MakePathRelativeTo(Relative, *(Root / TEXT("")));
		FPaths::NormalizeFilename(Relative); // forward slashes
		return Relative;
	}

	// repo-relative -> abs (the StateCache key, matching UE's InCommand.Files).
	FString ToAbsolute(const FString& RepoRelative, const FString& RepoRoot)
	{
		return FPaths::ConvertRelativePathToFull(RepoRoot, RepoRelative);
	}

	// Render a lore_hash_t as a 64-character lowercase hex string. Mirrors the
	// file-scoped HashToHex in the RevisionLog/ListUnpushed workers (and
	// ArcLoreRevisionHelpers::HashToHex used by InitFromHistory), so a hash here keys
	// identically to FArcLoreSourceControlRevision::RevisionHashHex.
	FString HashToHex(const lore_hash_t& Hash)
	{
		FString Result;
		Result.Reserve(64);
		for (int32 ByteIndex = 0; ByteIndex < 32; ++ByteIndex)
		{
			Result += FString::Printf(TEXT("%02x"), Hash.data[ByteIndex]);
		}
		return Result;
	}

	// True when every one of the hash's 32 bytes is zero. A zero revision_remote/local
	// means that tip is unknown (remote unreachable / branch not on remote) — the
	// out-of-date diff is skipped rather than diffing against a zero hash.
	bool IsZeroHash(const lore_hash_t& Hash)
	{
		for (int32 ByteIndex = 0; ByteIndex < 32; ++ByteIndex)
		{
			if (Hash.data[ByteIndex] != 0) { return false; }
		}
		return true;
	}

	// Per-revision commit metadata captured from lore_revision_history (the proven
	// source: lore_file_history carries none, and lore_revision_info(metadata=1) did
	// not surface it). Keyed by full revision hex; used to enrich file-history rows.
	struct FRevisionMeta
	{
		FString Description;
		FString UserName;
		uint64  EpochMillis = 0;
		bool    bHasDate = false;
	};

	// [diag] Log a single file-info event. Tracked files emit FILE_INFO; an
	// untracked path is expected to yield an ERROR (no FILE_INFO). The presence
	// or absence of a line per queried path is the tracked/untracked signal.
	void LogFileInfoEvent(const lore_event_t& Event)
	{
		if (lore_event_type(&Event) != LORE_EVENT_FILE_INFO)
		{
			return;
		}
		const lore_file_info_event_data_t& F = Event.file_info;
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [UpdateStatus/diag info] file: '%ls' is_file=%u is_dir=%u added=%u modified=%u deleted=%u conflict=%u repo_size=%llu local_size=%llu",
			*ArcLoreApi::LoreStringToFString(F.path), F.is_file, F.is_dir, F.flag_added,
			F.flag_modified, F.flag_deleted, F.flag_conflict,
			(unsigned long long)F.size, (unsigned long long)F.local_size);
	}

	// True when a lore_context_t is all-zero — the entry is NOT in the tracked index
	// (an untracked on-disk file). file_info DOES report such a file (with flag_added
	// and a zero context/hash), so presence of FILE_INFO is NOT itself "tracked".
	bool IsZeroContext(const lore_context_t& Context)
	{
		for (int32 ByteIndex = 0; ByteIndex < 16; ++ByteIndex)
		{
			if (Context.data[ByteIndex] != 0) { return false; }
		}
		return true;
	}

	// Map a FILE_INFO event to a working-copy state from its flags. Conflict wins, then
	// deleted/added/modified; otherwise a tracked file is clean (Unchanged). file_info
	// reports an UNTRACKED on-disk file as flag_added with a ZERO context — that is
	// NotControlled, not a staged Added (a real staged add is caught by the authoritative
	// repository_status(staged=1) pass and never reaches this fallback).
	EArcLoreState::Type DeriveFileInfoState(const lore_file_info_event_data_t& F)
	{
		if (F.flag_conflict != 0) { return EArcLoreState::Conflicted; }
		if (F.flag_deleted != 0)  { return EArcLoreState::Deleted; }
		if (F.flag_added != 0)
		{
			return IsZeroContext(F.context) ? EArcLoreState::NotControlled : EArcLoreState::Added;
		}
		if (F.flag_modified != 0) { return EArcLoreState::Modified; }
		return EArcLoreState::Unchanged;
	}

	// True for transient/derived trees we never want to source-control. Matched on
	// the full path with surrounding separators so we don't false-hit a folder named
	// e.g. "SavedGames" inside Content.
	bool IsTransientContentPath(const FString& FullForwardSlashPath)
	{
		return FullForwardSlashPath.Contains(TEXT("/Intermediate/"))
			|| FullForwardSlashPath.Contains(TEXT("/Saved/"))
			|| FullForwardSlashPath.Contains(TEXT("/Binaries/"))
			|| FullForwardSlashPath.Contains(TEXT("/DerivedDataCache/"));
	}

	// Collect every *.uasset / *.umap under a single content root, absolute +
	// forward-slash normalized, skipping transient subtrees. POOL THREAD only —
	// pure filesystem read, no provider/cache touch.
	void CollectContentAssetsUnderRoot(const FString& ContentDir, TArray<FString>& OutAbsPaths)
	{
		if (ContentDir.IsEmpty() || !FPaths::DirectoryExists(ContentDir))
		{
			return;
		}

		IFileManager& FileManager = IFileManager::Get();
		const TCHAR* const Extensions[] = { TEXT("*.uasset"), TEXT("*.umap") };
		for (const TCHAR* const Extension : Extensions)
		{
			TArray<FString> Found;
			FileManager.FindFilesRecursive(Found, *ContentDir, Extension, /*Files*/ true, /*Directories*/ false, /*bClearFileNames*/ false);
			for (const FString& Path : Found)
			{
				FString FullPath = FPaths::ConvertRelativePathToFull(Path);
				FPaths::NormalizeFilename(FullPath); // forward slashes for the Contains() skip test
				if (!IsTransientContentPath(FullPath))
				{
					OutAbsPaths.Add(MoveTemp(FullPath));
				}
			}
		}
	}
}

bool FArcLoreUpdateStatusWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FUpdateStatus>(InCommand.Operation);

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Repo-wide mode (the panel's whole-working-set enumeration): the caller passes
	// the single repo-root absolute path as a sentinel. In this mode there is no
	// caller-supplied file list — lore_repository_status enumerates every changed
	// file and the collector APPENDS a State per reported path (it cannot match a
	// pre-seeded index because none exist). Per-file mode keeps the match-only path.
	// NOTE: repo-wide status can stall ~5 min on a COLD connection (the reconnect
	// backoff documented near bQueryRepositoryStatus below). It is invoked ONLY on
	// an explicit, user-initiated panel Refresh (Step 7) on a warm connection —
	// never auto-polled from Tick.
	const bool bRepoWide = (InCommand.Files.Num() == 1
		&& FPaths::IsSamePath(InCommand.Files[0], RepoRoot));

	// Remember the mode for UpdateStates (game thread): in repo-wide mode the status pass
	// is authoritative over the WHOLE working set, so UpdateStates resets any cached state
	// not reported this pass back to Unchanged (drops committed/cleaned files from Dirty).
	bWasRepoWide = bRepoWide;

	// No files to query: nothing to do, but not an error. (The git path used the
	// empty-Files case to service FUpdateStatus::ShouldGetOpenedOnly by scanning
	// the whole project; that whole-repo discovery scan is deferred — the editor
	// drives UpdateStatus with explicit file lists for state refresh.)
	if (InCommand.Files.Num() == 0)
	{
		InCommand.bCommandSuccessful = true;
		return true;
	}

	// Build the queried repo-relative path list once; index-aligned with
	// InCommand.Files so we can map results back to absolute filenames.
	TArray<FString> RelativePaths;
	RelativePaths.Reserve(InCommand.Files.Num());
	for (const FString& AbsFile : InCommand.Files)
	{
		RelativePaths.Add(ArcLoreUpdateStatusInternal::ToRepoRelative(AbsFile, RepoRoot));
	}

	// Build the global args ONCE for the whole Execute (owns the UTF-8 buffers).
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// Seed States: every queried file starts Unchanged (clean + source-controlled).
	// lore_repository_status only emits status_file events for files WITH changes,
	// so unreported files must stay Unchanged, not Unknown. Map absolute filename
	// -> index in States for in-place overwrite from status events.
	//
	// Repo-wide mode SKIPS this seed: the single InCommand.Files entry is the repo
	// root (a sentinel), not a real file, so there is nothing to pre-seed. The
	// collector instead APPENDS one State per reported changed file. States starts
	// empty and AbsToStateIndex grows on the pool thread as events arrive.
	TMap<FString, int32> AbsToStateIndex;
	States.Reset();
	if (!bRepoWide)
	{
		States.Reserve(InCommand.Files.Num());
		AbsToStateIndex.Reserve(InCommand.Files.Num());
		for (int32 Index = 0; Index < InCommand.Files.Num(); ++Index)
		{
			FArcLoreSourceControlState SeedState(InCommand.Files[Index]);
			SeedState.WorkingCopyState = EArcLoreState::Unchanged;
			const int32 StateIndex = States.Add(MoveTemp(SeedState));
			AbsToStateIndex.Add(InCommand.Files[Index], StateIndex);
		}
	}

	// Indices the status op explicitly classified (staged/dirty/...). The file_info
	// fallback must NOT overwrite these — a staged Added would be clobbered to
	// NotControlled because file_info sees a staged-uncommitted file as untracked.
	TSet<int32> StatusReportedIndices;

	// ---- STATUS op (RE-ENABLED) ------------------------------------------------
	// lore_repository_status is the AUTHORITATIVE working-copy state source and a
	// superset of file_info: with scan=1 it reports STAGED adds (Added), on-disk
	// edits to tracked files (Modified via flag_dirty), deletes, and untracked paths
	// (NotControlled). file_info cannot see a staged-but-uncommitted file at all (it
	// is "untracked" in the index and emits no FILE_INFO), so with the op off such a
	// file classified NotControlled and CanCheckIn() was false — check-in impossible.
	// The earlier "hang" blamed on this op was the cold-connect transport stall
	// (~5 min of reconnect backoff); on a warm connection it returns in <1s.
	// TODO(Step 6): gate behind the warm-connection signal so it never runs cold.
	const bool bQueryRepositoryStatus = true;
	if (bQueryRepositoryStatus)
	{
		// lore_repository_status runs paths through convert_user_paths, which resolves
		// a RELATIVE path against the process CWD (…/Engine/Binaries/Win64) — the
		// editor's CWD is not the repo, so repo-relative paths resolve to nothing and
		// the op reports ZERO files. Feed ABSOLUTE paths; lore derives the repo-relative
		// form from repository_path. (Same CWD hazard as the file-mutation ops.)
		TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
		PathBuffers.Reserve(InCommand.Files.Num());
		TArray<lore_string_t> PathViews;
		PathViews.Reserve(InCommand.Files.Num());
		for (const FString& AbsFile : InCommand.Files)
		{
			const int32 BufIndex = PathBuffers.Emplace(AbsFile);
			PathViews.Add(PathBuffers[BufIndex].View());
		}

		lore_repository_status_args_t StatusArgs;
		FMemory::Memzero(&StatusArgs, sizeof(StatusArgs));
		// staged=1: include the STAGED state in the report. Without it the op reports
		// only filesystem-dirty/untracked files (staged=0) and NEVER a staged add
		// (flag_staged=1 -> Added), so a Mark-for-Add'd file goes invisible and cannot
		// be checked in. (The CLI `lore status` sets this too.)
		StatusArgs.staged = 1;
		// scan=0: READ existing dirty flags only — never walk the filesystem or PERSIST
		// new dirty flags. scan=1 marks every walked path dirty-added and persists it;
		// when Unreal enumerates the whole /Game content tree for status that permanently
		// dirties tens of thousands of untracked assets, so every later commit has to
		// reconcile ~all of them (observed: 60k dirty paths -> 73s commits). Staged adds
		// still come from staged=1; untracked->NotControlled comes from the file_info pass
		// below. (On-disk-edit detection for tracked files is deferred to a non-persisting
		// source — e.g. UPackage dirty — rather than a filesystem scan.)
		StatusArgs.scan = 0;
		StatusArgs.paths.ptr = PathViews.GetData();
		StatusArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());

		const ArcLoreApi::FLoreOpInvoker StatusInvoker =
			[&GlobalArgs, &StatusArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_status(&GlobalArgs.Args(), &StatusArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StatusCollector =
			[this, &RepoRoot, &AbsToStateIndex, &InCommand, &StatusReportedIndices, bRepoWide](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				switch (Tag)
				{
				case LORE_EVENT_REPOSITORY_STATUS_REVISION:
					{
						const lore_repository_status_revision_event_data_t& RevEvent = Event.repository_status_revision;
						// Remote ahead => this checkout has a newer revision upstream.
						bIsNewerOnRemote = (RevEvent.is_remote_ahead != 0);
						// Local ahead => local branch holds revisions not on the remote
						// history line (committed locally, not pushed).
						bIsLocalAhead = (RevEvent.is_local_ahead != 0);
						// Raw revision numbers — captured but NOT differenced (see header:
						// these are divergent-DAG flags, not a shared-lineage commit count).
						RevisionLocalNumber = RevEvent.revision_local_number;
						RevisionRemoteNumber = RevEvent.revision_remote_number;
						// Capture the local/remote tip hexes (empty when zero/unknown) so the
						// repo-wide path can run a per-asset out-of-date diff after the status
						// pass. Mirrors ListUnpushed's tip-hex capture (HashToHex + zero guard).
						bRemoteAhead = (RevEvent.is_remote_ahead != 0);
						LocalTipHex = ArcLoreUpdateStatusInternal::IsZeroHash(RevEvent.revision_local)
							? FString()
							: ArcLoreUpdateStatusInternal::HashToHex(RevEvent.revision_local);
						RemoteTipHex = ArcLoreUpdateStatusInternal::IsZeroHash(RevEvent.revision_remote)
							? FString()
							: ArcLoreUpdateStatusInternal::HashToHex(RevEvent.revision_remote);
						break;
					}
				case LORE_EVENT_REPOSITORY_STATUS_FILE:
					{
						const lore_repository_status_file_event_data_t& FileEvent = Event.repository_status_file;
						// Skip directory/link entries. A One-File-Per-Actor change drags in its
						// containing __ExternalActors__/<Map>/X/Y folders as separate dirty
						// directory entries; the user wants each OFPA package treated as ONE
						// unit (the actor .uasset only), not a chain of folder rows.
						if (FileEvent.type != LORE_NODE_TYPE_FILE)
						{
							break;
						}
						const FString RelPath = ArcLoreApi::LoreStringToFString(FileEvent.path);
						// Lore echoes the status path in any form; normalize to a full path,
						// then resolve it back to the matching queried editor key. Exact map
						// lookup first (fast path); IsSamePath fallback absorbs separator/
						// normalization differences so a real match is never missed.
						const FString Full = FPaths::ConvertRelativePathToFull(RepoRoot, RelPath);
						int32 MatchedIndex = INDEX_NONE;
						if (const int32* StateIndex = AbsToStateIndex.Find(Full))
						{
							MatchedIndex = *StateIndex;
						}
						else if (!bRepoWide)
						{
							// Per-file mode only: resolve the echoed path back to a queried
							// index via IsSamePath (separator/normalization tolerance).
							// Repo-wide mode never pre-seeds, so no match exists — instead
							// it appends below.
							for (int32 FileIndex = 0; FileIndex < InCommand.Files.Num(); ++FileIndex)
							{
								if (FPaths::IsSamePath(InCommand.Files[FileIndex], Full))
								{
									MatchedIndex = FileIndex;
									break;
								}
							}
						}
						const EArcLoreState::Type Derived = DeriveWorkingState(FileEvent);
						// A file staged as ADD whose .uasset no longer exists on disk is a stale staged entry
						// (marked-for-add, then deleted). scan=0 re-emits it from the staged index forever; by
						// refusing to record it we keep it OUT of States. A staged DELETE that is gone on disk
						// is a LEGITIMATE pending delete — never skip it.
						if (Derived == EArcLoreState::Added
							&& FileEvent.flag_staged != 0
							&& !FPaths::FileExists(Full))
						{
							UE_LOGF(LogSourceControl, Verbose,"ArcLore: [UpdateStatus/status] ghost staged-add skipped (gone on disk): '%ls'",
								*RelPath);
							break;
						}
						// Repo-wide mode: a STATUS_FILE for a path we have not seen yet is a
						// newly-discovered changed file — APPEND a State for it (pool thread,
						// worker-local arrays only) so UpdateStates writes it to the cache.
						// All pool-thread state mutation stays in States/AbsToStateIndex; the
						// provider is never touched here.
						if (bRepoWide && MatchedIndex == INDEX_NONE)
						{
							FArcLoreSourceControlState NewState(Full);
							NewState.WorkingCopyState = Derived;
							NewState.bStaged = (FileEvent.flag_staged != 0);
							MatchedIndex = States.Add(MoveTemp(NewState));
							AbsToStateIndex.Add(Full, MatchedIndex);
						}
						// [diag] Dump exactly how Lore classifies each reported file. action:
						// 0=KEEP 1=ADD 2=DELETE 3=MOVE 4=COPY. An untracked file is expected
						// here (header: status emits an event for "untracked status"); this
						// shows whether it arrives as ADD+staged=0 and what we derive from it.
						UE_LOGF(LogSourceControl, Verbose,"ArcLore: [UpdateStatus/status] file: '%ls' action=%d staged=%u dirty=%u conflict=%u merged=%u type=%d -> derived=%d matched=%d",
							*RelPath, (int32)FileEvent.action, FileEvent.flag_staged, FileEvent.flag_dirty,
							FileEvent.flag_conflict, FileEvent.flag_merged, (int32)FileEvent.type,
							(int32)Derived, States.IsValidIndex(MatchedIndex) ? 1 : 0);
						if (States.IsValidIndex(MatchedIndex))
						{
							States[MatchedIndex].WorkingCopyState = Derived;
							States[MatchedIndex].bStaged = (FileEvent.flag_staged != 0);
							StatusReportedIndices.Add(MatchedIndex);
						}
						break;
					}
				default:
					break;
				}
			};

		FString StatusError;
		const int32 StatusResult = ArcLoreApi::RunLoreOp(StatusInvoker, StatusCollector, StatusError, TEXT("UpdateStatus/status"));
		InCommand.bCommandSuccessful = (StatusResult == 0);
		if (!InCommand.bCommandSuccessful)
		{
			InCommand.ErrorMessages.Add(StatusError);
		}
	}
	else
	{
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [UpdateStatus/status] skipped (network op hangs; state derived from file_info)");
	}

	// ---- DISCOVERY (repo-wide only): find UNTRACKED content assets ---------------
	// The repo-wide status op above runs scan=0, so it reports only staged/dirty
	// files and never discovers brand-new untracked assets (a deliberate choice —
	// scan=1 would PERSIST a dirty-added flag on every walked path, re-polluting the
	// staged set, the exact failure we avoid). To surface untracked files as "?"
	// without persisting anything, we instead:
	//   1. walk the project + enabled-plugin Content dirs ON THIS POOL THREAD for
	//      every *.uasset / *.umap (skipping Intermediate/Saved/Binaries/DDC),
	//   2. drop candidates the status op already reported (dirty/staged), then
	//   3. batch a single lore_file_info over the remainder — a FAST local read that
	//      never persists. file_info emits FILE_INFO for a TRACKED path and NOTHING
	//      for an untracked one, so a candidate with NO FILE_INFO is UNTRACKED ->
	//      append a NotControlled State (CanAdd() true => the panel offers "Add").
	// This runs ONLY on an explicit, user-initiated repo-wide Refresh, async, on the
	// pool thread; all mutation stays in worker-local States/AbsToStateIndex (the
	// game-thread UpdateStates writes them to the cache). No Tick auto-poll.
	if (bRepoWide)
	{
		// 1. Build the bounded candidate list (pool thread, filesystem read only).
		TArray<FString> CandidateAbsPaths;
		ArcLoreUpdateStatusInternal::CollectContentAssetsUnderRoot(
			FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()), CandidateAbsPaths);

		const TArray<TSharedRef<IPlugin>> PluginsWithContent = IPluginManager::Get().GetEnabledPluginsWithContent();
		for (const TSharedRef<IPlugin>& Plugin : PluginsWithContent)
		{
			// Only PROJECT plugins (ArcGame/Plugins/**) are tracked in Lore. Engine plugins
			// (Engine/Plugins/**) carry GitDependencies content that .loreignore excludes — but
			// lore_file_info on explicit paths does NOT apply the ignore filter, so including
			// them here surfaced ~130k engine assets as bogus "uncontrolled". Skip them (also a
			// large perf win: no walking the engine content tree on every repo-wide refresh).
			if (Plugin->GetLoadedFrom() != EPluginLoadedFrom::Project)
			{
				continue;
			}
			ArcLoreUpdateStatusInternal::CollectContentAssetsUnderRoot(
				FPaths::ConvertRelativePathToFull(Plugin->GetContentDir()), CandidateAbsPaths);
		}

		// 2. Drop candidates the status op already classified (already in States via
		//    AbsToStateIndex), and de-dup. Match by exact key (paths are already full
		//    + forward-slash normalized). Build the repo-relative query list aligned
		//    with NewCandidateAbsPaths so we can map file_info results back.
		TArray<FString> NewCandidateAbsPaths;
		TArray<FString> NewCandidateRelPaths;
		NewCandidateAbsPaths.Reserve(CandidateAbsPaths.Num());
		NewCandidateRelPaths.Reserve(CandidateAbsPaths.Num());
		TSet<FString> SeenAbs;
		SeenAbs.Reserve(CandidateAbsPaths.Num());
		for (const FString& AbsPath : CandidateAbsPaths)
		{
			if (AbsToStateIndex.Contains(AbsPath) || SeenAbs.Contains(AbsPath))
			{
				continue;
			}
			SeenAbs.Add(AbsPath);
			NewCandidateAbsPaths.Add(AbsPath);
			NewCandidateRelPaths.Add(ArcLoreUpdateStatusInternal::ToRepoRelative(AbsPath, RepoRoot));
		}

		// 3. Batch lore_file_info over the candidates. file_info historically takes
		//    repo-relative paths in THIS worker (the per-file pass below feeds
		//    RelativePaths and matches results by the echoed file_info.path) — match
		//    that exactly. The collector records which RELATIVE paths got a FILE_INFO
		//    (tracked) so the silent ones can be classified untracked afterward.
		if (NewCandidateAbsPaths.Num() > 0)
		{
			TArray<ArcLoreApi::FLoreUtf8String> DiscoverBuffers;
			TArray<lore_string_t> DiscoverViews;
			// ABSOLUTE paths: lore_file_info resolves a RELATIVE path against the process
			// CWD (…/Engine/Binaries/Win64), not the repo, so repo-relative paths match
			// nothing and every candidate looks untracked. lore echoes the repo-relative
			// path on FILE_INFO, so TrackedRel (echoed-keyed) still matches NewCandidateRelPaths.
			DiscoverBuffers.Reserve(NewCandidateAbsPaths.Num());
			DiscoverViews.Reserve(NewCandidateAbsPaths.Num());
			for (const FString& AbsPath : NewCandidateAbsPaths)
			{
				const int32 BufIndex = DiscoverBuffers.Emplace(AbsPath);
				DiscoverViews.Add(DiscoverBuffers[BufIndex].View());
			}

			lore_file_info_args_t DiscoverArgs;
			FMemory::Memzero(&DiscoverArgs, sizeof(DiscoverArgs));
			DiscoverArgs.revision = ArcLoreApi::EmptyLoreString();
			DiscoverArgs.paths.ptr = DiscoverViews.GetData();
			DiscoverArgs.paths.count = static_cast<uintptr_t>(DiscoverViews.Num());
			// local=0: never compute on-disk hashes (the same hang hazard the per-file
			// pass documents below). We only need the tracked/untracked signal.
			DiscoverArgs.local = 0;

			// Tracked set: a candidate whose echoed file_info.path appears here is
			// TRACKED. PATH_IGNORE / FILTER_EXCLUDE => ignored (also "not untracked").
			TSet<FString> TrackedRel;
			TSet<FString> IgnoredRel;
			const ArcLoreApi::FLoreOpInvoker DiscoverInvoker =
				[&GlobalArgs, &DiscoverArgs](const lore_event_callback_config_t& Config) -> int32
				{
					return lore_file_info(&GlobalArgs.Args(), &DiscoverArgs, Config);
				};
			const ArcLoreApi::FLoreEventCollector DiscoverCollector =
				[&TrackedRel, &IgnoredRel](const lore_event_t& Event)
				{
					const uint32_t Tag = lore_event_type(&Event);
					if (Tag == LORE_EVENT_FILE_INFO)
					{
						// Untracked on-disk files ALSO emit FILE_INFO (flag_added + zero context); only a
							// non-zero context is in the tracked index. Zero-context => stays untracked below.
							if (!ArcLoreUpdateStatusInternal::IsZeroContext(Event.file_info.context)) { TrackedRel.Add(ArcLoreApi::LoreStringToFString(Event.file_info.path)); }
					}
					else if (Tag == LORE_EVENT_PATH_IGNORE)
					{
						IgnoredRel.Add(ArcLoreApi::LoreStringToFString(Event.path_ignore.path));
					}
					else if (Tag == LORE_EVENT_FILTER_EXCLUDE)
					{
						IgnoredRel.Add(ArcLoreApi::LoreStringToFString(Event.filter_exclude.path));
					}
				};

			FString DiscoverError;
			const int32 DiscoverResult = ArcLoreApi::RunLoreOp(DiscoverInvoker, DiscoverCollector, DiscoverError, TEXT("UpdateStatus/discover"));

			// 4. A candidate that is NEITHER tracked NOR ignored (file_info was silent)
			//    is UNTRACKED -> append a NotControlled State (pool thread, worker-local).
			int32 UntrackedCount = 0;
			for (int32 Index = 0; Index < NewCandidateAbsPaths.Num(); ++Index)
			{
				const FString& Rel = NewCandidateRelPaths[Index];
				if (TrackedRel.Contains(Rel) || IgnoredRel.Contains(Rel))
				{
					continue;
				}
				const FString& Abs = NewCandidateAbsPaths[Index];
				FArcLoreSourceControlState NewState(Abs);
				NewState.WorkingCopyState = EArcLoreState::NotControlled;
				NewState.bStaged = false;
				const int32 StateIndex = States.Add(MoveTemp(NewState));
				AbsToStateIndex.Add(Abs, StateIndex);
				++UntrackedCount;
			}

			UE_LOGF(LogSourceControl, Verbose,"ArcLore: [UpdateStatus/discover] candidates=%d tracked=%d ignored=%d untracked=%d (result=%d)",
				NewCandidateAbsPaths.Num(), TrackedRel.Num(), IgnoredRel.Num(), UntrackedCount, DiscoverResult);
		}
	}

	// ---- PER-ASSET OUT-OF-DATE SET (repo-wide only) ----------------------------
	// The branch-level is_remote_ahead flag is too coarse: when the local branch is
	// behind it marks EVERY statused file out-of-date. To get the EXACT files the
	// remote holds a newer revision of, diff the local tip against the remote tip with
	// source=local, target=remote (the INVERSE of ListUnpushed, which uses
	// source=remote, target=local for the unpushed set). Each LORE_EVENT_REVISION_DIFF_FILE
	// path is a repo-relative file the remote is ahead on; resolve it to ABSOLUTE and
	// add it to OutdatedFiles. UpdateStates stamps bIsNewerOnRemote per-file from this
	// set. Gated on bRemoteAhead + both tips known: a diverged branch with no shared
	// lineage still reports the differing files (correct: those ARE behind upstream).
	if (bRepoWide && bRemoteAhead && !RemoteTipHex.IsEmpty() && !LocalTipHex.IsEmpty())
	{
		// Both hex buffers MUST outlive the RunLoreOp call (their View()s are stored in
		// the args), so declare them BEFORE the invoker.
		ArcLoreApi::FLoreUtf8String SourceBuf(LocalTipHex);
		ArcLoreApi::FLoreUtf8String TargetBuf(RemoteTipHex);

		lore_revision_diff_args_t DiffArgs;
		FMemory::Memzero(&DiffArgs, sizeof(DiffArgs));
		DiffArgs.revision_source = SourceBuf.View();
		DiffArgs.revision_target = TargetBuf.View();
		// paths left empty => diff the whole tree.

		const ArcLoreApi::FLoreOpInvoker DiffInvoker =
			[&GlobalArgs, &DiffArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_revision_diff(&GlobalArgs.Args(), &DiffArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector DiffCollector =
			[this, &RepoRoot](const lore_event_t& Event)
			{
				if (lore_event_type(&Event) != LORE_EVENT_REVISION_DIFF_FILE)
				{
					return;
				}
				const lore_revision_diff_file_event_data_t& DiffFile = Event.revision_diff_file;
				const FString RelPath = ArcLoreApi::LoreStringToFString(DiffFile.path);
				if (RelPath.IsEmpty())
				{
					return;
				}
				const FString AbsPath = FPaths::ConvertRelativePathToFull(RepoRoot, RelPath);
				OutdatedFiles.Add(AbsPath);
			};

		FString DiffError;
		const int32 DiffResult = ArcLoreApi::RunLoreOp(DiffInvoker, DiffCollector, DiffError, TEXT("UpdateStatus/outdated"));
		if (DiffResult != 0)
		{
			// Soft: the badge is best-effort. A failed diff leaves OutdatedFiles empty
			// (no files marked out-of-date) and does NOT hard-fail the status command.
			InCommand.InfoMessages.Add(FString::Printf(TEXT("lore_revision_diff (out-of-date set) failed: %s"), *DiffError));
		}

		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [outdated/diag] remote-ahead out-of-date files=%d (local=%ls remote=%ls result=%d)",
			OutdatedFiles.Num(), *LocalTipHex, *RemoteTipHex, DiffResult);
	}

	// ---- STATE via lore_file_info (FAST, local: ~0.03s — the authoritative source) -
	// lore_file_info emits a FILE_INFO event for every TRACKED path (carrying the
	// add/modify/delete/conflict flags) and NONE for an untracked path. So:
	//   * FILE_INFO present -> derive working-copy state from its flags;
	//   * FILE_INFO absent   -> the path is untracked -> NotControlled (addable).
	// This replaces the (disabled, hanging) network status op as the state source.
	//
	// Repo-wide mode SKIPS this entire pass: RelativePaths holds only the repo-root
	// sentinel (not real files), so the file_info query would be meaningless and the
	// fallback loop below would clobber the status-derived States[] entries (it
	// writes by RelativePaths index, which in repo-wide mode does not align with the
	// appended States). The status op alone is the authoritative source repo-wide.
	TMap<FString, EArcLoreState::Type> InfoStateByRel;
	int32 InfoResult = 0;
	if (!bRepoWide)
	{
		TArray<ArcLoreApi::FLoreUtf8String> InfoBuffers;
		TArray<lore_string_t> InfoViews;
		// ABSOLUTE paths: lore_file_info resolves a RELATIVE path against the process
		// CWD (…/Engine/Binaries/Win64), not the repo, so repo-relative paths match
		// nothing and every tracked file is mis-classified untracked ("?"). InCommand.Files
		// is index-aligned with RelativePaths; lore echoes the repo-relative path on
		// FILE_INFO, so the InfoStateByRel lookup below stays keyed on RelativePaths[Index].
		InfoBuffers.Reserve(InCommand.Files.Num());
		InfoViews.Reserve(InCommand.Files.Num());
		for (const FString& AbsFile : InCommand.Files)
		{
			const int32 BufIndex = InfoBuffers.Emplace(AbsFile);
			InfoViews.Add(InfoBuffers[BufIndex].View());
		}

		lore_file_info_args_t InfoArgs;
		FMemory::Memzero(&InfoArgs, sizeof(InfoArgs));
		InfoArgs.revision = ArcLoreApi::EmptyLoreString();
		InfoArgs.paths.ptr = InfoViews.GetData();
		InfoArgs.paths.count = static_cast<uintptr_t>(InfoViews.Num());
		// local=0 deliberately: local=1 (compute the on-disk filtered hash/size) made
		// file_info block indefinitely on some files — it evidently triggers a remote
		// round-trip to fetch the comparison baseline, which hung the menu. We rely on
		// the tracked add/modify/delete/conflict flags instead; on-disk-dirty detection
		// of an unstaged edit to a tracked file is given up until this is safe to enable.
		InfoArgs.local = 0;

		const ArcLoreApi::FLoreOpInvoker InfoInvoker =
			[&GlobalArgs, &InfoArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_file_info(&GlobalArgs.Args(), &InfoArgs, Config);
			};
		const ArcLoreApi::FLoreEventCollector InfoCollector =
			[&InfoStateByRel](const lore_event_t& Event)
			{
				ArcLoreUpdateStatusInternal::LogFileInfoEvent(Event);
				if (lore_event_type(&Event) == LORE_EVENT_FILE_INFO)
				{
					InfoStateByRel.Add(
						ArcLoreApi::LoreStringToFString(Event.file_info.path),
						ArcLoreUpdateStatusInternal::DeriveFileInfoState(Event.file_info));
				}
			};

		FString InfoError;
		InfoResult = ArcLoreApi::RunLoreOp(InfoInvoker, InfoCollector, InfoError, TEXT("UpdateStatus/info"));
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [UpdateStatus/info] Lore tracks %d of %d queried path(s) (result=%d)", InfoStateByRel.Num(), RelativePaths.Num(), InfoResult);
	}

	// Fill in state for every file the status op did NOT classify. file_info emits a
	// FILE_INFO event for a TRACKED path (-> its flag-derived state) and NONE for an
	// untracked path (-> NotControlled, so "Mark for Add" is offered). Files the status
	// op already set (staged Added, dirty Modified, ...) are skipped so they are not
	// clobbered — file_info would mis-see a staged-uncommitted file as untracked.
	//
	// Repo-wide mode skips this: there is no caller file list to classify, and the
	// RelativePaths index does not align with the collector-appended States entries
	// (RelativePaths is just the repo-root sentinel). The status op already set every
	// entry. Running it here would overwrite a real status-derived State with the
	// repo root's bogus classification.
	if (!bRepoWide)
	{
		for (int32 Index = 0; Index < RelativePaths.Num(); ++Index)
		{
			if (!States.IsValidIndex(Index) || StatusReportedIndices.Contains(Index))
			{
				continue;
			}
			const EArcLoreState::Type* Found = InfoStateByRel.Find(RelativePaths[Index]);
			States[Index].WorkingCopyState = (Found != nullptr) ? *Found : EArcLoreState::NotControlled;
		}
	}

	// With the network status op disabled, file_info success is the command result.
	if (!bQueryRepositoryStatus)
	{
		InCommand.bCommandSuccessful = (InfoResult == 0);
	}

	// ---- HISTORY op (soft; only when requested) --------------------------------
	// Repo-wide mode never requests history (the panel does not), and RelativePaths
	// holds only the repo-root sentinel — so guard it out explicitly rather than
	// querying history for the repo root.
	if (!bRepoWide && Operation->ShouldUpdateHistory())
	{
		for (int32 Index = 0; Index < RelativePaths.Num(); ++Index)
		{
			const FString& RelPath = RelativePaths[Index];
			const FString& AbsPath = InCommand.Files[Index];

			// ABSOLUTE path: lore resolves a RELATIVE path against the process CWD
			// (…/Engine/Binaries/Win64 in the editor), NOT the repo — a repo-relative path
			// returns NO history in-editor (the CLI only works because its CWD is the repo).
			// Feed the absolute path; lore derives the repo-relative form from repository_path.
			// (Same CWD hazard the status/file_info/mutation ops document.)
			ArcLoreApi::FLoreUtf8String PathBuffer(AbsPath);

			lore_file_history_args_t HistoryArgs;
			FMemory::Memzero(&HistoryArgs, sizeof(HistoryArgs));
			HistoryArgs.revision = ArcLoreApi::EmptyLoreString();
			HistoryArgs.branch = ArcLoreApi::EmptyLoreString();
			HistoryArgs.path = PathBuffer.View();
			// Explicit revision-count bound (mirrors the RevisionLog worker's length=200).
			// Left at 0 the library uses an unspecified default; an explicit cap keeps the
			// native History window bounded and aligns with the per-revision metadata loop
			// below, which is capped to the same count.
			HistoryArgs.length = 200;

			// lore_file_history emits ONLY LORE_EVENT_FILE_HISTORY (no LORE_EVENT_METADATA;
			// lore.h:6316-6323), so the collector opens one revision record per entry
			// (hash/number/size/action via InitFromHistory). The message/author/date are
			// populated separately below via lore_revision_info(metadata=1), which DOES
			// emit LORE_EVENT_METADATA (lore.h:8642).
			TArcLoreSourceControlHistory History;

			const ArcLoreApi::FLoreOpInvoker HistoryInvoker =
				[&GlobalArgs, &HistoryArgs](const lore_event_callback_config_t& Config) -> int32
				{
					return lore_file_history(&GlobalArgs.Args(), &HistoryArgs, Config);
				};

			const ArcLoreApi::FLoreEventCollector HistoryCollector =
				[&History](const lore_event_t& Event)
				{
					const uint32_t Tag = lore_event_type(&Event);
					if (Tag == LORE_EVENT_FILE_HISTORY)
					{
						// Open a new revision record (core fields only; metadata follows).
						TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe> Revision =
							MakeShared<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>();
						Revision->InitFromHistory(Event.file_history);
						History.Add(Revision);
					}
				};

			FString HistoryError;
			const int32 HistoryResult = ArcLoreApi::RunLoreOp(HistoryInvoker, HistoryCollector, HistoryError, TEXT("UpdateStatus/history"));
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: [history/diag] '%ls' file_history entries=%d (result=%d)", *AbsPath, History.Num(), HistoryResult);
			if (HistoryResult != 0)
			{
				// Soft: history is best-effort. Record but do not fail the command.
				InCommand.InfoMessages.Add(FString::Printf(TEXT("Lore history unavailable for %s: %s"), *RelPath, *HistoryError));
			}

			// ---- PER-REVISION METADATA — via lore_revision_history (the PROVEN path) -
			// file_history carries no message/author/date, and lore_revision_info(metadata=1)
			// did not surface them. lore_revision_history reliably emits, per entry, the
			// METADATA (message/created-by/timestamp) that populates the Branch History tab
			// today (FArcLoreRevisionLogWorker). ONE bounded pass builds a hash->metadata map;
			// then enrich the file's revisions by hash. The file's revisions are branch
			// revisions, so they fall within the same length=200 window the panel uses.
			if (History.Num() > 0)
			{
				TMap<FString, ArcLoreUpdateStatusInternal::FRevisionMeta> MetaByHash;

				lore_revision_history_args_t MetaArgs;
				FMemory::Memzero(&MetaArgs, sizeof(MetaArgs));
				MetaArgs.revision = ArcLoreApi::EmptyLoreString();
				MetaArgs.branch = ArcLoreApi::EmptyLoreString();
				// branch / revision left empty => current branch from its tip.
				MetaArgs.length = 200;

				const ArcLoreApi::FLoreOpInvoker MetaInvoker =
					[&GlobalArgs, &MetaArgs](const lore_event_callback_config_t& Config) -> int32
					{
						return lore_revision_history(&GlobalArgs.Args(), &MetaArgs, Config);
					};

				// State machine (mirrors FArcLoreRevisionLogWorker): a REVISION_HISTORY_ENTRY
				// opens the "current" hash; trailing METADATA events attach to it.
				FString CurrentHash;
				const ArcLoreApi::FLoreEventCollector MetaCollector =
					[&MetaByHash, &CurrentHash](const lore_event_t& Event)
					{
						const uint32_t Tag = lore_event_type(&Event);
						if (Tag == LORE_EVENT_REVISION_HISTORY_ENTRY)
						{
							CurrentHash = ArcLoreUpdateStatusInternal::HashToHex(Event.revision_history_entry.revision);
							MetaByHash.FindOrAdd(CurrentHash);
							return;
						}
						if (Tag != LORE_EVENT_METADATA || CurrentHash.IsEmpty())
						{
							return;
						}
						ArcLoreUpdateStatusInternal::FRevisionMeta& Meta = MetaByHash.FindOrAdd(CurrentHash);
						const lore_metadata_event_data_t& M = Event.metadata;
						const FString Key = ArcLoreApi::LoreStringToFString(M.key);
						if (Key == TEXT("message") && M.value.tag == LORE_METADATA_TYPE_STRING)
						{
							Meta.Description = ArcLoreApi::LoreStringToFString(M.value.string);
						}
						else if (Key == TEXT("created-by") && M.value.tag == LORE_METADATA_TYPE_STRING)
						{
							Meta.UserName = ArcLoreApi::LoreStringToFString(M.value.string);
						}
						else if (Key == TEXT("timestamp") && M.value.tag == LORE_METADATA_TYPE_NUMERIC)
						{
							Meta.EpochMillis = M.value.numeric;
							Meta.bHasDate = true;
						}
					};

				FString MetaError;
				const int32 MetaResult = ArcLoreApi::RunLoreOp(MetaInvoker, MetaCollector, MetaError, TEXT("UpdateStatus/history-meta"));
				UE_LOGF(LogSourceControl, Verbose,"ArcLore: [history/diag] revision_history metadata map size=%d (result=%d)", MetaByHash.Num(), MetaResult);
				if (MetaResult != 0)
				{
					InCommand.InfoMessages.Add(FString::Printf(TEXT("Lore history metadata unavailable for %s: %s"), *RelPath, *MetaError));
				}

				for (int32 RevIndex = 0; RevIndex < History.Num(); ++RevIndex)
				{
					TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>& Revision = History[RevIndex];
					const ArcLoreUpdateStatusInternal::FRevisionMeta* Found = MetaByHash.Find(Revision->RevisionHashHex);
					if (Found == nullptr)
					{
						continue;
					}
					if (!Found->Description.IsEmpty())
					{
						Revision->SetDescription(Found->Description);
					}
					if (!Found->UserName.IsEmpty())
					{
						Revision->SetUserName(Found->UserName);
					}
					if (Found->bHasDate)
					{
						Revision->SetDateFromEpochMillis(Found->EpochMillis);
					}
				}
			}

			if (History.Num() > 0)
			{
				Histories.Add(AbsPath, MoveTemp(History));
			}
		}
	}

	// ---- LOCK QUERY op (soft; auth-gated) --------------------------------------
	// Single branch-wide lore_lock_file_query (owner/path filters empty => every lock
	// on the current branch, no 100-row limit). This replaces the old per-file
	// lore_lock_file_status pass, which was useless in repo-wide mode anyway:
	// InCommand.Files there is a single repo-root sentinel, so a per-file status pass
	// queries nothing real. One branch-wide query covers BOTH per-file and repo-wide
	// modes — the collector keys results by ABSOLUTE path, and UpdateStates matches
	// only the files it actually cares about.
	//
	// AUTH GATE: the lock service is an authenticated endpoint that fails with a
	// transport error (~2.1s guaranteed-fail latency) until a Lore identity is
	// resolved. Gate on Provider.IsAuthResolved() — a cheap bool read of the
	// auth-resolved flag the auth/Connect workers stamp on login (and clear on
	// logout). InCommand.Identity is the repo-identity arg threaded into the global
	// args, NOT a login signal, so it is the wrong gate; IsAuthResolved() is the
	// authoritative "logged in" bit. Skip silently when not logged in.
	FArcLoreSourceControlModule& LockModule = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	const bool bAuthResolved = LockModule.GetProvider().IsAuthResolved();
	if (bAuthResolved)
	{
		lore_lock_file_query_args_t LockArgs;
		FMemory::Memzero(&LockArgs, sizeof(LockArgs));
		LockArgs.branch = ArcLoreApi::EmptyLoreString();
		LockArgs.owner = ArcLoreApi::EmptyLoreString();
		LockArgs.path = ArcLoreApi::EmptyLoreString();
		// branch empty => current branch; owner empty => any owner; path empty => any path.

		const ArcLoreApi::FLoreOpInvoker LockInvoker =
			[&GlobalArgs, &LockArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_lock_file_query(&GlobalArgs.Args(), &LockArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector LockCollector =
			[this, &RepoRoot](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_LOCK_FILE_QUERY)
				{
					const lore_lock_file_query_event_data_t& LockEvent = Event.lock_file_query;
					// path is repo-relative; key the map by ABSOLUTE so UpdateStates
					// matches against States (keyed by absolute LocalFilename).
					const FString RelPath = ArcLoreApi::LoreStringToFString(LockEvent.path);
					const FString AbsPath = ArcLoreUpdateStatusInternal::ToAbsolute(RelPath, RepoRoot);
					FArcLoreUpdateStatusWorker::FLoreLockScratch Scratch;
					// owner is a RAW user id (compared to Provider.GetUserId() in UpdateStates).
					Scratch.OwnerId = ArcLoreApi::LoreStringToFString(LockEvent.owner);
					Scratch.LockedAt = LockEvent.locked_at; // milliseconds
					// branch query carries a branch id, but we drive display off the
					// current branch elsewhere; leave Branch empty.
					Locks.Add(AbsPath, MoveTemp(Scratch));
					UE_LOGF(LogSourceControl, Verbose,"ArcLore: [lockdiag] query lock: abs='%ls' rel='%ls' owner='%ls'", *AbsPath, *RelPath, *ArcLoreApi::LoreStringToFString(LockEvent.owner));
				}
			};

		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [UpdateStatus/lock] branch-wide query (branch=current, owner=any, path=any); globals: %ls",
			*ArcLoreApi::DescribeGlobalArgs(GlobalArgs.Args()));

		FString LockError;
		const int32 LockResult = ArcLoreApi::RunLoreOp(LockInvoker, LockCollector, LockError, TEXT("UpdateStatus/locks"));
		bLockQueryAuthoritative = (LockResult == 0);
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [UpdateStatus/lock] query returned %d lock(s); result=%d authoritative=%d", Locks.Num(), LockResult, bLockQueryAuthoritative ? 1 : 0);
		if (LockResult != 0)
		{
			// Soft: lock query is best-effort. Record but do not fail the command.
			InCommand.InfoMessages.Add(FString::Printf(TEXT("Lore lock query unavailable: %s"), *LockError));
		}
	}
	else
	{
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [UpdateStatus/lock] skipped (not logged in; auth unresolved)");
	}

	return InCommand.bCommandSuccessful;
}

bool FArcLoreUpdateStatusWorker::UpdateStates() const
{
	FArcLoreSourceControlModule& LoreSourceControl = FModuleManager::LoadModuleChecked<FArcLoreSourceControlModule>( "ArcLore" );
	FArcLoreSourceControlProvider& Provider = LoreSourceControl.GetProvider();

	bool bUpdated = false;
	// bLockedByMe compares the lock's RAW owner id to the provider's resolved user
	// id — NOT the display name (GetUserName), which never matches a raw id.
	const FString& ProviderUserId = Provider.GetUserId();
	const double NowSeconds = FPlatformTime::Seconds();

	// Apply the working-copy state + remote-current input for every queried file.
	for (const FArcLoreSourceControlState& StateItem : States)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(StateItem.LocalFilename);
		CachedState->WorkingCopyState = StateItem.WorkingCopyState;
		// Out-of-date is decided PER-ASSET from the repo-wide diff set, NOT the coarse
		// branch-level is_remote_ahead flag (which marked every statused file when behind).
		// Repo-wide mode is authoritative: a reported file is out-of-date IFF it is in
		// OutdatedFiles. Per-file mode must NOT touch bIsNewerOnRemote — it queries only a
		// subset and has no diff set, so it preserves the value the last repo-wide refresh
		// computed (a right-clicked clean asset must not be wrongly marked branch-wide).
		if (bWasRepoWide)
		{
			CachedState->bIsNewerOnRemote = OutdatedFiles.Contains(StateItem.LocalFilename);
		}
		CachedState->TimeStamp = FDateTime::Now();
		bUpdated = true;
	}

	// REPO-WIDE authority: the status pass enumerated every dirty + staged file and the
	// discovery pass found every untracked file, so anything NOT reported this pass is now
	// CLEAN (e.g. committed since the last refresh). Without this, a file that became clean
	// after a commit keeps its stale "Modified" cache entry and ghosts in the Dirty list.
	// Per-file mode (bWasRepoWide==false) queries only a subset, so clearing the others
	// would wipe legitimate state — gate strictly on repo-wide.
	if (bWasRepoWide)
	{
		TSet<FString> ReportedKeys;
		ReportedKeys.Reserve(States.Num());
		for (const FArcLoreSourceControlState& StateItem : States)
		{
			ReportedKeys.Add(StateItem.LocalFilename);
		}

		const TArray<FSourceControlStateRef> CachedStates =
			Provider.GetCachedStateByPredicate([](const FSourceControlStateRef&) { return true; });
		for (const FSourceControlStateRef& CachedRef : CachedStates)
		{
			const FString& CachedFilename = CachedRef->GetFilename();
			if (!ReportedKeys.Contains(CachedFilename))
			{
				TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(CachedFilename);
				CachedState->WorkingCopyState = EArcLoreState::Unchanged;
				CachedState->bStaged = false;
				// Out-of-date applies to clean files too: a file with no local change can
				// still be behind the remote. Stamp it from the same authoritative set so
				// exactly OutdatedFiles end up bIsNewerOnRemote=true after a repo-wide pass.
				CachedState->bIsNewerOnRemote = OutdatedFiles.Contains(CachedFilename);
				bUpdated = true;
			}
		}
	}

	// Apply file histories.
	for (const TPair<FString, TArcLoreSourceControlHistory>& History : Histories)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(History.Key);
		CachedState->History = History.Value;
		CachedState->TimeStamp = FDateTime::Now();
		bUpdated = true;
	}

	// Stamp the lock cache. bLockedByMe is decided HERE, on the game thread, by
	// comparing the captured owner to the provider's resolved identity.
	//
	// The branch-wide lock query is authoritative for ALL locks on the branch and
	// always returns every lock — regardless of whether this pass was repo-wide or
	// per-file. So when the query was authoritative, stamp every entry in Locks
	// unconditionally (Pass 1), then clear locks on files no longer locked with
	// scope that depends on mode (Pass 2).
	//
	// When !bLockQueryAuthoritative: leave all lock state untouched (preserve
	// direct CheckOut stamps; a skipped/failed query must not clobber them).
	UE_LOGF(LogSourceControl, Verbose,"ArcLore: [lockdiag] UpdateStates lock-apply: bWasRepoWide=%d States=%d Locks=%d authoritative=%d ProviderUserId='%ls'", bWasRepoWide ? 1 : 0, States.Num(), Locks.Num(), bLockQueryAuthoritative ? 1 : 0, *ProviderUserId);
	if (bLockQueryAuthoritative)
	{
		// Pass 1: stamp every locked file from the authoritative Locks map.
		// Runs in BOTH repo-wide and per-file mode — the branch-wide lock query
		// returns every lock on the branch, so clean-but-locked files (locked
		// elsewhere, not in the per-file query set) must still be stamped.
		for (const TPair<FString, FLoreLockScratch>& LockPair : Locks)
		{
			const FString& LockKey = LockPair.Key;
			const FLoreLockScratch& Scratch = LockPair.Value;
			TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(LockKey);
			CachedState->bIsLocked = true;
			CachedState->LockOwner = Scratch.OwnerId;
			CachedState->LockBranch = Scratch.Branch;
			// locked_at is in MILLISECONDS; FromUnixTimestamp wants seconds.
			CachedState->LockAcquiredAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Scratch.LockedAt / 1000));
			CachedState->bLockedByMe = (!Scratch.OwnerId.IsEmpty() && Scratch.OwnerId == ProviderUserId);
			CachedState->CachedAtSeconds = NowSeconds;
			bUpdated = true;
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: [lockdiag] stamp key='%ls' owner='%ls' byMe=%d", *LockKey, *Scratch.OwnerId, CachedState->bLockedByMe ? 1 : 0);
		}

		// Pass 2: clear locks on files no longer locked. Scope depends on mode:
		//   repo-wide: authoritative for the whole repo -> clear every cached file
		//              not in Locks (covers released locks anywhere).
		//   per-file:  only the queried subset (States) -> never clobber files this
		//              pass didn't query (they were not part of the request).
		if (bWasRepoWide)
		{
			const TArray<FSourceControlStateRef> AllCachedStates =
				Provider.GetCachedStateByPredicate([](const FSourceControlStateRef&) { return true; });
			for (const FSourceControlStateRef& CachedRef : AllCachedStates)
			{
				const FString& CachedFilename = CachedRef->GetFilename();
				if (!Locks.Contains(CachedFilename))
				{
					TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(CachedFilename);
					CachedState->bIsLocked = false;
					CachedState->bLockedByMe = false;
					CachedState->LockOwner.Empty();
					CachedState->LockBranch.Empty();
					CachedState->CachedAtSeconds = NowSeconds;
					bUpdated = true;
				}
			}
		}
		else
		{
			for (const FArcLoreSourceControlState& StateItem : States)
			{
				if (!Locks.Contains(StateItem.LocalFilename))
				{
					TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(StateItem.LocalFilename);
					CachedState->bIsLocked = false;
					CachedState->bLockedByMe = false;
					CachedState->LockOwner.Empty();
					CachedState->LockBranch.Empty();
					CachedState->CachedAtSeconds = NowSeconds;
					bUpdated = true;
					UE_LOGF(LogSourceControl, Verbose,"ArcLore: [lockdiag] clear(perfile) file='%ls' (not in Locks)", *StateItem.LocalFilename);
				}
			}
		}
	}
	// when !bLockQueryAuthoritative: leave all lock state untouched (preserve direct CheckOut stamps).

	// Update cached local changes count.
	{
		int32 Count = 0;
		for (const FArcLoreSourceControlState& StateItem : States)
		{
			if (StateItem.WorkingCopyState != EArcLoreState::Unknown
				&& StateItem.WorkingCopyState != EArcLoreState::Unchanged
				&& StateItem.WorkingCopyState != EArcLoreState::Ignored)
			{
				Count++;
			}
		}
		Provider.SetCachedNumLocalChanges(Count);
	}

	// Update cached staged-file count. A staged revision hard-errors a non-reset
	// branch switch, so the panel pre-warns off this count.
	{
		int32 StagedCount = 0;
		for (const FArcLoreSourceControlState& StateItem : States)
		{
			if (StateItem.bStaged)
			{
				StagedCount++;
			}
		}
		Provider.SetCachedStagedCount(StagedCount);
	}

	// Update cached remote/local ahead state from the status_revision event.
	// Count caveat: is_local_ahead/is_remote_ahead are independent divergent-DAG
	// flags, so revision_local_number - revision_remote_number is NOT a reliable
	// unpushed-commit count. Pass NumUnpushed unset and drive UI off bLocalAhead.
	{
		Provider.SetCachedRemoteState(bIsNewerOnRemote, bIsLocalAhead, TOptional<int>());
	}

	return bUpdated;
}

FName FArcLoreResolveWorker::GetName() const
{
	return "Resolve";
}

// ----------------------------------------------------------------------------
// Resolve — mark merge-conflicted files as resolved via lore_branch_merge_resolve.
//
// A single batch call over the command's repo-relative paths: the plain
// (whole-file accept of the staged merge) variant. UE's FResolve carries no
// mine/theirs choice in this engine, so the _resolve_mine/_resolve_theirs
// variants are not used here (MVP).
//
// UpdateStates clears the conflict by stamping each resolved file Modified — a
// resolved merge is no longer in conflict but still needs a commit, so Modified
// (not Unchanged) is the correct working state.
// ----------------------------------------------------------------------------
bool FArcLoreResolveWorker::Execute( class FArcLoreSourceControlCommand& InCommand )
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Build the path list for lore file-mutation ops. Unlike status/file_info
	// (which match a repo-relative string against the tracked index),
	// lore_branch_merge_resolve resolves a RELATIVE path against the process working dir
	// (…/Engine/Binaries/Win64) — sending "ArcGame/Plugins/…" produced
	// "Invalid path Engine/Binaries/Win64/ArcGame/…".
	// So feed resolve the ABSOLUTE path; lore derives the repo-relative form from
	// repository_path itself.
	TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
	PathBuffers.Reserve(InCommand.Files.Num());
	TArray<lore_string_t> PathViews;
	PathViews.Reserve(InCommand.Files.Num());
	for (const FString& AbsFile : InCommand.Files)
	{
		const int32 BufIndex = PathBuffers.Emplace(AbsFile);
		PathViews.Add(PathBuffers[BufIndex].View());
	}

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// ---- RESOLVE op ------------------------------------------------------------
	{
		lore_branch_merge_resolve_args_t ResolveArgs;
		FMemory::Memzero(&ResolveArgs, sizeof(ResolveArgs));
		ResolveArgs.paths.ptr   = PathViews.GetData();
		ResolveArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());

		const ArcLoreApi::FLoreOpInvoker ResolveInvoker =
			[&GlobalArgs, &ResolveArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_branch_merge_resolve(&GlobalArgs.Args(), &ResolveArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector ResolveCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString ResolveError;
		const int32 ResolveResult = ArcLoreApi::RunLoreOp(ResolveInvoker, ResolveCollector, ResolveError);
		if (ResolveResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_branch_merge_resolve failed: %s"), *ResolveError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// Resolve succeeded for the whole batch.
	ResolvedFiles = InCommand.Files;

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreResolveWorker::UpdateStates() const
{
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	bool bUpdated = false;
	const FDateTime Now = FDateTime::Now();
	for (const FString& AbsFile : ResolvedFiles)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(AbsFile);
		// Conflict cleared. A resolved merge still needs a commit, so Modified
		// (not Unchanged) is the right state.
		CachedState->WorkingCopyState = EArcLoreState::Modified;
		CachedState->TimeStamp = Now;
		bUpdated = true;
	}
	return bUpdated;
}

FName FArcLoreCheckOutWorker::GetName() const { return "CheckOut"; }

// ----------------------------------------------------------------------------
// CheckOut — acquire Lore file locks (lock-as-checkout) via lore_lock_file_acquire.
//
// A single batch acquire over the command's repo-relative paths. The op emits a
// LOCK_FILE_ACQUIRE event per newly-acquired path and per path already held BY ME
// (the latter grouped under a LOCK_FILE_ACQUIRE_BEGIN with ignored=1) — both count as "ours".
// A path locked by ANOTHER user makes the whole op FAIL (RunLoreOp returns
// non-zero with the lore error naming the conflicting file/owner); we surface
// that as an explicit failure and acquire nothing on disk (no read-only flips —
// the provider does not use local read-only state).
//
// Successfully-acquired paths (collected on the lore worker thread; each echoed
// path is mapped back to its matching InCommand.Files entry so the cache key equals
// the editor's absolute key) are stamped locked-by-me in UpdateStates on the game thread.
// ----------------------------------------------------------------------------
bool FArcLoreCheckOutWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Build the path list for the lore lock-acquire op. Unlike status/file_info
	// (which match a repo-relative string against the tracked index),
	// lore_lock_file_acquire resolves a RELATIVE path against the process working dir
	// (…/Engine/Binaries/Win64) — sending "ArcGame/Plugins/…" produced
	// "Invalid path Engine/Binaries/Win64/ArcGame/…".
	// So feed acquire the ABSOLUTE path; lore derives the repo-relative form from
	// repository_path itself.
	TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
	PathBuffers.Reserve(InCommand.Files.Num());
	TArray<lore_string_t> PathViews;
	PathViews.Reserve(InCommand.Files.Num());
	for (const FString& AbsFile : InCommand.Files)
	{
		const int32 BufIndex = PathBuffers.Emplace(AbsFile);
		PathViews.Add(PathBuffers[BufIndex].View());
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [CheckOut] sending path to lore: '%ls'", *AbsFile);
	}

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// ---- ACQUIRE op ------------------------------------------------------------
	{
		lore_lock_file_acquire_args_t AcquireArgs;
		FMemory::Memzero(&AcquireArgs, sizeof(AcquireArgs));
		AcquireArgs.branch = ArcLoreApi::EmptyLoreString();
		AcquireArgs.paths.ptr   = PathViews.GetData();
		AcquireArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());
		// branch left empty => acquire on the current branch.

		const ArcLoreApi::FLoreOpInvoker AcquireInvoker =
			[&GlobalArgs, &AcquireArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_lock_file_acquire(&GlobalArgs.Args(), &AcquireArgs, Config);
			};

		// Capture every path Lore reports as acquired (newly locked) or ignored
		// (already held by us) — both leave the file ours. The StateCache (set by
		// UpdateStates) is keyed by the EDITOR's absolute filenames — the very values
		// in InCommand.Files (produced by ResolveAbsoluteFilenames). Lore may echo the
		// path back in any form (repo-relative, or — now that we send absolute input —
		// absolute and possibly differently normalized). If we keyed off the echoed
		// form directly, the lock would land on the server but the editor would never
		// show the locked-by-me state. So normalize each echoed path to a full path,
		// then resolve it back to the matching InCommand.Files entry via IsSamePath so
		// the cache key is guaranteed identical. InCommand outlives this lambda (it runs
		// during Execute on the worker thread), so capturing it by reference is safe.
		const ArcLoreApi::FLoreEventCollector AcquireCollector =
			[this, &RepoRoot, &InCommand](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);

				// Map an echoed lock path (any form) onto the editor's StateCache key.
				const TFunction<void(const FString&, const TCHAR*)> KeyFromEcho =
					[this, &RepoRoot, &InCommand](const FString& Echoed, const TCHAR* Context) -> void
				{
					// ConvertRelativePathToFull returns Echoed unchanged when already
					// absolute, and resolves it against RepoRoot when relative — handles
					// both echo forms.
					const FString Full = FPaths::ConvertRelativePathToFull(RepoRoot, Echoed);
					for (const FString& Candidate : InCommand.Files)
					{
						if (FPaths::IsSamePath(Candidate, Full))
						{
							AcquiredFiles.Add(Candidate);
							return;
						}
					}
					// No editor entry matched — the cache key may not line up. Push the
					// best-effort full path and make the mismatch visible.
					UE_LOGF(LogSourceControl, Warning,"ArcLore: [CheckOut] %ls echo '%ls' (full '%ls') matched no InCommand.Files entry; using best-effort key",
						Context, *Echoed, *Full);
					AcquiredFiles.Add(Full);
				};

				switch (Tag)
				{
				case LORE_EVENT_LOCK_FILE_ACQUIRE:
					{
						const FString Echoed = ArcLoreApi::LoreStringToFString(Event.lock_file_acquire.path);
						UE_LOGF(LogSourceControl, Verbose,"ArcLore: [CheckOut] lore echoed lock path: '%ls'", *Echoed);
						KeyFromEcho(Echoed, TEXT("LOCK_FILE_ACQUIRE"));
						break;
					}
				default:
					break;
				}
			};

		FString AcquireError;
		const int32 AcquireResult = ArcLoreApi::RunLoreOp(AcquireInvoker, AcquireCollector, AcquireError);
		if (AcquireResult != 0)
		{
			// A path is locked by another user (or the acquire otherwise failed).
			// Surface a clear, explicit failure; the lore error names the file/owner.
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("Failed to acquire lock (a file may be locked by another user): %s"), *AcquireError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreCheckOutWorker::UpdateStates() const
{
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	// LockOwner is a RAW Lore user id, so stamp the provider's user id (not the
	// display name) to stay consistent with the lock_query stamp in UpdateStatus —
	// otherwise bLockedByMe would flip false on the next status refresh.
	const FString& ProviderUserId = Provider.GetUserId();
	const double NowSeconds = FPlatformTime::Seconds();

	bool bUpdated = false;
	for (const FString& AbsFile : AcquiredFiles)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(AbsFile);
		CachedState->bIsLocked = true;
		CachedState->bLockedByMe = true;
		CachedState->LockOwner = ProviderUserId;
		CachedState->CachedAtSeconds = NowSeconds;
		bUpdated = true;
	}

	return bUpdated;
}

FName FArcLoreReleaseLockWorker::GetName() const { return "LoreReleaseLock"; }

// ----------------------------------------------------------------------------
// ReleaseLock — standalone Unlock of Lore file locks via lore_lock_file_release.
//
// A single batch release over the command's paths. Mirrors the CheckOut worker's
// path-building (feed the ABSOLUTE path; lore derives the repo-relative form from
// repository_path) and the Push worker's release-args shape (branch/owner/owner_id
// left empty => current user, current branch). The op emits a LOCK_FILE_RELEASE
// event per successfully-released path; a LOCK_FILE_RELEASE_BEGIN with not_found=1 is SOFT (the
// lock simply was not ours/did not exist) and is NOT an error.
//
// Released paths (collected on the lore worker thread; each echoed path mapped back
// to its matching InCommand.Files entry via IsSamePath so the cache key equals the
// editor's absolute key) are stamped unlocked in UpdateStates on the game thread.
// ----------------------------------------------------------------------------
bool FArcLoreReleaseLockWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Build the path list for the lore lock-release op. lore_lock_file_release resolves
	// a RELATIVE path against the process working dir (…/Engine/Binaries/Win64), so feed
	// it the ABSOLUTE path; lore derives the repo-relative form from repository_path
	// itself. (Same hazard documented on the CheckOut acquire / Push release blocks.)
	TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
	PathBuffers.Reserve(InCommand.Files.Num());
	TArray<lore_string_t> PathViews;
	PathViews.Reserve(InCommand.Files.Num());
	for (const FString& AbsFile : InCommand.Files)
	{
		const int32 BufIndex = PathBuffers.Emplace(AbsFile);
		PathViews.Add(PathBuffers[BufIndex].View());
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [ReleaseLock] sending path to lore: '%ls'", *AbsFile);
	}

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// ---- RELEASE op ------------------------------------------------------------
	{
		lore_lock_file_release_args_t ReleaseArgs;
		FMemory::Memzero(&ReleaseArgs, sizeof(ReleaseArgs));
		ReleaseArgs.branch = ArcLoreApi::EmptyLoreString();
		ReleaseArgs.owner = ArcLoreApi::EmptyLoreString();
		ReleaseArgs.owner_id = ArcLoreApi::EmptyLoreString();
		ReleaseArgs.paths.ptr   = PathViews.GetData();
		ReleaseArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());
		// branch / owner / owner_id left empty => current branch, current user.

		const ArcLoreApi::FLoreOpInvoker ReleaseInvoker =
			[&GlobalArgs, &ReleaseArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_lock_file_release(&GlobalArgs.Args(), &ReleaseArgs, Config);
			};

		// Capture every path Lore reports as released. The StateCache (set by
		// UpdateStates) is keyed by the EDITOR's absolute filenames — the values in
		// InCommand.Files. Lore may echo the path back in any form, so normalize each
		// echoed path to a full path, then resolve it back to the matching
		// InCommand.Files entry via IsSamePath so the cache key is guaranteed identical
		// (mirrors the CheckOut acquire collector). A not-found release emits only a
		// LOCK_FILE_RELEASE_BEGIN with not_found=1 and no paths (soft outcome). InCommand
		// outlives this lambda (it runs during Execute on the worker thread), so capturing
		// it by reference is safe.
		const ArcLoreApi::FLoreEventCollector ReleaseCollector =
			[this, &RepoRoot, &InCommand](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag != LORE_EVENT_LOCK_FILE_RELEASE)
				{
					// LOCK_FILE_RELEASE_BEGIN (including not_found=1) and any other event is soft.
					return;
				}

				const FString Echoed = ArcLoreApi::LoreStringToFString(Event.lock_file_release.path);
				UE_LOGF(LogSourceControl, Verbose,"ArcLore: [ReleaseLock] lore echoed released path: '%ls'", *Echoed);

				// ConvertRelativePathToFull returns Echoed unchanged when already absolute,
				// and resolves it against RepoRoot when relative — handles both echo forms.
				const FString Full = FPaths::ConvertRelativePathToFull(RepoRoot, Echoed);
				for (const FString& Candidate : InCommand.Files)
				{
					if (FPaths::IsSamePath(Candidate, Full))
					{
						ReleasedFiles.Add(Candidate);
						return;
					}
				}
				// No editor entry matched — the cache key may not line up. Push the
				// best-effort full path and make the mismatch visible.
				UE_LOGF(LogSourceControl, Warning,"ArcLore: [ReleaseLock] release echo '%ls' (full '%ls') matched no InCommand.Files entry; using best-effort key",
					*Echoed, *Full);
				ReleasedFiles.Add(Full);
			};

		FString ReleaseError;
		const int32 ReleaseResult = ArcLoreApi::RunLoreOp(ReleaseInvoker, ReleaseCollector, ReleaseError, TEXT("ReleaseLock/release"));
		if (ReleaseResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_lock_file_release failed: %s"), *ReleaseError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreReleaseLockWorker::UpdateStates() const
{
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	bool bUpdated = false;
	const FDateTime Now = FDateTime::Now();
	for (const FString& AbsFile : ReleasedFiles)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(AbsFile);
		CachedState->bIsLocked = false;
		CachedState->bLockedByMe = false;
		CachedState->LockOwner.Empty();
		CachedState->LockBranch.Empty();
		CachedState->TimeStamp = Now;
		bUpdated = true;
	}

	return bUpdated;
}

FName FArcLorePushWorker::GetName() const
{
	return "Push";
}

// ----------------------------------------------------------------------------
// Push — explicit push of committed-but-unpushed local revisions to the remote
// via lore_branch_push, then a cheap re-query of ahead/behind state and (when
// files are supplied) a lock release.
//
//   1) PUSH op (lore_branch_push, current branch, fast_forward_merge=0). On
//      failure: error + return false, change nothing.
//   2) RE-QUERY op (lore_repository_status, revision_only=1) — read the
//      status_revision is_remote_ahead/is_local_ahead and store into worker
//      members. We actually re-query (never optimistically zero) and do NOT
//      difference revision numbers into an unpushed count (Step-1 decision).
//   3) RELEASE op (lore_lock_file_release) only when files were supplied — push
//      with no files is a branch-only push. FLorePush carries no KeepCheckedOut
//      (FConnect-style), so locks are always released when files are given.
//
// Execute + the status collector run on the POOL thread (write only worker-local
// members there). UpdateStates runs on the GAME thread and is the only place the
// provider setter is touched.
// ----------------------------------------------------------------------------
bool FArcLorePushWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Build the ABSOLUTE path list for the lock-release op. lore_lock_file_release
	// resolves a RELATIVE path against the process working dir (…/Engine/Binaries/Win64),
	// so feed it the ABSOLUTE path; lore derives the repo-relative form from
	// repository_path itself. (Same hazard documented on the CheckIn release block.)
	TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
	PathBuffers.Reserve(InCommand.Files.Num());
	TArray<lore_string_t> PathViews;
	PathViews.Reserve(InCommand.Files.Num());
	TArray<FString> AbsFiles;
	AbsFiles.Reserve(InCommand.Files.Num());
	for (const FString& AbsFile : InCommand.Files)
	{
		const int32 BufIndex = PathBuffers.Emplace(AbsFile);
		PathViews.Add(PathBuffers[BufIndex].View());
		AbsFiles.Add(AbsFile);
	}

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// ---- 1) PUSH op ------------------------------------------------------------
	{
		lore_branch_push_args_t PushArgs;
		FMemory::Memzero(&PushArgs, sizeof(PushArgs));
		PushArgs.branch = ArcLoreApi::EmptyLoreString();
		// branch left empty => push the current branch.
		PushArgs.fast_forward_merge = 0;

		const ArcLoreApi::FLoreOpInvoker PushInvoker =
			[&GlobalArgs, &PushArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_branch_push(&GlobalArgs.Args(), &PushArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector PushCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString PushError;
		const int32 PushResult = ArcLoreApi::RunLoreOp(PushInvoker, PushCollector, PushError, TEXT("Push/push"));
		if (PushResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_branch_push failed: %s"), *PushError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// Push succeeded.
	bPushSucceeded = true;

	// ---- 2) RE-QUERY ahead/behind (revision_only=1, cheap) ---------------------
	{
		lore_repository_status_args_t StatusArgs;
		FMemory::Memzero(&StatusArgs, sizeof(StatusArgs));
		// revision_only=1 skips file diffs and the filesystem scan — we only want the
		// status_revision event's ahead/behind flags after the push.
		StatusArgs.revision_only = 1;

		const ArcLoreApi::FLoreOpInvoker StatusInvoker =
			[&GlobalArgs, &StatusArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_status(&GlobalArgs.Args(), &StatusArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StatusCollector =
			[this](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REPOSITORY_STATUS_REVISION)
				{
					const lore_repository_status_revision_event_data_t& RevEvent = Event.repository_status_revision;
					bIsNewerOnRemote = (RevEvent.is_remote_ahead != 0);
					bIsLocalAhead = (RevEvent.is_local_ahead != 0);
				}
			};

		FString StatusError;
		const int32 StatusResult = ArcLoreApi::RunLoreOp(StatusInvoker, StatusCollector, StatusError, TEXT("Push/status"));
		if (StatusResult != 0)
		{
			// Soft: the push is durable. A failed re-query just leaves the cached
			// ahead/behind stale; record a warning but the push still succeeded.
			InCommand.InfoMessages.Add(FString::Printf(TEXT("Ahead/behind re-query after push failed (push is durable): %s"), *StatusError));
		}
	}

	// ---- 3) RELEASE op (only when files supplied) ------------------------------
	if (InCommand.Files.Num() > 0)
	{
		lore_lock_file_release_args_t ReleaseArgs;
		FMemory::Memzero(&ReleaseArgs, sizeof(ReleaseArgs));
		ReleaseArgs.branch = ArcLoreApi::EmptyLoreString();
		ReleaseArgs.owner = ArcLoreApi::EmptyLoreString();
		ReleaseArgs.owner_id = ArcLoreApi::EmptyLoreString();
		ReleaseArgs.paths.ptr   = PathViews.GetData();
		ReleaseArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());
		// branch / owner / owner_id left empty => current branch, current user.

		const ArcLoreApi::FLoreOpInvoker ReleaseInvoker =
			[&GlobalArgs, &ReleaseArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_lock_file_release(&GlobalArgs.Args(), &ReleaseArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector ReleaseCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString ReleaseError;
		const int32 ReleaseResult = ArcLoreApi::RunLoreOp(ReleaseInvoker, ReleaseCollector, ReleaseError, TEXT("Push/release"));
		if (ReleaseResult != 0)
		{
			// Soft: the push is durable. A failed release just leaves the lock held;
			// record a warning but the push still succeeded.
			InCommand.InfoMessages.Add(FString::Printf(TEXT("Lock release after push failed (push succeeded): %s"), *ReleaseError));
		}
		else
		{
			ReleasedFiles = AbsFiles;
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLorePushWorker::UpdateStates() const
{
	// Nothing succeeded on push failure: leave states untouched.
	if (!bPushSucceeded)
	{
		return false;
	}

	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();

	// Apply the re-queried ahead/behind flags. NumUnpushed stays unset (Step-1
	// decision: is_local_ahead/is_remote_ahead are divergent-DAG flags, not a
	// shared-lineage unpushed-commit count).
	Provider.SetCachedRemoteState(bIsNewerOnRemote, bIsLocalAhead, TOptional<int>());

	// Released-lock cache fix-up (files supplied + release succeeded).
	const FDateTime Now = FDateTime::Now();
	for (const FString& AbsFile : ReleasedFiles)
	{
		TSharedRef<FArcLoreSourceControlState, ESPMode::ThreadSafe> CachedState = Provider.GetStateInternal(AbsFile);
		CachedState->bIsLocked = false;
		CachedState->bLockedByMe = false;
		CachedState->LockOwner.Empty();
		CachedState->LockBranch.Empty();
		CachedState->TimeStamp = Now;
	}

	// Return true so OnSourceControlStateChanged broadcasts (ahead/behind changed).
	return true;
}

// ----------------------------------------------------------------------------
// FArcLoreRevisionLogWorker
//
// No-file op: query the current branch's commit log via lore_revision_history.
// The collector is a state machine identical to the UpdateStatus history
// collector — a revision_history_entry event opens a new revision record and
// trailing METADATA events attach to it until the next entry. Execute runs on
// the POOL thread (worker-local Entries only); UpdateStates runs on the GAME
// thread and is the only place the operation payload is touched.
// ----------------------------------------------------------------------------
namespace ArcLoreRevisionLogInternal
{
	// Render a lore_hash_t as a 64-character lowercase hex string. Mirrors
	// ArcLoreRevisionHelpers::HashToHex (file-scoped in ArcLoreSourceControlRevision.cpp,
	// not exposed in a header) so the entry collector matches InitFromHistory's
	// hash-hex encoding exactly.
	FString HashToHex(const lore_hash_t& Hash)
	{
		FString Result;
		Result.Reserve(64);
		for (int32 ByteIndex = 0; ByteIndex < 32; ++ByteIndex)
		{
			Result += FString::Printf(TEXT("%02x"), Hash.data[ByteIndex]);
		}
		return Result;
	}

	// True when every one of the hash's 32 bytes is zero. A zero revision_remote
	// means the remote tip is unknown (remote unreachable / branch not on remote).
	bool IsZeroHash(const lore_hash_t& Hash)
	{
		for (int32 ByteIndex = 0; ByteIndex < 32; ++ByteIndex)
		{
			if (Hash.data[ByteIndex] != 0) { return false; }
		}
		return true;
	}
}

FName FArcLoreRevisionLogWorker::GetName() const
{
	return "LoreRevisionLog";
}

bool FArcLoreRevisionLogWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// collected revisions back to its payload on the game thread.
	Operation = InCommand.Operation;

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	// ---- STATUS pre-pass (revision_only=1) — read the remote branch tip number --
	// Cheap re-query of the remote tip so we can flag locally-committed-but-unpushed
	// revisions below. RemoteNumber is the remote branch's latest revision number;
	// bRemoteKnown is true only when the remote query returned an authoritative tip
	// (branch exists on remote AND its identifier is non-zero). When the remote is
	// unknown/unreachable we leave bRemoteKnown false and mark nothing. This status
	// call is SOFT: on failure we proceed with the history alone, unmarked.
	uint64 RemoteNumber = 0;
	bool bRemoteKnown = false;
	{
		lore_repository_status_args_t StatusArgs;
		FMemory::Memzero(&StatusArgs, sizeof(StatusArgs));
		// revision_only=1 skips file diffs and the filesystem scan — we only want the
		// status_revision event's remote tip number and the remote-known flags.
		StatusArgs.revision_only = 1;

		const ArcLoreApi::FLoreOpInvoker StatusInvoker =
			[&GlobalArgs, &StatusArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_status(&GlobalArgs.Args(), &StatusArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StatusCollector =
			[&RemoteNumber, &bRemoteKnown](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REPOSITORY_STATUS_REVISION)
				{
					const lore_repository_status_revision_event_data_t& RevEvent = Event.repository_status_revision;
					// Remote is "known" when the branch exists on the remote AND the
					// returned tip identifier is non-zero (zero == unknown/unreachable).
					bRemoteKnown = (RevEvent.remote_branch_exist != 0)
						&& !ArcLoreRevisionLogInternal::IsZeroHash(RevEvent.revision_remote);
					RemoteNumber = RevEvent.revision_remote_number;
				}
			};

		FString StatusError;
		const int32 StatusResult = ArcLoreApi::RunLoreOp(StatusInvoker, StatusCollector, StatusError, TEXT("RevisionLog/status"));
		if (StatusResult != 0)
		{
			// Soft: proceed with the history; nothing gets the local-only mark.
			bRemoteKnown = false;
			InCommand.InfoMessages.Add(FString::Printf(TEXT("lore_repository_status (revision) failed; unpushed marks unavailable: %s"), *StatusError));
		}
	}

	lore_revision_history_args_t Args;
	FMemory::Memzero(&Args, sizeof(Args));
	Args.revision = ArcLoreApi::EmptyLoreString();
	Args.branch = ArcLoreApi::EmptyLoreString();
	// branch / revision left empty => the current branch from its tip.
	Args.length = 200;

	const ArcLoreApi::FLoreOpInvoker Invoker =
		[&GlobalArgs, &Args](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_revision_history(&GlobalArgs.Args(), &Args, Config);
		};

	// Collector state machine: a revision_history_entry event opens a new revision
	// record; trailing METADATA events attach to it until the next entry. We append
	// each record as it opens and mutate the last.
	const ArcLoreApi::FLoreEventCollector Collector =
		[this](const lore_event_t& Event)
		{
			const uint32_t Tag = lore_event_type(&Event);
			switch (Tag)
			{
			case LORE_EVENT_REVISION_HISTORY_ENTRY:
				{
					// Open a new revision record.
					const lore_revision_history_entry_event_data_t& Entry = Event.revision_history_entry;
					TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe> NewRev =
						MakeShared<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>();
					NewRev->RevisionHashHex = ArcLoreRevisionLogInternal::HashToHex(Entry.revision);
					NewRev->ShortRevisionHashHex = NewRev->RevisionHashHex.Left(8);
					// revision_number is uint64; cast to int32 for the interface.
					NewRev->RevisionNumber = static_cast<int32>(Entry.revision_number);
					Entries.Add(NewRev);
					break;
				}
			case LORE_EVENT_METADATA:
				{
					// Attach to the current (most recent) revision record.
					if (Entries.Num() == 0)
					{
						break;
					}
					TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>& Current = Entries.Last();
					const lore_metadata_event_data_t& Meta = Event.metadata;
					const FString Key = ArcLoreApi::LoreStringToFString(Meta.key);
					if (Key == TEXT("message") && Meta.value.tag == LORE_METADATA_TYPE_STRING)
					{
						Current->SetDescription(ArcLoreApi::LoreStringToFString(Meta.value.string));
					}
					else if (Key == TEXT("created-by") && Meta.value.tag == LORE_METADATA_TYPE_STRING)
					{
						Current->SetUserName(ArcLoreApi::LoreStringToFString(Meta.value.string));
					}
					else if (Key == TEXT("timestamp") && Meta.value.tag == LORE_METADATA_TYPE_NUMERIC)
					{
						Current->SetDateFromEpochMillis(Meta.value.numeric);
					}
					break;
				}
			default:
				break;
			}
		};

	FString Error;
	const int32 Result = ArcLoreApi::RunLoreOp(Invoker, Collector, Error, TEXT("RevisionLog"));

	// Mark locally-committed-but-unpushed revisions: a revision whose sequential
	// number is greater than the remote branch tip number is not yet on the remote.
	// Only mark when the remote tip is authoritatively known (bRemoteKnown); when it
	// is not, every entry stays unmarked (renders as normal/pushed).
	if (bRemoteKnown)
	{
		for (TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>& Entry : Entries)
		{
			Entry->bIsLocalOnly = (Entry->GetRevisionNumber() > static_cast<int32>(RemoteNumber));
		}
	}

	// Soft: a local read-only op. On failure record an info line but still return
	// the success bool so the command result reflects the actual outcome.
	InCommand.bCommandSuccessful = (Result == 0);
	if (Result != 0)
	{
		InCommand.InfoMessages.Add(FString::Printf(TEXT("lore_revision_history failed: %s"), *Error));
	}

	return InCommand.bCommandSuccessful;
}

bool FArcLoreRevisionLogWorker::UpdateStates() const
{
	// Hand the collected revisions to the operation payload (captured in Execute,
	// since UpdateStates takes no command). No StateCache change, so return false
	// (nothing to broadcast).
	if (Operation.IsValid())
	{
		StaticCastSharedRef<FLoreRevisionLog>(Operation.ToSharedRef())->Revisions = Entries;
	}
	return false;
}

// ----------------------------------------------------------------------------
// FArcLoreRepositoryViewWorker
//
// LOCAL-only repository view: two reads under one FLoreGlobalArgsScope.
//   1) lore_branch_list (archived=0) — the branch_list_begin event reports the
//      location of the run that follows (LOCAL/REMOTE); each branch_list_entry
//      copies name/category/creator/created/flags. We track the current location
//      and stamp bRemote per entry from it.
//   2) lore_repository_instance_list — each repository_instance event copies the
//      checkout path / branch name / stale flag.
// Both are local reads. Execute runs on the POOL thread (worker-local scratch
// only); UpdateStates runs on the GAME thread and writes the captured operation.
// ----------------------------------------------------------------------------
FName FArcLoreRepositoryViewWorker::GetName() const
{
	return "LoreRepositoryView";
}

bool FArcLoreRepositoryViewWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// collected branches/instances back to its payload on the game thread.
	Operation = InCommand.Operation;

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	bool bAnyResult = false;

	// ---- 1) BRANCH LIST op (local) ---------------------------------------------
	{
		lore_branch_list_args_t BranchArgs;
		FMemory::Memzero(&BranchArgs, sizeof(BranchArgs));
		BranchArgs.archived = 0;

		const ArcLoreApi::FLoreOpInvoker BranchInvoker =
			[&GlobalArgs, &BranchArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_branch_list(&GlobalArgs.Args(), &BranchArgs, Config);
			};

		// The begin event reports the location of the entries that follow; track it
		// and stamp bRemote per entry. created is MILLISECONDS since the epoch.
		bool bCurrentLocationRemote = false;
		const ArcLoreApi::FLoreEventCollector BranchCollector =
			[this, &bCurrentLocationRemote](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				switch (Tag)
				{
				case LORE_EVENT_BRANCH_LIST_BEGIN:
					{
						bCurrentLocationRemote = (Event.branch_list_begin.location == LORE_BRANCH_LOCATION_REMOTE);
						break;
					}
				case LORE_EVENT_BRANCH_LIST_ENTRY:
					{
						const lore_branch_list_entry_event_data_t& EntryData = Event.branch_list_entry;
						FArcLoreBranchEntry BranchEntry;
						BranchEntry.Name = ArcLoreApi::LoreStringToFString(EntryData.name);
						BranchEntry.Category = ArcLoreApi::LoreStringToFString(EntryData.category);
						BranchEntry.Creator = ArcLoreApi::LoreStringToFString(EntryData.creator);
						// created is uint64 MILLISECONDS since the epoch — convert to seconds.
						BranchEntry.Created = FDateTime::FromUnixTimestamp(static_cast<int64>(EntryData.created / 1000));
						BranchEntry.bIsCurrent = (EntryData.is_current != 0);
						BranchEntry.bArchived = (EntryData.archived != 0);
						BranchEntry.bRemote = bCurrentLocationRemote;
						Branches.Add(MoveTemp(BranchEntry));
						break;
					}
				default:
					break;
				}
			};

		FString BranchError;
		const int32 BranchResult = ArcLoreApi::RunLoreOp(BranchInvoker, BranchCollector, BranchError, TEXT("RepositoryView/branches"));
		if (BranchResult != 0)
		{
			// Soft: local read. Record an info line, keep going to the instance list.
			InCommand.InfoMessages.Add(FString::Printf(TEXT("lore_branch_list failed: %s"), *BranchError));
		}
		else
		{
			bAnyResult = true;
		}
	}

	// ---- 2) INSTANCE LIST op (local) -------------------------------------------
	{
		lore_repository_instance_list_args_t InstanceArgs;
		FMemory::Memzero(&InstanceArgs, sizeof(InstanceArgs));
		// args carry no usable params — zero-init.

		const ArcLoreApi::FLoreOpInvoker InstanceInvoker =
			[&GlobalArgs, &InstanceArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_instance_list(&GlobalArgs.Args(), &InstanceArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector InstanceCollector =
			[this](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REPOSITORY_INSTANCE)
				{
					const lore_repository_instance_event_data_t& InstanceData = Event.repository_instance;
					FArcLoreInstanceEntry InstanceEntry;
					InstanceEntry.Path = ArcLoreApi::LoreStringToFString(InstanceData.path);
					InstanceEntry.BranchName = ArcLoreApi::LoreStringToFString(InstanceData.branch_name);
					InstanceEntry.bStale = (InstanceData.stale != 0);
					Instances.Add(MoveTemp(InstanceEntry));
				}
			};

		FString InstanceError;
		const int32 InstanceResult = ArcLoreApi::RunLoreOp(InstanceInvoker, InstanceCollector, InstanceError, TEXT("RepositoryView/instances"));
		if (InstanceResult != 0)
		{
			// Soft: local read. Record an info line.
			InCommand.InfoMessages.Add(FString::Printf(TEXT("lore_repository_instance_list failed: %s"), *InstanceError));
		}
		else
		{
			bAnyResult = true;
		}
	}

	// Soft success: the view is informational; succeed if either local read worked.
	InCommand.bCommandSuccessful = bAnyResult;
	return InCommand.bCommandSuccessful;
}

bool FArcLoreRepositoryViewWorker::UpdateStates() const
{
	// Hand the collected branches/instances to the operation payload (captured in
	// Execute, since UpdateStates takes no command). No StateCache change, so
	// return false (nothing to broadcast).
	if (Operation.IsValid())
	{
		TSharedRef<FLoreRepositoryView, ESPMode::ThreadSafe> View = StaticCastSharedRef<FLoreRepositoryView>(Operation.ToSharedRef());
		View->Branches = Branches;
		View->Instances = Instances;
	}
	return false;
}

// ----------------------------------------------------------------------------
// FArcLoreBranchGraphWorker
//
// Cross-branch commit DAG fetch for the Branch Graph tab. Two phases under ONE
// FLoreGlobalArgsScope on the POOL thread:
//   1) lore_branch_list (archived = bIncludeArchived) — one FArcLoreBranchInput
//      per LORE_EVENT_BRANCH_LIST_ENTRY: name, tip hash, the FULL fork-lineage
//      stack[], is_current, archived.
//   2) For EACH collected branch, a SEPARATE lore_revision_history call
//      (branch = that name; length capped; only_branch = 0 so shared ancestry is
//      captured for dedup). The collector mirrors FArcLoreRevisionLogWorker: a
//      REVISION_HISTORY_ENTRY opens a record, trailing METADATA mutates the last.
//      Per-branch collector state is reset between calls so metadata never bleeds
//      across branches. A single branch's history failure is recorded but does not
//      fail the command; success follows the branch enumeration (phase 1).
// UpdateStates runs on the GAME thread and writes both scratch arrays back.
// ----------------------------------------------------------------------------
namespace ArcLoreBranchGraphInternal
{
	// Render a lore_hash_t as a 64-character lowercase hex string. Copied from
	// ArcLoreRevisionLogInternal::HashToHex so this worker is self-contained.
	FString HashToHex(const lore_hash_t& Hash)
	{
		FString Result;
		Result.Reserve(64);
		for (int32 ByteIndex = 0; ByteIndex < 32; ++ByteIndex)
		{
			Result += FString::Printf(TEXT("%02x"), Hash.data[ByteIndex]);
		}
		return Result;
	}

	// True when every one of the hash's 32 bytes is zero. A zero parent slot means
	// "no parent" (parent[1] is zero for a non-merge commit). Copied from
	// ArcLoreRevisionLogInternal::IsZeroHash so this worker is self-contained.
	bool IsZeroHash(const lore_hash_t& Hash)
	{
		for (int32 ByteIndex = 0; ByteIndex < 32; ++ByteIndex)
		{
			if (Hash.data[ByteIndex] != 0) { return false; }
		}
		return true;
	}

	// Render a lore_branch_id_t (16 raw bytes) as a 32-character lowercase hex
	// string. The branch_point only exposes the opaque branch id (no name string),
	// so we store its hex form as the branch-point label.
	FString BranchIdToHex(const lore_branch_id_t& Id)
	{
		FString Result;
		Result.Reserve(32);
		for (int32 ByteIndex = 0; ByteIndex < 16; ++ByteIndex)
		{
			Result += FString::Printf(TEXT("%02x"), Id.data[ByteIndex]);
		}
		return Result;
	}
}

FName FArcLoreBranchGraphWorker::GetName() const
{
	return "LoreBranchGraph";
}

bool FArcLoreBranchGraphWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// collected branches/revisions back to its payload on the game thread.
	Operation = InCommand.Operation;

	TSharedRef<FLoreBranchGraph, ESPMode::ThreadSafe> Op =
		StaticCastSharedRef<FLoreBranchGraph>(InCommand.Operation);

	// Fixed per-branch history cap. A concrete bound (not 0/unlimited) keeps each of
	// the N history round-trips bounded; ancestry beyond it surfaces as dangling
	// edges in BuildLayout, not as missing commits.
	static constexpr int32 BranchHistoryLengthCap = 200;

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	// ---- PHASE 1: BRANCH LIST (local) ------------------------------------------
	{
		lore_branch_list_args_t BranchArgs;
		FMemory::Memzero(&BranchArgs, sizeof(BranchArgs));
		BranchArgs.archived = (Op->bIncludeArchived ? 1 : 0);

		const ArcLoreApi::FLoreOpInvoker BranchInvoker =
			[&GlobalArgs, &BranchArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_branch_list(&GlobalArgs.Args(), &BranchArgs, Config);
			};

		// Dedupe local+remote entries of the SAME branch into one FArcLoreBranchInput.
		// Key = branch NAME: guaranteed shared across the local & remote entries of one
		// branch and unique among live branches, so it cannot false-merge. lore reports
		// each branch potentially twice — once with location LOCAL, once with REMOTE.
		// MERGE INVARIANTS (correctness-critical):
		//   - Id / Stack / bIsCurrent / TipHash(=local tip) come from the LOCAL entry.
		//   - A REMOTE entry contributes ONLY RemoteTipHash + bHasRemote, and must NEVER
		//     overwrite an existing local entry's Id / Stack / bIsCurrent / TipHash.
		//   - bIsCurrent is ALWAYS from the local entry (remote entries report is_current=0).
		//   - A remote-only branch (no local entry) takes Id/Stack/RemoteTipHash from the
		//     remote entry, bIsCurrent=false, and leaves TipHash empty.
		// NameToIndex maps branch name -> index into the worker's Branches array.
		TMap<FString, int32> NameToIndex;
		const ArcLoreApi::FLoreEventCollector BranchCollector =
			[this, &NameToIndex](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag != LORE_EVENT_BRANCH_LIST_ENTRY)
				{
					return;
				}

				const lore_branch_list_entry_event_data_t& EntryData = Event.branch_list_entry;
				const FString EntryName = ArcLoreApi::LoreStringToFString(EntryData.name);
				const FString EntryIdHex = ArcLoreBranchGraphInternal::BranchIdToHex(EntryData.id);
				const FString EntryTipHex = ArcLoreBranchGraphInternal::HashToHex(EntryData.latest);
				const bool bEntryRemote = (EntryData.location == LORE_BRANCH_LOCATION_REMOTE);

				int32* ExistingIndexPtr = NameToIndex.Find(EntryName);

				if (bEntryRemote)
				{
					// REMOTE entry: only ever contributes RemoteTipHash + bHasRemote.
					if (ExistingIndexPtr != nullptr)
					{
						// Existing (local arrived first): fill remote fields ONLY.
						// Do NOT touch Id / Stack / bIsCurrent / TipHash (invariant).
						FArcLoreBranchInput& Existing = Branches[*ExistingIndexPtr];
						Existing.RemoteTipHash = EntryTipHex;
						Existing.bHasRemote = true;
						return;
					}

					// Remote-only branch: seed from the remote entry, no local tip.
					FArcLoreBranchInput RemoteOnly;
					RemoteOnly.Name = EntryName;
					RemoteOnly.Id = EntryIdHex;
					RemoteOnly.RemoteTipHash = EntryTipHex;
					RemoteOnly.bHasRemote = true;
					RemoteOnly.bIsCurrent = false;
					// TipHash deliberately left empty (no local tip for a remote-only branch).

					const lore_branch_point_array_t& RemoteStack = EntryData.stack;
					RemoteOnly.Stack.Reserve(static_cast<int32>(RemoteStack.count));
					for (uintptr_t PointIndex = 0; PointIndex < RemoteStack.count; ++PointIndex)
					{
						const lore_branch_point_t& Point = RemoteStack.ptr[PointIndex];
						FArcLoreBranchPoint BranchPoint;
						BranchPoint.BranchIdHex = ArcLoreBranchGraphInternal::BranchIdToHex(Point.branch);
						BranchPoint.RevisionHash = ArcLoreBranchGraphInternal::HashToHex(Point.revision);
						RemoteOnly.Stack.Add(MoveTemp(BranchPoint));
					}

					const int32 NewIndex = Branches.Add(MoveTemp(RemoteOnly));
					NameToIndex.Add(EntryName, NewIndex);
					return;
				}

				// LOCAL entry: owns Id / Stack / bIsCurrent / TipHash(=local tip).
				if (ExistingIndexPtr != nullptr)
				{
					// Existing (remote arrived first): fill the LOCAL-owned fields onto it
					// WITHOUT clobbering RemoteTipHash / bHasRemote.
					FArcLoreBranchInput& Existing = Branches[*ExistingIndexPtr];
					Existing.Id = EntryIdHex;
					Existing.TipHash = EntryTipHex;
					Existing.bIsCurrent = (EntryData.is_current != 0);
					Existing.bArchived = (EntryData.archived != 0);

					const lore_branch_point_array_t& LocalStack = EntryData.stack;
					Existing.Stack.Reset(static_cast<int32>(LocalStack.count));
					for (uintptr_t PointIndex = 0; PointIndex < LocalStack.count; ++PointIndex)
					{
						const lore_branch_point_t& Point = LocalStack.ptr[PointIndex];
						FArcLoreBranchPoint BranchPoint;
						BranchPoint.BranchIdHex = ArcLoreBranchGraphInternal::BranchIdToHex(Point.branch);
						BranchPoint.RevisionHash = ArcLoreBranchGraphInternal::HashToHex(Point.revision);
						Existing.Stack.Add(MoveTemp(BranchPoint));
					}
					return;
				}

				// First (and so far only) entry for this name: a fresh local input.
				FArcLoreBranchInput BranchInput;
				BranchInput.Name = EntryName;
				BranchInput.Id = EntryIdHex;
				BranchInput.TipHash = EntryTipHex;
				BranchInput.bIsCurrent = (EntryData.is_current != 0);
				BranchInput.bArchived = (EntryData.archived != 0);

				// Copy the FULL fork lineage (stack of branch points). Each point's
				// branch is an opaque id; store its hex form as the label.
				const lore_branch_point_array_t& Stack = EntryData.stack;
				BranchInput.Stack.Reserve(static_cast<int32>(Stack.count));
				for (uintptr_t PointIndex = 0; PointIndex < Stack.count; ++PointIndex)
				{
					const lore_branch_point_t& Point = Stack.ptr[PointIndex];
					FArcLoreBranchPoint BranchPoint;
					BranchPoint.BranchIdHex = ArcLoreBranchGraphInternal::BranchIdToHex(Point.branch);
					BranchPoint.RevisionHash = ArcLoreBranchGraphInternal::HashToHex(Point.revision);
					BranchInput.Stack.Add(MoveTemp(BranchPoint));
				}

				const int32 NewIndex = Branches.Add(MoveTemp(BranchInput));
				NameToIndex.Add(EntryName, NewIndex);
			};

		FString BranchError;
		const int32 BranchResult = ArcLoreApi::RunLoreOp(BranchInvoker, BranchCollector, BranchError, TEXT("BranchGraph/branches"));
		if (BranchResult != 0)
		{
			// Hard: without the branch enumeration there is nothing to walk.
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_branch_list failed: %s"), *BranchError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// ---- PHASE 1b: SYNC STATE --------------------------------------------------
	// Tip-equality sync: a branch with no remote, or whose local tip equals its
	// remote tip, has nothing to push/pull at the tip.
	for (FArcLoreBranchInput& MergedBranch : Branches)
	{
		MergedBranch.bInSync = (!MergedBranch.bHasRemote) || (MergedBranch.TipHash == MergedBranch.RemoteTipHash);
	}

	// Exact ahead/behind for the CURRENT branch only. lore_repository_status reports
	// against the current branch's remote (revision_only=1 skips file diffs / fs scan);
	// mirror of FArcLoreCheckInWorker's post-commit re-query. Non-current branches keep
	// bAheadBehindKnown=false (the DAG can't see remote-ahead commits — no fake counts).
	{
		lore_repository_status_args_t StatusArgs;
		FMemory::Memzero(&StatusArgs, sizeof(StatusArgs));
		StatusArgs.revision_only = 1;

		bool bStatusRevisionSeen = false;
		bool bStatusLocalAhead = false;
		bool bStatusRemoteAhead = false;
		int64 StatusLocalNumber = 0;
		int64 StatusRemoteNumber = 0;

		const ArcLoreApi::FLoreOpInvoker StatusInvoker =
			[&GlobalArgs, &StatusArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_status(&GlobalArgs.Args(), &StatusArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StatusCollector =
			[&bStatusRevisionSeen, &bStatusLocalAhead, &bStatusRemoteAhead, &StatusLocalNumber, &StatusRemoteNumber]
			(const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REPOSITORY_STATUS_REVISION)
				{
					const lore_repository_status_revision_event_data_t& RevEvent = Event.repository_status_revision;
					bStatusRevisionSeen = true;
					bStatusLocalAhead = (RevEvent.is_local_ahead != 0);
					bStatusRemoteAhead = (RevEvent.is_remote_ahead != 0);
					StatusLocalNumber = static_cast<int64>(RevEvent.revision_local_number);
					StatusRemoteNumber = static_cast<int64>(RevEvent.revision_remote_number);
				}
			};

		FString StatusError;
		const int32 StatusResult = ArcLoreApi::RunLoreOp(StatusInvoker, StatusCollector, StatusError, TEXT("BranchGraph/status"));
		if (StatusResult != 0)
		{
			// Soft: branch pills still render with tip-equality sync; only the exact
			// current-branch ahead/behind is missing. Record and continue.
			InCommand.InfoMessages.Add(FString::Printf(
				TEXT("lore_repository_status failed (ahead/behind unavailable): %s"), *StatusError));
		}
		else if (bStatusRevisionSeen)
		{
			for (FArcLoreBranchInput& MergedBranch : Branches)
			{
				if (!MergedBranch.bIsCurrent)
				{
					continue;
				}
				MergedBranch.Ahead = (bStatusLocalAhead && StatusLocalNumber > StatusRemoteNumber)
					? static_cast<int32>(StatusLocalNumber - StatusRemoteNumber)
					: 0;
				MergedBranch.Behind = (bStatusRemoteAhead && StatusRemoteNumber > StatusLocalNumber)
					? static_cast<int32>(StatusRemoteNumber - StatusLocalNumber)
					: 0;
				MergedBranch.bAheadBehindKnown = true;
				break;
			}
		}
	}

	// Diagnostic: one line per merged branch. Confirms name-dedupe collapsed local+remote
	// into one node and surfaces the local/remote tips + sync state. Id is logged for both
	// passes' agreement: it's whichever pass last wrote it (local owns it when present), so
	// a mismatch vs the model's same-id claim would show here as a remote-only id.
	for (const FArcLoreBranchInput& MergedBranch : Branches)
	{
		UE_LOGF(LogSourceControl, Verbose,"[branchdiag] name='%ls' id=%ls localTip=%ls remoteTip=%ls hasRemote=%d inSync=%d ahead=%d behind=%d abKnown=%d current=%d",
			*MergedBranch.Name,
			*MergedBranch.Id,
			MergedBranch.TipHash.IsEmpty() ? TEXT("<none>") : *MergedBranch.TipHash,
			MergedBranch.RemoteTipHash.IsEmpty() ? TEXT("<none>") : *MergedBranch.RemoteTipHash,
			MergedBranch.bHasRemote ? 1 : 0,
			MergedBranch.bInSync ? 1 : 0,
			MergedBranch.Ahead,
			MergedBranch.Behind,
			MergedBranch.bAheadBehindKnown ? 1 : 0,
			MergedBranch.bIsCurrent ? 1 : 0);
	}

	// ---- PHASE 2: PER-BRANCH HISTORY (best-effort) -----------------------------
	// One lore_revision_history call per branch. Each call gets its own collector
	// state (CurrentIndex tracks the last-opened record within THIS branch's walk),
	// so trailing metadata never bleeds across branch calls. Failures are recorded
	// but do not fail the command.
	for (int32 BranchIndex = 0; BranchIndex < Branches.Num(); ++BranchIndex)
	{
		const FString BranchName = Branches[BranchIndex].Name;

		// NameBuffer MUST outlive HistoryInvoker — HistoryArgs.branch.View() points
		// into it across the lore_revision_history call.
		ArcLoreApi::FLoreUtf8String NameBuffer(BranchName);

		lore_revision_history_args_t HistoryArgs;
		FMemory::Memzero(&HistoryArgs, sizeof(HistoryArgs));
		HistoryArgs.revision = ArcLoreApi::EmptyLoreString();
		HistoryArgs.branch = NameBuffer.View();
		HistoryArgs.length = static_cast<uint32_t>(BranchHistoryLengthCap);
		HistoryArgs.only_branch = 0;

		const ArcLoreApi::FLoreOpInvoker HistoryInvoker =
			[&GlobalArgs, &HistoryArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_revision_history(&GlobalArgs.Args(), &HistoryArgs, Config);
			};

		// Index into Revisions of the record opened by the most recent
		// REVISION_HISTORY_ENTRY in THIS branch's walk; INDEX_NONE before any entry.
		// Reset per branch so metadata attaches only within this call.
		int32 CurrentIndex = INDEX_NONE;

		const ArcLoreApi::FLoreEventCollector HistoryCollector =
			[this, &CurrentIndex, &BranchName](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				switch (Tag)
				{
				case LORE_EVENT_REVISION_HISTORY_ENTRY:
					{
						const lore_revision_history_entry_event_data_t& Entry = Event.revision_history_entry;
						FArcLoreRevisionInput RevInput;
						RevInput.Hash = ArcLoreBranchGraphInternal::HashToHex(Entry.revision);
						RevInput.BranchName = BranchName;
						// parent[0] = direct parent; parent[1] = merge parent or zero.
						RevInput.ParentHash = ArcLoreBranchGraphInternal::IsZeroHash(Entry.parent[0])
							? FString()
							: ArcLoreBranchGraphInternal::HashToHex(Entry.parent[0]);
						RevInput.MergeParentHash = ArcLoreBranchGraphInternal::IsZeroHash(Entry.parent[1])
							? FString()
							: ArcLoreBranchGraphInternal::HashToHex(Entry.parent[1]);
						RevInput.RevisionNumber = static_cast<int32>(Entry.revision_number);
						CurrentIndex = Revisions.Add(MoveTemp(RevInput));
						break;
					}
				case LORE_EVENT_METADATA:
					{
						if (CurrentIndex == INDEX_NONE)
						{
							break;
						}
						FArcLoreRevisionInput& Current = Revisions[CurrentIndex];
						const lore_metadata_event_data_t& Meta = Event.metadata;
						const FString Key = ArcLoreApi::LoreStringToFString(Meta.key);
						if (Key == TEXT("message") && Meta.value.tag == LORE_METADATA_TYPE_STRING)
						{
							Current.Message = ArcLoreApi::LoreStringToFString(Meta.value.string);
						}
						else if (Key == TEXT("created-by") && Meta.value.tag == LORE_METADATA_TYPE_STRING)
						{
							Current.Author = ArcLoreApi::LoreStringToFString(Meta.value.string);
						}
						else if (Key == TEXT("timestamp") && Meta.value.tag == LORE_METADATA_TYPE_NUMERIC)
						{
							// Store RAW milliseconds — BuildLayout divides only for display.
							Current.TimestampMs = static_cast<int64>(Meta.value.numeric);
						}
						break;
					}
				default:
					break;
				}
			};

		FString HistoryError;
		const int32 HistoryResult = ArcLoreApi::RunLoreOp(HistoryInvoker, HistoryCollector, HistoryError,
			TEXT("BranchGraph/history"));
		if (HistoryResult != 0)
		{
			// Best-effort: record the failure and continue with the other branches.
			InCommand.InfoMessages.Add(FString::Printf(
				TEXT("lore_revision_history failed for branch '%s': %s"), *BranchName, *HistoryError));
		}
	}

	// Phase 1 succeeded (we returned early otherwise), so the command succeeds even if
	// some per-branch history walks failed.
	InCommand.bCommandSuccessful = true;
	return InCommand.bCommandSuccessful;
}

bool FArcLoreBranchGraphWorker::UpdateStates() const
{
	// Hand the collected branches/revisions to the operation payload (captured in
	// Execute, since UpdateStates takes no command). No StateCache change, so return
	// false (nothing to broadcast).
	if (Operation.IsValid())
	{
		TSharedRef<FLoreBranchGraph, ESPMode::ThreadSafe> Op = StaticCastSharedRef<FLoreBranchGraph>(Operation.ToSharedRef());
		Op->Branches = Branches;
		Op->Revisions = Revisions;
	}
	return false;
}

// ----------------------------------------------------------------------------
// FArcLoreListRemoteReposWorker
//
// REMOTE repo listing via lore_repository_list against the provider's remote URL.
// Blocks on network with no timeout — dispatched only behind an explicit user
// action (see panel). Execute runs on the POOL thread (worker-local scratch
// only); UpdateStates runs on the GAME thread and writes the captured operation.
// ----------------------------------------------------------------------------
FName FArcLoreListRemoteReposWorker::GetName() const
{
	return "LoreListRemoteRepos";
}

bool FArcLoreListRemoteReposWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// collected repos back to its payload on the game thread.
	Operation = InCommand.Operation;

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	// Resolve the remote URL from the provider (same accessor the Connect worker
	// uses for its log line). The UTF-8 buffer must outlive the lore_repository_list
	// call that consumes its view.
	const FString RemoteUrl = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore").GetProvider().GetRemoteUrl();
	ArcLoreApi::FLoreUtf8String UrlBuffer(RemoteUrl);

	// ---- REPO LIST op (REMOTE) -------------------------------------------------
	// Blocks on network with no timeout — dispatched only behind an explicit user
	// action (see panel).
	lore_repository_list_args_t RepoArgs;
	FMemory::Memzero(&RepoArgs, sizeof(RepoArgs));
	RepoArgs.url = UrlBuffer.View();

	const ArcLoreApi::FLoreOpInvoker RepoInvoker =
		[&GlobalArgs, &RepoArgs](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_repository_list(&GlobalArgs.Args(), &RepoArgs, Config);
		};

	const ArcLoreApi::FLoreEventCollector RepoCollector =
		[this](const lore_event_t& Event)
		{
			const uint32_t Tag = lore_event_type(&Event);
			if (Tag == LORE_EVENT_REPOSITORY_LIST_ENTRY)
			{
				// Surface only the name; the entry's id is an opaque struct.
				FArcLoreRepoEntry RepoEntry;
				RepoEntry.Name = ArcLoreApi::LoreStringToFString(Event.repository_list_entry.name);
				RemoteRepos.Add(MoveTemp(RepoEntry));
			}
		};

	FString RepoError;
	const int32 RepoResult = ArcLoreApi::RunLoreOp(RepoInvoker, RepoCollector, RepoError, TEXT("ListRemoteRepos"));

	// Soft: a remote read-only op. On failure record an info line but still return
	// the success bool so the command result reflects the actual outcome.
	InCommand.bCommandSuccessful = (RepoResult == 0);
	if (RepoResult != 0)
	{
		InCommand.InfoMessages.Add(FString::Printf(TEXT("lore_repository_list failed: %s"), *RepoError));
	}

	return InCommand.bCommandSuccessful;
}

bool FArcLoreListRemoteReposWorker::UpdateStates() const
{
	// Hand the collected repos to the operation payload (captured in Execute, since
	// UpdateStates takes no command). No StateCache change, so return false.
	if (Operation.IsValid())
	{
		StaticCastSharedRef<FLoreListRemoteRepos>(Operation.ToSharedRef())->RemoteRepos = RemoteRepos;
	}
	return false;
}

// ----------------------------------------------------------------------------
// FArcLoreListUnpushedWorker
//
// No-file op: enumerate the committed-but-unpushed local commits and the files each
// changed, GROUPED BY COMMIT. Reads under one FLoreGlobalArgsScope:
//
//   1) lore_repository_status (revision_only=1) — the status_revision event reports
//      is_local_ahead plus the remote/local tip revision NUMBERS. The remote number
//      is the history-walk termination bound: only revisions numbered strictly above
//      it are unpushed. revision_remote ZERO means the remote tip is unknown.
//   2) lore_revision_history (from the local tip, bounded length) — each
//      LORE_EVENT_REVISION_HISTORY_ENTRY opens a candidate commit; trailing METADATA
//      events (message / timestamp) attach to it. History is newest-first; we keep a
//      commit only while its revision_number > RemoteNumber and STOP including once a
//      number <= RemoteNumber is seen (everything older is already on the remote).
//   3) For each kept commit: lore_revision_info(revision = commit hash, delta=1) —
//      one REVISION_INFO_DELTA event per changed file (directories — flag_file==0 —
//      skipped). NOTE: a content-modified file arrives as a KEEP action (there is no
//      MODIFY action), so KEEP maps to Modified.
//
// When the branch is not ahead OR the remote tip is unknown, steps 2/3 are skipped and
// Commits stays empty — a valid result (nothing outgoing / remote unreachable). The
// history/info reads are soft: a non-zero result records an info line, never hard-fails.
//
// Each lore_revision_info is its own RunLoreOp under the SAME FLoreGlobalArgsScope,
// chained sequentially (the established multi-op-per-Execute pattern). Every hex buffer
// fed to an op MUST outlive that op's RunLoreOp call.
//
// Execute runs on the POOL thread (worker-local scratch only); UpdateStates runs on
// the GAME thread and is the only place the captured operation payload is touched.
// ----------------------------------------------------------------------------
namespace ArcLoreListUnpushedInternal
{
	// Render a lore_hash_t as a 64-character lowercase hex string. Mirrors the
	// file-scoped HashToHex in the RevisionLog worker (and InitFromHistory) so the
	// remote tip hex fed to lore_revision_diff matches Lore's own encoding exactly.
	FString HashToHex(const lore_hash_t& Hash)
	{
		FString Result;
		Result.Reserve(64);
		for (int32 ByteIndex = 0; ByteIndex < 32; ++ByteIndex)
		{
			Result += FString::Printf(TEXT("%02x"), Hash.data[ByteIndex]);
		}
		return Result;
	}

	// True when every one of the hash's 32 bytes is zero. A zero revision_remote means
	// the remote tip is unknown (remote unreachable / branch not on remote).
	bool IsZeroHash(const lore_hash_t& Hash)
	{
		for (int32 ByteIndex = 0; ByteIndex < 32; ++ByteIndex)
		{
			if (Hash.data[ByteIndex] != 0) { return false; }
		}
		return true;
	}

	// Map a diff file action to a working-copy state. There is no MODIFY action:
	// a content-modified file in a diff arrives as KEEP, so KEEP maps to Modified;
	// MOVE maps to Renamed; ADD/COPY introduce a path (Added); DELETE removes one (Deleted).
	EArcLoreState::Type MapDiffActionToState(lore_file_action_t Action)
	{
		switch (Action)
		{
		case LORE_FILE_ACTION_ADD:
		case LORE_FILE_ACTION_COPY:
			return EArcLoreState::Added;
		case LORE_FILE_ACTION_DELETE:
			return EArcLoreState::Deleted;
		case LORE_FILE_ACTION_MOVE:
			return EArcLoreState::Renamed;
		case LORE_FILE_ACTION_KEEP:
		default:
			return EArcLoreState::Modified;
		}
	}
}

FName FArcLoreListUnpushedWorker::GetName() const
{
	return "LoreListUnpushed";
}

bool FArcLoreListUnpushedWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// collected commits back to its payload on the game thread.
	Operation = InCommand.Operation;

	// Build the global args ONCE — the scope owns the UTF-8 buffers the lore_string_t
	// views point into, so it MUST outlive every RunLoreOp call below.
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	// Local tip hex captured by the status collector; declared here so it outlives the
	// status op and seeds the history walk below. Empty == local tip unknown (the walk
	// then starts from the current tip implicitly).
	FString LocalHex;

	// ---- 1) STATUS op (revision_only=1) — read ahead + revision numbers --------
	{
		lore_repository_status_args_t StatusArgs;
		FMemory::Memzero(&StatusArgs, sizeof(StatusArgs));
		// revision_only=1 skips file diffs and the filesystem scan — we only want the
		// status_revision event's ahead flag and the local/remote revision numbers.
		StatusArgs.revision_only = 1;

		// remote-known is a local flag: zero revision_remote means the remote tip is
		// unknown/unreachable, in which case we cannot bound the walk and report nothing.
		bool bRemoteKnown = false;

		const ArcLoreApi::FLoreOpInvoker StatusInvoker =
			[&GlobalArgs, &StatusArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_status(&GlobalArgs.Args(), &StatusArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StatusCollector =
			[this, &LocalHex, &bRemoteKnown](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REPOSITORY_STATUS_REVISION)
				{
					const lore_repository_status_revision_event_data_t& RevEvent = Event.repository_status_revision;
					bAhead = (RevEvent.is_local_ahead != 0);
					RemoteNumber = RevEvent.revision_remote_number;
					LocalNumber = RevEvent.revision_local_number;
					// revision_remote is ZERO when the remote tip is unknown/unreachable —
					// the bug-#1 lesson: never fold the working copy into "outgoing". Here
					// an unknown remote simply means no bound, so report nothing.
					bRemoteKnown = !ArcLoreListUnpushedInternal::IsZeroHash(RevEvent.revision_remote);
					// Local tip: the explicit history-walk start (see LocalHex declaration).
					LocalHex = ArcLoreListUnpushedInternal::IsZeroHash(RevEvent.revision_local)
						? FString()
						: ArcLoreListUnpushedInternal::HashToHex(RevEvent.revision_local);
				}
			};

		FString StatusError;
		const int32 StatusResult = ArcLoreApi::RunLoreOp(StatusInvoker, StatusCollector, StatusError, TEXT("Unpushed/status"));
		if (StatusResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_repository_status (revision) failed: %s"), *StatusError));
			InCommand.bCommandSuccessful = false;
			return false;
		}

		// ---- 2) Nothing outgoing / remote unknown — empty result is valid ------
		// Not ahead, or no known remote tip to bound the walk: Commits stays empty and
		// the command succeeds. The panel reads an empty list as "nothing to push".
		if (!bAhead || !bRemoteKnown)
		{
			InCommand.bCommandSuccessful = true;
			return true;
		}
	}

	// ---- 3a) HISTORY WALK — collect the unpushed commits (number > RemoteNumber) -
	// History is newest-first. We open a commit on each HISTORY_ENTRY and attach its
	// trailing METADATA (message / timestamp). A commit numbered above the remote tip
	// is unpushed and is kept; once an entry numbered <= RemoteNumber is seen, every
	// subsequent (older) entry is already on the remote, so we stop keeping commits.
	{
		// LocalHex (when known) MUST outlive the RunLoreOp call (its View() is stored
		// in the args), so declare its buffer BEFORE the invoker.
		ArcLoreApi::FLoreUtf8String LocalBuf(LocalHex);

		lore_revision_history_args_t HistoryArgs;
		FMemory::Memzero(&HistoryArgs, sizeof(HistoryArgs));
		HistoryArgs.revision = ArcLoreApi::EmptyLoreString();
		HistoryArgs.branch = ArcLoreApi::EmptyLoreString();
		// branch left empty => current branch; revision = local tip (empty => current
		// tip). length bounded — unpushed runs are short, 200 is ample headroom.
		if (!LocalHex.IsEmpty())
		{
			HistoryArgs.revision = LocalBuf.View();
		}
		HistoryArgs.length = 200;

		const ArcLoreApi::FLoreOpInvoker HistoryInvoker =
			[&GlobalArgs, &HistoryArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_revision_history(&GlobalArgs.Args(), &HistoryArgs, Config);
			};

		// bStopped latches once an entry at/below the remote tip number is seen; from
		// then on no further entries (or their metadata) are recorded.
		bool bStopped = false;
		const ArcLoreApi::FLoreEventCollector HistoryCollector =
			[this, &bStopped](const lore_event_t& Event)
			{
				if (bStopped)
				{
					return;
				}
				const uint32_t Tag = lore_event_type(&Event);
				switch (Tag)
				{
				case LORE_EVENT_REVISION_HISTORY_ENTRY:
					{
						const lore_revision_history_entry_event_data_t& Entry = Event.revision_history_entry;
						// At/below the remote tip number => already pushed; stop keeping.
						if (Entry.revision_number <= RemoteNumber)
						{
							bStopped = true;
							break;
						}
						FArcLoreUnpushedCommit Commit;
						Commit.RevisionHashHex = ArcLoreListUnpushedInternal::HashToHex(Entry.revision);
						Commit.RevisionNumber = static_cast<int32>(Entry.revision_number);
						Commits.Add(MoveTemp(Commit));
						break;
					}
				case LORE_EVENT_METADATA:
					{
						// Attach to the current (most recent) kept commit.
						if (Commits.Num() == 0)
						{
							break;
						}
						FArcLoreUnpushedCommit& Current = Commits.Last();
						const lore_metadata_event_data_t& Meta = Event.metadata;
						const FString Key = ArcLoreApi::LoreStringToFString(Meta.key);
						if (Key == TEXT("message") && Meta.value.tag == LORE_METADATA_TYPE_STRING)
						{
							Current.Message = ArcLoreApi::LoreStringToFString(Meta.value.string);
						}
						else if (Key == TEXT("timestamp") && Meta.value.tag == LORE_METADATA_TYPE_NUMERIC)
						{
							// timestamp is MILLISECONDS since the epoch — divide by 1000.
							Current.Date = FDateTime::FromUnixTimestamp(static_cast<int64>(Meta.value.numeric / 1000));
						}
						break;
					}
				default:
					break;
				}
			};

		FString HistoryError;
		const int32 HistoryResult = ArcLoreApi::RunLoreOp(HistoryInvoker, HistoryCollector, HistoryError, TEXT("Unpushed/history"));
		if (HistoryResult != 0)
		{
			// Soft: the status pre-pass already determined ahead. A failed walk leaves the
			// list incomplete; record an info line but do not hard-fail the command.
			InCommand.InfoMessages.Add(FString::Printf(TEXT("lore_revision_history failed (unpushed commit list incomplete): %s"), *HistoryError));
		}
	}

	// ---- 3b) PER-COMMIT FILE ENUMERATION — lore_revision_info(delta=1) ----------
	// For each kept commit, list the files it changed versus its parent. Each call is
	// its own RunLoreOp under the SAME GlobalArgs scope; the per-commit hex buffer must
	// outlive its RunLoreOp call (declared inside the loop body, before the invoker).
	for (FArcLoreUnpushedCommit& Commit : Commits)
	{
		ArcLoreApi::FLoreUtf8String RevBuf(Commit.RevisionHashHex);

		lore_revision_info_args_t InfoArgs;
		FMemory::Memzero(&InfoArgs, sizeof(InfoArgs));
		InfoArgs.revision = RevBuf.View();
		// delta=1 emits one REVISION_INFO_DELTA per changed file vs the parent;
		// metadata=0 suppresses the per-file metadata we do not need here.
		InfoArgs.delta = 1;
		InfoArgs.metadata = 0;

		const ArcLoreApi::FLoreOpInvoker InfoInvoker =
			[&GlobalArgs, &InfoArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_revision_info(&GlobalArgs.Args(), &InfoArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector InfoCollector =
			[&Commit](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REVISION_INFO_DELTA)
				{
					const lore_revision_info_delta_event_data_t& Delta = Event.revision_info_delta;
					// Skip directory entries; only real files belong in the list.
					if (Delta.flag_file == 0)
					{
						return;
					}
					// Skip unchanged passthrough: lore_revision_info(delta=1) emits a KEEP
					// entry for every file in the revision; only actually-changed files carry
					// flag_modify (or a structural ADD/DELETE/MOVE/COPY). Without this the
					// list shows the whole tree, not just what the commit changed.
					if (Delta.action == LORE_FILE_ACTION_KEEP && Delta.flag_modify == 0)
					{
						return;
					}
					FArcLoreUnpushedFile File;
					File.Path = ArcLoreApi::LoreStringToFString(Delta.path);
					File.Action = ArcLoreListUnpushedInternal::MapDiffActionToState(Delta.action);
					Commit.Files.Add(MoveTemp(File));
				}
			};

		FString InfoError;
		const int32 InfoResult = ArcLoreApi::RunLoreOp(InfoInvoker, InfoCollector, InfoError, TEXT("Unpushed/info"));
		if (InfoResult != 0)
		{
			// Soft: a failed per-commit read leaves that commit's file list empty;
			// record an info line but keep enumerating the rest.
			InCommand.InfoMessages.Add(FString::Printf(TEXT("lore_revision_info failed for %s (files incomplete): %s"), *Commit.RevisionHashHex, *InfoError));
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreListUnpushedWorker::UpdateStates() const
{
	// Hand the collected commits to the operation payload (captured in Execute, since
	// UpdateStates takes no command). No StateCache change, so return false (nothing
	// to broadcast).
	if (Operation.IsValid())
	{
		TSharedRef<FLoreListUnpushed, ESPMode::ThreadSafe> Op = StaticCastSharedRef<FLoreListUnpushed>(Operation.ToSharedRef());
		Op->Commits = Commits;
		Op->bLocalAhead = bAhead;
	}
	return false;
}

// ----------------------------------------------------------------------------
// FArcLoreListLocksWorker
//
// Branch-wide lock listing: one lore_lock_file_query (branch/owner/path filters all
// empty => every lock on the current branch) under one FLoreGlobalArgsScope. Mirrors
// the UpdateStatus lock-query pass, but as a standalone result-payload op for the
// panel's Locks tab. AUTH-GATED on Provider.IsAuthResolved() — when not logged in the
// query is skipped and the result is an empty list (the lock service is an
// authenticated endpoint). Soft: a failed query records an info line, never hard-fails.
// Execute runs on the POOL thread (worker-local scratch only); UpdateStates runs on the
// GAME thread and writes the captured operation.
// ----------------------------------------------------------------------------
FName FArcLoreListLocksWorker::GetName() const
{
	return "LoreListLocks";
}

bool FArcLoreListLocksWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// collected entries back to its payload on the game thread.
	Operation = InCommand.Operation;

	// AUTH GATE: the lock service is an authenticated endpoint that fails with a
	// transport error (~2.1s guaranteed-fail latency) until a Lore identity is resolved.
	// Skip silently when not logged in — an empty Locks list is a valid result.
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	if (!Module.GetProvider().IsAuthResolved())
	{
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [ListLocks] skipped (not logged in; auth unresolved)");
		InCommand.bCommandSuccessful = true;
		return true;
	}

	// Build the global args ONCE — the scope owns the UTF-8 buffers the lore_string_t
	// views point into, so it MUST outlive the RunLoreOp call below.
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	lore_lock_file_query_args_t LockArgs;
	FMemory::Memzero(&LockArgs, sizeof(LockArgs));
	LockArgs.branch = ArcLoreApi::EmptyLoreString();
	LockArgs.owner = ArcLoreApi::EmptyLoreString();
	LockArgs.path = ArcLoreApi::EmptyLoreString();
	// branch empty => current branch; owner empty => any owner; path empty => any path.

	const ArcLoreApi::FLoreOpInvoker LockInvoker =
		[&GlobalArgs, &LockArgs](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_lock_file_query(&GlobalArgs.Args(), &LockArgs, Config);
		};

	const ArcLoreApi::FLoreEventCollector LockCollector =
		[this](const lore_event_t& Event)
		{
			const uint32_t Tag = lore_event_type(&Event);
			if (Tag == LORE_EVENT_LOCK_FILE_QUERY)
			{
				const lore_lock_file_query_event_data_t& LockEvent = Event.lock_file_query;
				FArcLoreLockEntry Entry;
				// path is repo-relative; owner is a RAW user id (resolved to "me" by the
				// panel against the provider's user id). locked_at is in MILLISECONDS.
				Entry.Path = ArcLoreApi::LoreStringToFString(LockEvent.path);
				Entry.OwnerId = ArcLoreApi::LoreStringToFString(LockEvent.owner);
				Entry.LockedAt = FDateTime::FromUnixTimestamp(static_cast<int64>(LockEvent.locked_at / 1000));
				Entries.Add(MoveTemp(Entry));
			}
		};

	UE_LOGF(LogSourceControl, Verbose,"ArcLore: [ListLocks] branch-wide query (branch=current, owner=any, path=any); globals: %ls",
		*ArcLoreApi::DescribeGlobalArgs(GlobalArgs.Args()));

	FString LockError;
	const int32 LockResult = ArcLoreApi::RunLoreOp(LockInvoker, LockCollector, LockError, TEXT("ListLocks"));
	if (LockResult != 0)
	{
		// Soft: the lock query is best-effort. Record but do not fail the command.
		InCommand.InfoMessages.Add(FString::Printf(TEXT("Lore lock query unavailable: %s"), *LockError));
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreListLocksWorker::UpdateStates() const
{
	// Hand the collected entries to the operation payload (captured in Execute, since
	// UpdateStates takes no command). No StateCache change, so return false (nothing
	// to broadcast).
	if (Operation.IsValid())
	{
		TSharedRef<FLoreListLocks, ESPMode::ThreadSafe> Op = StaticCastSharedRef<FLoreListLocks>(Operation.ToSharedRef());
		Op->Locks = Entries;
	}
	return false;
}

// ----------------------------------------------------------------------------
// FArcLoreRevisionFilesWorker
//
// List the files changed in a single revision versus its parent via
// lore_revision_info (delta=1, metadata=0). One read under one
// FLoreGlobalArgsScope: each LORE_EVENT_REVISION_INFO_DELTA event carries one
// changed path; directory entries (flag_file==0) are skipped. The revision hex is
// read from the captured FLoreRevisionFiles::Revision; an empty revision yields an
// empty result. Execute runs on the POOL thread (worker-local scratch only);
// UpdateStates runs on the GAME thread and writes the captured operation.
// ----------------------------------------------------------------------------
namespace ArcLoreRevisionFilesInternal
{
	// Map a delta file action to a working-copy state. Mirrors the diff mapping:
	// there is no MODIFY action — a content-modified file arrives as KEEP, so KEEP
	// maps to Modified; MOVE maps to Renamed; ADD/COPY introduce a path (Added);
	// DELETE removes one (Deleted).
	EArcLoreState::Type MapDeltaActionToState(lore_file_action_t Action)
	{
		switch (Action)
		{
		case LORE_FILE_ACTION_ADD:
		case LORE_FILE_ACTION_COPY:
			return EArcLoreState::Added;
		case LORE_FILE_ACTION_DELETE:
			return EArcLoreState::Deleted;
		case LORE_FILE_ACTION_MOVE:
			return EArcLoreState::Renamed;
		case LORE_FILE_ACTION_KEEP:
		default:
			return EArcLoreState::Modified;
		}
	}
}

FName FArcLoreRevisionFilesWorker::GetName() const
{
	return "LoreRevisionFiles";
}

bool FArcLoreRevisionFilesWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// collected entries back to its payload on the game thread.
	Operation = InCommand.Operation;

	// Read the input revision from the captured operation payload.
	const FString Revision = StaticCastSharedRef<FLoreRevisionFiles>(InCommand.Operation)->Revision;

	// Empty revision: nothing to list — a valid, empty result.
	if (Revision.IsEmpty())
	{
		InCommand.bCommandSuccessful = true;
		return true;
	}

	// Build the global args ONCE — the scope owns the UTF-8 buffers the lore_string_t
	// views point into, so it MUST outlive the RunLoreOp call below.
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	// The revision hex buffer MUST outlive the RunLoreOp call below (its View() is
	// stored in the args), so declare it BEFORE the invoker.
	ArcLoreApi::FLoreUtf8String RevBuf(Revision);

	lore_revision_info_args_t Args;
	FMemory::Memzero(&Args, sizeof(Args));
	Args.revision = RevBuf.View();
	// delta=1 emits one REVISION_INFO_DELTA event per file changed vs the parent;
	// metadata=0 suppresses the per-file metadata entries we do not need here.
	Args.delta = 1;
	Args.metadata = 0;

	const ArcLoreApi::FLoreOpInvoker Invoker =
		[&GlobalArgs, &Args](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_revision_info(&GlobalArgs.Args(), &Args, Config);
		};

	const ArcLoreApi::FLoreEventCollector Collector =
		[this](const lore_event_t& Event)
		{
			const uint32_t Tag = lore_event_type(&Event);
			if (Tag == LORE_EVENT_REVISION_INFO_DELTA)
			{
				const lore_revision_info_delta_event_data_t& Delta = Event.revision_info_delta;
				// Skip directory entries; only real files belong in the list.
				if (Delta.flag_file == 0)
				{
					return;
				}
				// Skip unchanged passthrough: lore_revision_info(delta=1) emits a KEEP entry
				// for every file in the revision; only actually-changed files carry
				// flag_modify (or a structural ADD/DELETE/MOVE/COPY). Without this the
				// revision's file list shows the whole tree, not just what it changed.
				if (Delta.action == LORE_FILE_ACTION_KEEP && Delta.flag_modify == 0)
				{
					return;
				}
				FArcLoreUnpushedFile File;
				File.Path = ArcLoreApi::LoreStringToFString(Delta.path);
				File.Action = ArcLoreRevisionFilesInternal::MapDeltaActionToState(Delta.action);
				Entries.Add(MoveTemp(File));
			}
		};

	FString Error;
	const int32 Result = ArcLoreApi::RunLoreOp(Invoker, Collector, Error, TEXT("RevisionFiles"));
	if (Result != 0)
	{
		// Soft: a local read-only op. Record an info line but still succeed so the
		// panel simply shows an empty (or partial) list rather than erroring.
		InCommand.InfoMessages.Add(FString::Printf(TEXT("lore_revision_info failed: %s"), *Error));
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreRevisionFilesWorker::UpdateStates() const
{
	// Hand the collected entries to the operation payload (captured in Execute, since
	// UpdateStates takes no command). No StateCache change, so return false (nothing
	// to broadcast).
	if (Operation.IsValid())
	{
		StaticCastSharedRef<FLoreRevisionFiles>(Operation.ToSharedRef())->Files = Entries;
	}
	return false;
}

// ----------------------------------------------------------------------------
// Unpushed-history rewrite ops: FLoreUndoLastCommit + FLoreRemoveFromOutgoing.
//
// Both ops rewrite UNPUSHED local history (move the branch tip with
// lore_branch_reset, re-commit). They MUST fail safe: the panel's cached
// IsLocalAhead() is only refreshed on explicit Refresh/CheckIn/Push and can be
// stale, so each worker re-runs lore_repository_status(revision_only=1) at op time
// and refuses unless the tip is STRICTLY ahead of the remote (VerifyAheadOrFail).
// Resetting a commit that is actually on the remote would diverge the branch.
//
// File-scoped helpers (unique namespace per the project's free-function rule):
//   HashToHex / IsZeroHash — copy of the 32-byte loops used by the other workers
//     (file-scoped there, not cross-TU visible).
//   Abs                    — repo-relative -> absolute via FPaths::Combine (the
//     path-bearing ops file_reset/file_dirty/file_stage need ABSOLUTE paths).
//   VerifyAheadOrFail      — the server-side ahead-gate (S1).
//
// Execute + collectors run on the POOL thread (worker-local scratch only);
// UpdateStates runs on the GAME thread and is the only place the operation payload
// is touched.
// ----------------------------------------------------------------------------
namespace ArcLoreHistoryRewriteInternal
{
	// Render a lore_hash_t as a 64-character lowercase hex string. Mirrors the
	// file-scoped HashToHex in the RevisionLog/ListUnpushed workers so a hash fed to
	// lore_branch_reset matches Lore's own encoding exactly.
	FString HashToHex(const lore_hash_t& Hash)
	{
		FString Result;
		Result.Reserve(64);
		for (int32 ByteIndex = 0; ByteIndex < 32; ++ByteIndex)
		{
			Result += FString::Printf(TEXT("%02x"), Hash.data[ByteIndex]);
		}
		return Result;
	}

	// True when every one of the hash's 32 bytes is zero. A zero parent means the
	// revision is a root commit (no parent to reset to).
	bool IsZeroHash(const lore_hash_t& Hash)
	{
		for (int32 ByteIndex = 0; ByteIndex < 32; ++ByteIndex)
		{
			if (Hash.data[ByteIndex] != 0) { return false; }
		}
		return true;
	}

	// repo-relative -> absolute (the path-bearing ops resolve a RELATIVE path against
	// the process working dir, not the repo root — see the CheckIn worker's note — so
	// every path fed to file_reset/file_dirty/file_stage MUST be absolute).
	FString Abs(const FString& RepoRoot, const FString& Rel)
	{
		return FPaths::Combine(RepoRoot, Rel);
	}

	// Case-insensitive / slash-normalized compare of two repo-relative paths. A MOVE
	// emits the new-side path and slash/case may differ, so never use raw TArray::Contains.
	bool SamePath(const FString& A, const FString& B)
	{
		return FPaths::IsSamePath(A, B) || A.Equals(B, ESearchCase::IgnoreCase);
	}

	// S1: re-verify the local branch tip is STRICTLY ahead of the remote, server-side,
	// before any destructive branch_reset. Returns true only when is_local_ahead != 0
	// AND revision_local_number > revision_remote_number. On any other outcome (not
	// ahead, equal/behind numbers, status op failure) sets OutError and returns false.
	// Never trusts the cached panel flag.
	bool VerifyAheadOrFail(ArcLoreApi::FLoreGlobalArgsScope& GlobalArgs, FString& OutError)
	{
		lore_repository_status_args_t StatusArgs;
		FMemory::Memzero(&StatusArgs, sizeof(StatusArgs));
		// revision_only=1 skips file diffs and the filesystem scan — we only want the
		// status_revision event's ahead flag and the local/remote revision numbers.
		StatusArgs.revision_only = 1;

		bool bSawRevision = false;
		bool bLocalAhead = false;
		uint64 LocalNumber = 0;
		uint64 RemoteNumber = 0;

		const ArcLoreApi::FLoreOpInvoker StatusInvoker =
			[&GlobalArgs, &StatusArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_repository_status(&GlobalArgs.Args(), &StatusArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector StatusCollector =
			[&bSawRevision, &bLocalAhead, &LocalNumber, &RemoteNumber](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REPOSITORY_STATUS_REVISION)
				{
					const lore_repository_status_revision_event_data_t& RevEvent = Event.repository_status_revision;
					bSawRevision = true;
					bLocalAhead = (RevEvent.is_local_ahead != 0);
					LocalNumber = RevEvent.revision_local_number;
					RemoteNumber = RevEvent.revision_remote_number;
				}
			};

		FString StatusError;
		const int32 StatusResult = ArcLoreApi::RunLoreOp(StatusInvoker, StatusCollector, StatusError, TEXT("HistoryRewrite/ahead-gate"));
		if (StatusResult != 0)
		{
			OutError = FString::Printf(TEXT("ahead-gate status query failed; refusing the history rewrite: %s"), *StatusError);
			return false;
		}
		if (!bSawRevision)
		{
			OutError = TEXT("ahead-gate status query returned no revision info; refusing the history rewrite.");
			return false;
		}
		// Gate on the revision NUMBERS alone: a commit is unpushed (safe to reset) when
		// the local branch holds more revisions than the remote tip. is_local_ahead is
		// unreliable in a "diverged" state (reads 0 there) and wrongly blocked a safe
		// local-only reset, so it is captured + logged for diagnostics but NOT required.
		if (!(LocalNumber > RemoteNumber))
		{
			OutError = FString::Printf(
				TEXT("the local tip is not ahead of the remote (local=%llu remote=%llu, is_local_ahead=%d); refusing to reset a pushed commit."),
				static_cast<unsigned long long>(LocalNumber),
				static_cast<unsigned long long>(RemoteNumber),
				bLocalAhead ? 1 : 0);
			return false;
		}
		return true;
	}
}

// ----------------------------------------------------------------------------
// FArcLoreUndoLastCommitWorker
// ----------------------------------------------------------------------------
FName FArcLoreUndoLastCommitWorker::GetName() const
{
	return "LoreUndoLastCommit";
}

bool FArcLoreUndoLastCommitWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Build the global args ONCE — the scope owns the UTF-8 buffers the lore_string_t
	// views point into, so it MUST outlive every RunLoreOp call below.
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	// ---- 0) AHEAD-GATE (S1) — refuse unless the tip is strictly ahead ----------
	{
		FString AheadError;
		if (!ArcLoreHistoryRewriteInternal::VerifyAheadOrFail(GlobalArgs, AheadError))
		{
			InCommand.ErrorMessages.Add(AheadError);
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// ---- 1) Read the current tip's direct parent (delta/metadata off) ----------
	lore_hash_t ParentHash;
	FMemory::Memzero(&ParentHash, sizeof(ParentHash));
	bool bSawInfo = false;
	{
		lore_revision_info_args_t InfoArgs;
		FMemory::Memzero(&InfoArgs, sizeof(InfoArgs));
		InfoArgs.revision = ArcLoreApi::EmptyLoreString();
		// revision empty == current tip; delta/metadata=0 — we only want parent[0].
		InfoArgs.delta = 0;
		InfoArgs.metadata = 0;

		const ArcLoreApi::FLoreOpInvoker InfoInvoker =
			[&GlobalArgs, &InfoArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_revision_info(&GlobalArgs.Args(), &InfoArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector InfoCollector =
			[&ParentHash, &bSawInfo](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REVISION_INFO)
				{
					bSawInfo = true;
					ParentHash = Event.revision_info.parent[0];
				}
			};

		FString InfoError;
		const int32 InfoResult = ArcLoreApi::RunLoreOp(InfoInvoker, InfoCollector, InfoError, TEXT("UndoLastCommit/info"));
		if (InfoResult != 0 || !bSawInfo)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_revision_info failed (nothing reset): %s"), *InfoError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	if (ArcLoreHistoryRewriteInternal::IsZeroHash(ParentHash))
	{
		InCommand.ErrorMessages.Add(TEXT("the latest commit has no parent (root commit); cannot undo."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// ---- 2) Reset the local branch tip to the parent --------------------------
	{
		// The parent hex buffer MUST outlive the RunLoreOp call (its View() is stored
		// in the args), so declare it BEFORE the invoker.
		ArcLoreApi::FLoreUtf8String ParentBuf(ArcLoreHistoryRewriteInternal::HashToHex(ParentHash));

		lore_branch_reset_args_t ResetArgs;
		FMemory::Memzero(&ResetArgs, sizeof(ResetArgs));
		ResetArgs.branch = ArcLoreApi::EmptyLoreString();
		ResetArgs.revision = ParentBuf.View();
		// branch left empty == current branch.

		const ArcLoreApi::FLoreOpInvoker ResetInvoker =
			[&GlobalArgs, &ResetArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_branch_reset(&GlobalArgs.Args(), &ResetArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector ResetCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString ResetError;
		const int32 ResetResult = ArcLoreApi::RunLoreOp(ResetInvoker, ResetCollector, ResetError, TEXT("UndoLastCommit/reset"));
		if (ResetResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_branch_reset failed (nothing reset): %s"), *ResetError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreUndoLastCommitWorker::UpdateStates() const
{
	// No payload to write back; the panel's follow-up Refresh re-reads the working set
	// (the reset commit's files are now dirty/staged again). Nothing to broadcast here.
	return false;
}

// ----------------------------------------------------------------------------
// FArcLoreAmendLastCommitWorker
// ----------------------------------------------------------------------------
FName FArcLoreAmendLastCommitWorker::GetName() const
{
	return "LoreAmendLastCommit";
}

bool FArcLoreAmendLastCommitWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Build the global args ONCE — the scope owns the UTF-8 buffers the lore_string_t
	// views point into, so it MUST outlive every RunLoreOp call below.
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	// ---- 0) AHEAD-GATE (S1) — refuse unless the tip is strictly ahead ----------
	// Amend rewrites the tip to a NEW hash; amending a pushed commit would diverge the
	// branch, so only an unpushed tip may be amended.
	{
		FString AheadError;
		if (!ArcLoreHistoryRewriteInternal::VerifyAheadOrFail(GlobalArgs, AheadError))
		{
			InCommand.ErrorMessages.Add(AheadError);
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// ---- 1) Read + validate the new message ------------------------------------
	const FString NewMsg = StaticCastSharedRef<FLoreAmendLastCommit>(InCommand.Operation)->NewMessage;
	if (NewMsg.TrimStartAndEnd().IsEmpty())
	{
		InCommand.ErrorMessages.Add(TEXT("the amend message is empty; refusing to amend the commit message."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// ---- 2) Amend the latest commit's message ----------------------------------
	{
		// The message buffer MUST outlive the RunLoreOp call (its View() is stored in
		// the args), so declare it BEFORE the invoker.
		ArcLoreApi::FLoreUtf8String MsgBuf(NewMsg);

		lore_revision_amend_args_t Args;
		FMemory::Memzero(&Args, sizeof(Args));
		Args.message = MsgBuf.View();

		// Capture the amended (new) tip hash for a diagnostic log — the amend produces a
		// new revision hash (history rewrite).
		FString AmendedRevisionHex;

		const ArcLoreApi::FLoreOpInvoker AmendInvoker =
			[&GlobalArgs, &Args](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_revision_amend(&GlobalArgs.Args(), &Args, Config);
			};

		const ArcLoreApi::FLoreEventCollector AmendCollector =
			[&AmendedRevisionHex](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REVISION_COMMIT_REVISION)
				{
					AmendedRevisionHex = ArcLoreCheckInInternal::HashToHexPrefix(Event.revision_commit_revision.revision);
				}
			};

		FString AmendError;
		const int32 AmendResult = ArcLoreApi::RunLoreOp(AmendInvoker, AmendCollector, AmendError, TEXT("AmendLastCommit/amend"));
		if (AmendResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_revision_amend failed (message unchanged): %s"), *AmendError));
			InCommand.bCommandSuccessful = false;
			return false;
		}

		if (!AmendedRevisionHex.IsEmpty())
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: amended last local commit message; new tip %ls", *AmendedRevisionHex);
		}
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreAmendLastCommitWorker::UpdateStates() const
{
	// No payload to write back; the panel's follow-up Refresh re-reads the amended
	// message (History/Outgoing). Nothing to broadcast here.
	return false;
}

// ----------------------------------------------------------------------------
// FArcLoreRemoveFromOutgoingWorker
// ----------------------------------------------------------------------------
FName FArcLoreRemoveFromOutgoingWorker::GetName() const
{
	return "LoreRemoveFromOutgoing";
}

bool FArcLoreRemoveFromOutgoingWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write
	// bRemoved/Note back to its payload on the game thread.
	Operation = InCommand.Operation;

	// Read the input target (repo-relative) from the captured operation payload.
	const FString TargetRel = StaticCastSharedRef<FLoreRemoveFromOutgoing>(InCommand.Operation)->FilePath;

	const FString& RepoRoot = InCommand.PathToRepositoryRoot;

	// Build the global args ONCE — the scope owns the UTF-8 buffers the lore_string_t
	// views point into, so it MUST outlive every RunLoreOp call below.
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, InCommand.Identity);

	// ---- 0) AHEAD-GATE (S1) — refuse unless the tip is strictly ahead ----------
	{
		FString AheadError;
		if (!ArcLoreHistoryRewriteInternal::VerifyAheadOrFail(GlobalArgs, AheadError))
		{
			InCommand.ErrorMessages.Add(AheadError);
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// ---- 1) Read the latest commit's delta + parent + message ------------------
	lore_hash_t ParentHash;
	FMemory::Memzero(&ParentHash, sizeof(ParentHash));
	bool bSawInfo = false;
	TArray<FString> DeltaRel;
	FString Message;
	{
		lore_revision_info_args_t InfoArgs;
		FMemory::Memzero(&InfoArgs, sizeof(InfoArgs));
		InfoArgs.revision = ArcLoreApi::EmptyLoreString();
		// revision empty == current tip; delta=1 emits one REVISION_INFO_DELTA per
		// changed file; metadata=1 emits the "message" key.
		InfoArgs.delta = 1;
		InfoArgs.metadata = 1;

		const ArcLoreApi::FLoreOpInvoker InfoInvoker =
			[&GlobalArgs, &InfoArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_revision_info(&GlobalArgs.Args(), &InfoArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector InfoCollector =
			[&ParentHash, &bSawInfo, &DeltaRel, &Message](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_REVISION_INFO)
				{
					bSawInfo = true;
					ParentHash = Event.revision_info.parent[0];
				}
				else if (Tag == LORE_EVENT_REVISION_INFO_DELTA)
				{
					const lore_revision_info_delta_event_data_t& Delta = Event.revision_info_delta;
					// Only real files belong in the delta set (skip directory entries).
					if (Delta.flag_file != 0)
					{
						DeltaRel.Add(ArcLoreApi::LoreStringToFString(Delta.path));
					}
				}
				else if (Tag == LORE_EVENT_METADATA)
				{
					const lore_metadata_event_data_t& Meta = Event.metadata;
					const FString Key = ArcLoreApi::LoreStringToFString(Meta.key);
					if (Key == TEXT("message") && Meta.value.tag == LORE_METADATA_TYPE_STRING)
					{
						Message = ArcLoreApi::LoreStringToFString(Meta.value.string);
					}
				}
			};

		FString InfoError;
		const int32 InfoResult = ArcLoreApi::RunLoreOp(InfoInvoker, InfoCollector, InfoError, TEXT("RemoveFromOutgoing/info"));
		if (InfoResult != 0 || !bSawInfo)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_revision_info failed (nothing changed): %s"), *InfoError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// ---- 2) Guard: target must be in the LATEST commit's delta (normalized) ----
	bool bTargetInDelta = false;
	for (const FString& Rel : DeltaRel)
	{
		if (ArcLoreHistoryRewriteInternal::SamePath(Rel, TargetRel))
		{
			bTargetInDelta = true;
			break;
		}
	}
	if (!bTargetInDelta)
	{
		// Valid "can't do that here": the file is in an OLDER unpushed commit (rebase
		// territory Lore does not expose) or not unpushed at all. Soft success.
		Note = TEXT("not in the latest unpushed commit");
		bRemoved = false;
		InCommand.bCommandSuccessful = true;
		return true;
	}

	if (ArcLoreHistoryRewriteInternal::IsZeroHash(ParentHash))
	{
		InCommand.ErrorMessages.Add(TEXT("the latest commit has no parent (root commit); cannot remove a file from it."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// ---- 3) Reset the branch tip to the parent --------------------------------
	// Up to here NOTHING is mutated; a reset failure is a clean abort (no rollback msg).
	{
		ArcLoreApi::FLoreUtf8String ParentBuf(ArcLoreHistoryRewriteInternal::HashToHex(ParentHash));

		lore_branch_reset_args_t ResetArgs;
		FMemory::Memzero(&ResetArgs, sizeof(ResetArgs));
		ResetArgs.branch = ArcLoreApi::EmptyLoreString();
		ResetArgs.revision = ParentBuf.View();

		const ArcLoreApi::FLoreOpInvoker ResetInvoker =
			[&GlobalArgs, &ResetArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_branch_reset(&GlobalArgs.Args(), &ResetArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector ResetCollector =
			[](const lore_event_t& /*Event*/)
			{
			};

		FString ResetError;
		const int32 ResetResult = ArcLoreApi::RunLoreOp(ResetInvoker, ResetCollector, ResetError, TEXT("RemoveFromOutgoing/reset"));
		if (ResetResult != 0)
		{
			InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_branch_reset failed (nothing changed): %s"), *ResetError));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// From here on the branch tip HAS moved to the parent. Any further failure leaves
	// the commit's files uncommitted on disk → a HARD failure with a recovery message.
	bool bResetFileSeenForTarget = false;
	{
		// ---- 4) Discard the target file against the new tip --------------------
		const FString TargetAbs = ArcLoreHistoryRewriteInternal::Abs(RepoRoot, TargetRel);
		ArcLoreApi::FLoreUtf8String TargetBuf(TargetAbs);
		lore_string_t TargetView = TargetBuf.View();

		lore_file_reset_args_t ResetFileArgs;
		FMemory::Memzero(&ResetFileArgs, sizeof(ResetFileArgs));
		ResetFileArgs.revision = ArcLoreApi::EmptyLoreString();
		ResetFileArgs.paths.ptr = &TargetView;
		ResetFileArgs.paths.count = 1;
		// revision empty == the new tip (parent), so resetting discards the change.
		ResetFileArgs.purge = 0;

		const ArcLoreApi::FLoreOpInvoker ResetFileInvoker =
			[&GlobalArgs, &ResetFileArgs](const lore_event_callback_config_t& Config) -> int32
			{
				return lore_file_reset(&GlobalArgs.Args(), &ResetFileArgs, Config);
			};

		const ArcLoreApi::FLoreEventCollector ResetFileCollector =
			[&bResetFileSeenForTarget, &TargetRel](const lore_event_t& Event)
			{
				const uint32_t Tag = lore_event_type(&Event);
				if (Tag == LORE_EVENT_FILE_RESET_FILE)
				{
					const FString ResetRel = ArcLoreApi::LoreStringToFString(Event.file_reset_file.path);
					if (ArcLoreHistoryRewriteInternal::SamePath(ResetRel, TargetRel))
					{
						bResetFileSeenForTarget = true;
					}
				}
			};

		FString ResetFileError;
		const int32 ResetFileResult = ArcLoreApi::RunLoreOp(ResetFileInvoker, ResetFileCollector, ResetFileError, TEXT("RemoveFromOutgoing/file-reset"));

		// ---- 5) Re-stage the remaining files and re-commit ---------------------
		TArray<FString> Remaining;
		for (const FString& Rel : DeltaRel)
		{
			if (!ArcLoreHistoryRewriteInternal::SamePath(Rel, TargetRel))
			{
				Remaining.Add(Rel);
			}
		}

		bool bRecommitted = false;
		int32 DirtyResult = 0;
		int32 StageResult = 0;
		int32 CommitResult = 0;
		if (Remaining.Num() > 0)
		{
			// Build the ABSOLUTE path list for the dirty + stage ops (shared buffers).
			TArray<ArcLoreApi::FLoreUtf8String> PathBuffers;
			PathBuffers.Reserve(Remaining.Num());
			TArray<lore_string_t> PathViews;
			PathViews.Reserve(Remaining.Num());
			for (const FString& Rel : Remaining)
			{
				const int32 BufIndex = PathBuffers.Emplace(ArcLoreHistoryRewriteInternal::Abs(RepoRoot, Rel));
				PathViews.Add(PathBuffers[BufIndex].View());
			}

			// ---- 5a) DIRTY ----------------------------------------------------
			{
				lore_file_dirty_args_t DirtyArgs;
				FMemory::Memzero(&DirtyArgs, sizeof(DirtyArgs));
				DirtyArgs.paths.ptr = PathViews.GetData();
				DirtyArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());

				const ArcLoreApi::FLoreOpInvoker DirtyInvoker =
					[&GlobalArgs, &DirtyArgs](const lore_event_callback_config_t& Config) -> int32
					{
						return lore_file_dirty(&GlobalArgs.Args(), &DirtyArgs, Config);
					};

				const ArcLoreApi::FLoreEventCollector DirtyCollector =
					[](const lore_event_t& /*Event*/)
					{
					};

				FString DirtyError;
				DirtyResult = ArcLoreApi::RunLoreOp(DirtyInvoker, DirtyCollector, DirtyError, TEXT("RemoveFromOutgoing/dirty"));
			}

			// ---- 5b) STAGE (scan=1) — a non-scanned restage leaves a non- ------
			// persistent stage for a modified tracked file ("Nothing staged" trap).
			{
				lore_file_stage_args_t StageArgs;
				FMemory::Memzero(&StageArgs, sizeof(StageArgs));
				StageArgs.paths.ptr = PathViews.GetData();
				StageArgs.paths.count = static_cast<uintptr_t>(PathViews.Num());
				StageArgs.scan = 1;
				StageArgs.case_change = 0;

				const ArcLoreApi::FLoreOpInvoker StageInvoker =
					[&GlobalArgs, &StageArgs](const lore_event_callback_config_t& Config) -> int32
					{
						return lore_file_stage(&GlobalArgs.Args(), &StageArgs, Config);
					};

				const ArcLoreApi::FLoreEventCollector StageCollector =
					[](const lore_event_t& /*Event*/)
					{
					};

				FString StageError;
				StageResult = ArcLoreApi::RunLoreOp(StageInvoker, StageCollector, StageError, TEXT("RemoveFromOutgoing/stage"));
			}

			// ---- 5c) COMMIT (original message) --------------------------------
			{
				ArcLoreApi::FLoreUtf8String MessageBuffer(Message);

				lore_revision_commit_args_t CommitArgs;
				FMemory::Memzero(&CommitArgs, sizeof(CommitArgs));
				CommitArgs.link = ArcLoreApi::EmptyLoreString();
				CommitArgs.layer = ArcLoreApi::EmptyLoreString();
				CommitArgs.message = MessageBuffer.View();

				const ArcLoreApi::FLoreOpInvoker CommitInvoker =
					[&GlobalArgs, &CommitArgs](const lore_event_callback_config_t& Config) -> int32
					{
						return lore_revision_commit(&GlobalArgs.Args(), &CommitArgs, Config);
					};

				const ArcLoreApi::FLoreEventCollector CommitCollector =
					[](const lore_event_t& /*Event*/)
					{
					};

				FString CommitError;
				CommitResult = ArcLoreApi::RunLoreOp(CommitInvoker, CommitCollector, CommitError, TEXT("RemoveFromOutgoing/commit"));
			}

			bRecommitted = true;
		}

		// ---- 6) POST-COMMIT ASSERTION (B2/B3 safety net) -----------------------
		// When a recommit happened, re-read the NEW tip's delta and confirm it
		// EXCLUDES the target and EQUALS Remaining (same normalized compare). Any
		// mismatch / nonzero sub-step / missing reset-file event is a HARD failure:
		// the branch is already at the parent and the files are uncommitted on disk.
		bool bAssertionPassed = true;
		if (bRecommitted)
		{
			TArray<FString> NewDelta;
			{
				lore_revision_info_args_t VerifyArgs;
				FMemory::Memzero(&VerifyArgs, sizeof(VerifyArgs));
				VerifyArgs.revision = ArcLoreApi::EmptyLoreString();
				VerifyArgs.delta = 1;
				VerifyArgs.metadata = 0;

				const ArcLoreApi::FLoreOpInvoker VerifyInvoker =
					[&GlobalArgs, &VerifyArgs](const lore_event_callback_config_t& Config) -> int32
					{
						return lore_revision_info(&GlobalArgs.Args(), &VerifyArgs, Config);
					};

				const ArcLoreApi::FLoreEventCollector VerifyCollector =
					[&NewDelta](const lore_event_t& Event)
					{
						const uint32_t Tag = lore_event_type(&Event);
						if (Tag == LORE_EVENT_REVISION_INFO_DELTA)
						{
							const lore_revision_info_delta_event_data_t& Delta = Event.revision_info_delta;
							if (Delta.flag_file != 0)
							{
								NewDelta.Add(ArcLoreApi::LoreStringToFString(Delta.path));
							}
						}
					};

				FString VerifyError;
				const int32 VerifyResult = ArcLoreApi::RunLoreOp(VerifyInvoker, VerifyCollector, VerifyError, TEXT("RemoveFromOutgoing/verify"));
				if (VerifyResult != 0)
				{
					bAssertionPassed = false;
				}
			}

			// New tip's delta must exclude the target...
			for (const FString& Rel : NewDelta)
			{
				if (ArcLoreHistoryRewriteInternal::SamePath(Rel, TargetRel))
				{
					bAssertionPassed = false;
					break;
				}
			}
			// ...and equal Remaining (same count + every Remaining present).
			if (NewDelta.Num() != Remaining.Num())
			{
				bAssertionPassed = false;
			}
			else
			{
				for (const FString& Rel : Remaining)
				{
					bool bFound = false;
					for (const FString& NewRel : NewDelta)
					{
						if (ArcLoreHistoryRewriteInternal::SamePath(NewRel, Rel))
						{
							bFound = true;
							break;
						}
					}
					if (!bFound)
					{
						bAssertionPassed = false;
						break;
					}
				}
			}
		}

		// Aggregate every failure mode that occurred AFTER the branch_reset.
		const bool bSubStepFailed = (ResetFileResult != 0) || (DirtyResult != 0) || (StageResult != 0) || (CommitResult != 0);
		if (!bResetFileSeenForTarget || bSubStepFailed || !bAssertionPassed)
		{
			bRemoved = false;
			InCommand.ErrorMessages.Add(FString::Printf(
				TEXT("Remove failed after the commit was reset to its parent — %d file(s) are now uncommitted on disk; re-stage and commit manually."),
				DeltaRel.Num()));
			InCommand.bCommandSuccessful = false;
			return false;
		}
	}

	// ---- 7) Full success -------------------------------------------------------
	bRemoved = true;
	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreRemoveFromOutgoingWorker::UpdateStates() const
{
	// Hand the result back to the operation payload (captured in Execute, since
	// UpdateStates takes no command). The panel's follow-up Refresh re-reads the
	// working set; no StateCache change here, so return false (nothing to broadcast).
	if (Operation.IsValid())
	{
		TSharedRef<FLoreRemoveFromOutgoing, ESPMode::ThreadSafe> Op = StaticCastSharedRef<FLoreRemoveFromOutgoing>(Operation.ToSharedRef());
		Op->bRemoved = bRemoved;
		Op->Note = Note;
	}
	return false;
}

// ----------------------------------------------------------------------------
// FArcLoreLoginWithTokenWorker — non-interactive token login (lore_auth_login_with_token).
// One round-trip, no polling — SAFE under RunLoreOp. Reads Token/TokenType from the
// captured operation, authenticates, and captures the resolved identity from the
// LORE_EVENT_AUTH_USER_INFO event. On success UpdateStates writes the result onto the
// operation AND persists the identity to the provider. Execute runs on the POOL thread
// (worker-local scratch only); UpdateStates runs on the GAME thread.
// ----------------------------------------------------------------------------
FName FArcLoreLoginWithTokenWorker::GetName() const
{
	return "LoreLoginWithToken";
}

bool FArcLoreLoginWithTokenWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// result back to its payload on the game thread.
	Operation = InCommand.Operation;

	// Read the login inputs from the captured operation payload.
	TSharedRef<FLoreLoginWithToken, ESPMode::ThreadSafe> LoginOp = StaticCastSharedRef<FLoreLoginWithToken>(InCommand.Operation);
	const FString Token = LoginOp->Token;
	const FString TokenType = LoginOp->TokenType;

	if (Token.IsEmpty())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore token login requires a non-empty token."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Build the global args ONCE — the scope owns the UTF-8 buffers the lore_string_t
	// views point into, so it MUST outlive the RunLoreOp call below.
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	// The token + token-type buffers MUST outlive the RunLoreOp call (their View() is
	// stored in the args), so declare them BEFORE the invoker.
	ArcLoreApi::FLoreUtf8String TokenBuf(Token);
	ArcLoreApi::FLoreUtf8String TokenTypeBuf(TokenType);

	lore_auth_login_with_token_args_t LoginArgs;
	FMemory::Memzero(&LoginArgs, sizeof(LoginArgs));
	LoginArgs.remote_url = ArcLoreApi::EmptyLoreString();
	LoginArgs.auth_url = ArcLoreApi::EmptyLoreString();
	// remote_url empty => resolve from the repository config.
	LoginArgs.token      = TokenBuf.View();
	LoginArgs.token_type = TokenTypeBuf.View();
	// auth_url empty => resolve from the repo remote.

	const ArcLoreApi::FLoreOpInvoker Invoker =
		[&GlobalArgs, &LoginArgs](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_auth_login_with_token(&GlobalArgs.Args(), &LoginArgs, Config);
		};

	const ArcLoreApi::FLoreEventCollector Collector =
		[this](const lore_event_t& Event)
		{
			const uint32_t Tag = lore_event_type(&Event);
			if (Tag == LORE_EVENT_AUTH_USER_INFO)
			{
				UserName = ArcLoreApi::LoreStringToFString(Event.auth_user_info.name);
				UserId   = ArcLoreApi::LoreStringToFString(Event.auth_user_info.id);
			}
		};

	FString Error;
	const int32 Result = ArcLoreApi::RunLoreOp(Invoker, Collector, Error, TEXT("LoginWithToken"));
	bSuccess = (Result == 0);
	if (!bSuccess)
	{
		InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_auth_login_with_token failed: %s"), *Error));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	UE_LOGF(LogSourceControl, Verbose,"ArcLore: Login — authenticated as '%ls' (id='%ls')",
		UserName.IsEmpty() ? TEXT("<unknown>") : *UserName,
		UserId.IsEmpty() ? TEXT("<unknown>") : *UserId);

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreLoginWithTokenWorker::UpdateStates() const
{
	// Hand the result back to the operation payload (captured in Execute, since
	// UpdateStates takes no command).
	if (Operation.IsValid())
	{
		TSharedRef<FLoreLoginWithToken, ESPMode::ThreadSafe> Op = StaticCastSharedRef<FLoreLoginWithToken>(Operation.ToSharedRef());
		Op->bSuccess = bSuccess;
		Op->UserName = UserName;
		Op->UserId   = UserId;
	}

	if (!bSuccess)
	{
		return false;
	}

	// Persist the resolved identity so the panel header refreshes and the lock owner-id
	// comparison (Phase 3) has the stable user id.
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();
	Provider.SetUserName(UserName);
	Provider.SetUserId(UserId);
	Provider.SetAuthResolved(true);

	// Return true so OnSourceControlStateChanged broadcasts (the provider's cached
	// identity changed — the panel header re-reads it).
	return true;
}

// ----------------------------------------------------------------------------
// FArcLoreLoginInteractiveWorker — browser-based OAuth login (lore_auth_login_interactive,
// sync variant). Blocks the pool thread until LORE_EVENT_END; lore.dll opens the OS
// default browser (no_browser=0). Execute runs on the POOL thread (worker-local scratch
// only); UpdateStates runs on the GAME thread and writes the result onto the captured
// FLoreLoginInteractive operation + persists identity to the provider on success.
// NOTE: the C ABI exposes no cancel handle; the op self-times-out (~150s) on the lore side.
// ----------------------------------------------------------------------------
FName FArcLoreLoginInteractiveWorker::GetName() const
{
	return "LoreLoginInteractive";
}

bool FArcLoreLoginInteractiveWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// result back to its payload on the game thread.
	Operation = InCommand.Operation;

	// Build the global args ONCE — the scope owns the UTF-8 buffers the lore_string_t
	// views point into, so it MUST outlive the RunLoreOp call below.
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	lore_auth_login_interactive_args_t InteractiveArgs;
	FMemory::Memzero(&InteractiveArgs, sizeof(InteractiveArgs));
	InteractiveArgs.remote_url = ArcLoreApi::EmptyLoreString();
	// remote_url empty => resolve from the repository config.
	// no_browser=0 => lore.dll opens the OS default browser itself; AUTH_URL is only
	// emitted when no_browser is set, so we do not handle LORE_EVENT_AUTH_URL here.
	InteractiveArgs.no_browser = 0;

	const ArcLoreApi::FLoreOpInvoker Invoker =
		[&GlobalArgs, &InteractiveArgs](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_auth_login_interactive(&GlobalArgs.Args(), &InteractiveArgs, Config);
		};

	const ArcLoreApi::FLoreEventCollector Collector =
		[this](const lore_event_t& Event)
		{
			const uint32_t Tag = lore_event_type(&Event);
			if (Tag == LORE_EVENT_AUTH_USER_INFO)
			{
				UserName = ArcLoreApi::LoreStringToFString(Event.auth_user_info.name);
				UserId   = ArcLoreApi::LoreStringToFString(Event.auth_user_info.id);
			}
		};

	FString Error;
	const int32 Result = ArcLoreApi::RunLoreOp(Invoker, Collector, Error, TEXT("LoginInteractive"));
	bSuccess = (Result == 0);
	if (!bSuccess)
	{
		InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_auth_login_interactive failed: %s"), *Error));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	UE_LOGF(LogSourceControl, Verbose,"ArcLore: Interactive login — authenticated as '%ls' (id='%ls')",
		UserName.IsEmpty() ? TEXT("<unknown>") : *UserName,
		UserId.IsEmpty() ? TEXT("<unknown>") : *UserId);

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreLoginInteractiveWorker::UpdateStates() const
{
	// Hand the result back to the operation payload (captured in Execute, since
	// UpdateStates takes no command).
	if (Operation.IsValid())
	{
		TSharedRef<FLoreLoginInteractive, ESPMode::ThreadSafe> Op = StaticCastSharedRef<FLoreLoginInteractive>(Operation.ToSharedRef());
		Op->bSuccess = bSuccess;
		Op->UserName = UserName;
		Op->UserId   = UserId;
	}

	if (!bSuccess)
	{
		return false;
	}

	// Persist the resolved identity so the panel header refreshes and the lock owner-id
	// comparison has the stable user id.
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();
	Provider.SetUserName(UserName);
	Provider.SetUserId(UserId);
	Provider.SetAuthResolved(true);

	// Return true so OnSourceControlStateChanged broadcasts (the provider's cached
	// identity changed — the panel header re-reads it).
	return true;
}

// ----------------------------------------------------------------------------
// FArcLoreLogoutWorker — logout the current identity (lore_auth_logout, all args empty).
// UpdateStates clears the provider's cached identity. Execute runs on the POOL thread;
// UpdateStates runs on the GAME thread.
// ----------------------------------------------------------------------------
FName FArcLoreLogoutWorker::GetName() const
{
	return "LoreLogout";
}

bool FArcLoreLogoutWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Build the global args ONCE — the scope owns the UTF-8 buffers the lore_string_t
	// views point into, so it MUST outlive the RunLoreOp call below.
	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	lore_auth_logout_args_t LogoutArgs;
	FMemory::Memzero(&LogoutArgs, sizeof(LogoutArgs));
	LogoutArgs.auth_url = ArcLoreApi::EmptyLoreString();
	LogoutArgs.resource = ArcLoreApi::EmptyLoreString();
	LogoutArgs.user_id = ArcLoreApi::EmptyLoreString();
	// All args empty (auth_url / resource / user_id) == log out the current identity.

	const ArcLoreApi::FLoreOpInvoker Invoker =
		[&GlobalArgs, &LogoutArgs](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_auth_logout(&GlobalArgs.Args(), &LogoutArgs, Config);
		};

	const ArcLoreApi::FLoreEventCollector Collector =
		[](const lore_event_t& /*Event*/)
		{
		};

	FString Error;
	const int32 Result = ArcLoreApi::RunLoreOp(Invoker, Collector, Error, TEXT("Logout"));
	bLoggedOut = (Result == 0);
	if (!bLoggedOut)
	{
		InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_auth_logout failed: %s"), *Error));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreLogoutWorker::UpdateStates() const
{
	if (!bLoggedOut)
	{
		return false;
	}

	// Clear the provider's cached identity so the panel header reflects the logged-out
	// state and the lock owner-id comparison no longer matches a stale user.
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	FArcLoreSourceControlProvider& Provider = Module.GetProvider();
	Provider.SetUserName(FString());
	Provider.SetUserId(FString());
	Provider.SetAuthResolved(false);

	// Stop the live-notification stream now that the user has logged out (no identity
	// to subscribe under). Idempotent — a later re-login re-arms via the Connect worker.
	Provider.StopNotificationSubscription();

	// Return true so OnSourceControlStateChanged broadcasts (cached identity changed).
	return true;
}

// ----------------------------------------------------------------------------
// FArcLoreBranchSwitchWorker
//
// Switch the working copy to a different branch via lore_branch_switch (revision
// empty == that branch's tip). With reset=0 Lore HARD-ERRORS on a staged revision
// or on dirty files the incoming diff would overwrite (LocalModifications); with
// reset=1 those local changes are DISCARDED (data loss). The RunLoreOp OutError is
// captured VERBATIM into ErrorText (never masked) so the panel can surface the exact
// reason. LORE_EVENT_BRANCH_SWITCH_END is the success marker;
// LORE_EVENT_BRANCH_MULTIPLE_INSTANCE is a non-fatal warning (logged). Execute runs
// on the POOL thread (worker-local scratch only); UpdateStates runs on the GAME
// thread and writes the captured operation.
// ----------------------------------------------------------------------------
FName FArcLoreBranchSwitchWorker::GetName() const
{
	return "BranchSwitch";
}

bool FArcLoreBranchSwitchWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// result back to its payload on the game thread.
	Operation = InCommand.Operation;

	// Read the input off the captured operation.
	const TSharedRef<FLoreBranchSwitch, ESPMode::ThreadSafe> SwitchOp = StaticCastSharedRef<FLoreBranchSwitch>(InCommand.Operation);
	const FString BranchName = SwitchOp->Branch;
	const bool bReset = SwitchOp->bReset;

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	// The branch name buffer MUST outlive the RunLoreOp call (its View() is stored in
	// the args), so declare it BEFORE the invoker.
	ArcLoreApi::FLoreUtf8String BranchBuf(BranchName);

	lore_branch_switch_args_t SwitchArgs;
	FMemory::Memzero(&SwitchArgs, sizeof(SwitchArgs));
	SwitchArgs.revision = ArcLoreApi::EmptyLoreString();
	SwitchArgs.branch = BranchBuf.View();
	// revision left empty == the named branch's tip.
	SwitchArgs.reset = bReset ? 1 : 0;
	SwitchArgs.bare = 0;

	const ArcLoreApi::FLoreOpInvoker SwitchInvoker =
		[&GlobalArgs, &SwitchArgs](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_branch_switch(&GlobalArgs.Args(), &SwitchArgs, Config);
		};

	// Track the success marker (BRANCH_SWITCH_END) and log the multiple-instance
	// warning (non-fatal — Lore still proceeds).
	bool bSawSwitchEnd = false;
	const ArcLoreApi::FLoreEventCollector SwitchCollector =
		[&bSawSwitchEnd](const lore_event_t& Event)
		{
			const uint32_t Tag = lore_event_type(&Event);
			switch (Tag)
			{
			case LORE_EVENT_BRANCH_SWITCH_END:
				{
					bSawSwitchEnd = true;
					break;
				}
			case LORE_EVENT_BRANCH_MULTIPLE_INSTANCE:
				{
					UE_LOGF(LogSourceControl, Warning,"ArcLore: branch switch — more than one instance of the branch was found (non-fatal).");
					break;
				}
			default:
				break;
			}
		};

	FString SwitchError;
	const int32 SwitchResult = ArcLoreApi::RunLoreOp(SwitchInvoker, SwitchCollector, SwitchError, TEXT("BranchSwitch"));
	if (SwitchResult != 0)
	{
		// Capture Lore's failure text VERBATIM — do NOT mask it. With reset=0 this is
		// typically "Unable to sync when there is a staged state" or a LocalModifications
		// error naming the dirty files the diff would overwrite.
		ErrorText = SwitchError;
		bSucceeded = false;
		InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_branch_switch failed: %s"), *SwitchError));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	bSucceeded = bSawSwitchEnd;
	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreBranchSwitchWorker::UpdateStates() const
{
	// Hand the result to the operation payload (captured in Execute, since UpdateStates
	// takes no command).
	if (Operation.IsValid())
	{
		TSharedRef<FLoreBranchSwitch, ESPMode::ThreadSafe> SwitchOp = StaticCastSharedRef<FLoreBranchSwitch>(Operation.ToSharedRef());
		SwitchOp->bSucceeded = bSucceeded;
		SwitchOp->ErrorText = ErrorText;
	}

	// Return true on a successful switch so OnSourceControlStateChanged broadcasts and
	// the panel refreshes (the working set, branch, history all changed). A failed
	// switch changed nothing — nothing to broadcast.
	return bSucceeded;
}

// ----------------------------------------------------------------------------
// FArcLoreBranchCreateWorker
//
// Create a new branch from the current revision via lore_branch_create. Does NOT
// switch the working copy. Execute runs on the POOL thread (worker-local scratch
// only); UpdateStates runs on the GAME thread and writes the captured operation.
// ----------------------------------------------------------------------------
FName FArcLoreBranchCreateWorker::GetName() const
{
	return "BranchCreate";
}

bool FArcLoreBranchCreateWorker::Execute(FArcLoreSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	if (!ArcLoreApi::IsReady())
	{
		InCommand.ErrorMessages.Add(TEXT("Lore library is not initialized."));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	// Capture the operation so UpdateStates (which takes no command) can write the
	// result back to its payload on the game thread.
	Operation = InCommand.Operation;

	// Read the input off the captured operation.
	const TSharedRef<FLoreBranchCreate, ESPMode::ThreadSafe> CreateOp = StaticCastSharedRef<FLoreBranchCreate>(InCommand.Operation);
	const FString BranchName = CreateOp->Name;
	const FString Category = CreateOp->Category;

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(InCommand.PathToRepositoryRoot, InCommand.Identity);

	// The name/category buffers MUST outlive the RunLoreOp call (their View()s are
	// stored in the args), so declare them BEFORE the invoker.
	ArcLoreApi::FLoreUtf8String NameBuf(BranchName);
	ArcLoreApi::FLoreUtf8String CategoryBuf(Category);

	lore_branch_create_args_t CreateArgs;
	FMemory::Memzero(&CreateArgs, sizeof(CreateArgs));
	CreateArgs.id = ArcLoreApi::EmptyLoreString();
	CreateArgs.branch = NameBuf.View();
	CreateArgs.category = CategoryBuf.View();
	// id left empty == Lore derives a fresh branch id.

	const ArcLoreApi::FLoreOpInvoker CreateInvoker =
		[&GlobalArgs, &CreateArgs](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_branch_create(&GlobalArgs.Args(), &CreateArgs, Config);
		};

	bool bSawCreate = false;
	const ArcLoreApi::FLoreEventCollector CreateCollector =
		[&bSawCreate](const lore_event_t& Event)
		{
			const uint32_t Tag = lore_event_type(&Event);
			if (Tag == LORE_EVENT_BRANCH_CREATE)
			{
				bSawCreate = true;
			}
		};

	FString CreateError;
	const int32 CreateResult = ArcLoreApi::RunLoreOp(CreateInvoker, CreateCollector, CreateError, TEXT("BranchCreate"));
	if (CreateResult != 0)
	{
		bSucceeded = false;
		InCommand.ErrorMessages.Add(FString::Printf(TEXT("lore_branch_create failed: %s"), *CreateError));
		InCommand.bCommandSuccessful = false;
		return false;
	}

	bSucceeded = bSawCreate;
	InCommand.bCommandSuccessful = true;
	return true;
}

bool FArcLoreBranchCreateWorker::UpdateStates() const
{
	// Hand the result to the operation payload (captured in Execute). A create does NOT
	// switch the working copy, so no StateCache change — return false (nothing to
	// broadcast); the panel re-lists branches on its own Refresh.
	if (Operation.IsValid())
	{
		TSharedRef<FLoreBranchCreate, ESPMode::ThreadSafe> CreateOp = StaticCastSharedRef<FLoreBranchCreate>(Operation.ToSharedRef());
		CreateOp->bSucceeded = bSucceeded;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
