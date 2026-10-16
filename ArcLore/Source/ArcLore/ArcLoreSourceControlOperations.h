// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IArcLoreSourceControlWorker.h"
#include "ArcLoreSourceControlState.h"
#include "ArcLoreSourceControlRevision.h"
#include "ArcLoreSourceControlUtils.h"
#include "SourceControlOperationBase.h"
#include "Graph/ArcLoreGraphLayout.h"

#define LOCTEXT_NAMESPACE "ArcLoreSourceControl"

/** Explicit "Push" source-control operation backed by lore_branch_push.
 *  Modeled on engine FConnect (no extra payload): the panel constructs this op to
 *  push committed-but-unpushed local revisions to the remote and refresh the
 *  cached ahead/behind state. */
class FLorePush : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Push";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_Pushing", "Pushing to remote...");
	}
};

/** Explicit "Unstage" source-control operation backed by lore_file_unstage.
 *  Inverse of MarkForAdd's stage step (no extra payload): the panel constructs
 *  this op to remove the supplied files from the staged changeset. */
class FLoreUnstage : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Unstage";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_Unstaging", "Unstaging...");
	}
};

/** "RevisionLog" source-control operation backed by lore_revision_history.
 *  No-file op: the panel constructs this to query the current branch's commit
 *  log; the worker fills Revisions with the resulting revisions (newest first,
 *  as Lore emits them). Modeled on the engine FGetPendingChangelists-style op
 *  that carries a result payload back to the caller. */
class FLoreRevisionLog : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreRevisionLog";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_RevisionLog", "Querying revision log...");
	}

public:
	/** Revisions returned by lore_revision_history, populated by the worker's
	 *  UpdateStates on the game thread. */
	TArray<TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>> Revisions;
};

/** One branch row returned by lore_branch_list (plain POD; no USTRUCT). */
struct FArcLoreBranchEntry
{
	FString Name;
	FString Category;
	FString Creator;
	FDateTime Created;
	bool bIsCurrent = false;
	bool bArchived = false;
	bool bRemote = false;
};

/** One checkout-instance row returned by lore_repository_instance_list (POD). */
struct FArcLoreInstanceEntry
{
	FString Path;
	FString BranchName;
	bool bStale = false;
};

/** One remote-repository row returned by lore_repository_list (POD). Only the
 *  name is surfaced; the entry's id is an opaque struct, not a string. */
struct FArcLoreRepoEntry
{
	FString Name;
};

/** "LoreRepositoryView" source-control operation — LOCAL-only repository view.
 *  No-file op: the panel constructs this on its normal Refresh to enumerate the
 *  repository's branches (lore_branch_list) and the tracked checkout instances
 *  (lore_repository_instance_list). Both are local reads, so this is safe to run
 *  on the panel's normal Refresh. The remote repo listing is a SEPARATE op
 *  (FLoreListRemoteRepos) because it blocks on the network. */
class FLoreRepositoryView : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreRepositoryView";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_RepositoryView", "Querying repositories...");
	}

public:
	/** Branches returned by lore_branch_list, populated by the worker's
	 *  UpdateStates on the game thread. */
	TArray<FArcLoreBranchEntry> Branches;

	/** Checkout instances returned by lore_repository_instance_list, populated by
	 *  the worker's UpdateStates on the game thread. */
	TArray<FArcLoreInstanceEntry> Instances;
};

/** "LoreListRemoteRepos" source-control operation — REMOTE repo listing.
 *  No-file op: lists the repositories available on the server via
 *  lore_repository_list against the provider's remote URL. This BLOCKS on the
 *  network with no timeout/cancel in RunLoreOp, so it is kept SEPARATE from the
 *  local FLoreRepositoryView and dispatched only behind an explicit user action
 *  (a panel button), never on the panel's normal Refresh. */
class FLoreListRemoteRepos : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreListRemoteRepos";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_ListRemoteRepos", "Listing remote repositories...");
	}

public:
	/** Repositories returned by lore_repository_list, populated by the worker's
	 *  UpdateStates on the game thread. */
	TArray<FArcLoreRepoEntry> RemoteRepos;
};

/** One unpushed-file row returned by lore_revision_diff (remote tip -> local HEAD). */
struct FArcLoreUnpushedFile
{
	/** Repo-relative path of the file that differs (forward-slash, as Lore emits). */
	FString Path;

	/** Derived working-state for the diff action (ADD/COPY->Added, DELETE->Deleted,
	 *  MOVE/KEEP->Modified — a content-modified file in a diff arrives as KEEP). */
	EArcLoreState::Type Action = EArcLoreState::Modified;

	/** TRANSIENT (display only): absolute on-disk path, filled game-side from the repo
	 *  root + Path so the panel row can resolve a useful name (actor label / asset short
	 *  name) instead of the raw OFPA GUID filename. */
	FString AbsPath;

	/** TRANSIENT (display only): lazily-resolved display fields (see the panel's
	 *  ResolveDisplayInfo) — a useful name, the owning map / package folder, and the
	 *  full on-disk path kept as the row tooltip. */
	FString CachedPrimary;
	FString CachedSecondary;
	FString CachedTooltip;
	bool bResolved = false;

	/** TRANSIENT (display only): asset class short name (e.g. "WidgetBlueprint"),
	 *  resolved no-load via ArcLoreAssetDisplay::ResolveAssetTypeName, cached once. */
	FString CachedType;
	bool bTypeResolved = false;
};

/** One committed-but-unpushed local revision, with the files it changed (POD; no
 *  USTRUCT). Built by FArcLoreListUnpushedWorker from the history walk + per-commit
 *  delta reads, and consumed by the panel's Outgoing tree (one commit node per entry,
 *  one file child per Files entry). */
struct FArcLoreUnpushedCommit
{
	/** Commit hash (full 64-char lowercase hex). */
	FString RevisionHashHex;

	/** Sequential revision number (from the history entry). */
	int32 RevisionNumber = 0;

	/** Full commit message (the panel shows its first line in the commit header). */
	FString Message;

	/** Commit timestamp (UTC; Lore emits MILLISECONDS, divided by 1000). */
	FDateTime Date;

	/** Files changed in THIS commit (one entry per delta path; directories skipped). */
	TArray<FArcLoreUnpushedFile> Files;
};

/** "LoreListUnpushed" source-control operation — LOCAL-ONLY, no-file query that
 *  enumerates the committed-but-unpushed local revisions and the files each changed.
 *  The status pre-pass (lore_repository_status revision_only=1) reads is_local_ahead
 *  plus the local/remote revision NUMBERS; the worker then walks lore_revision_history
 *  from the local tip and keeps every revision whose number is greater than the remote
 *  tip number (those not yet on the remote), and for each runs lore_revision_info
 *  (delta=1) to list its changed files. When the local branch is not ahead or the
 *  remote tip is unknown, the result is simply empty (a valid outcome — nothing
 *  outgoing / remote unreachable). Excludes working-copy dirty files: only committed
 *  revisions are inspected, never the working copy. */
class FLoreListUnpushed : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreListUnpushed";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_ListUnpushed", "Listing unpushed files...");
	}

public:
	/** Unpushed local commits (newest first), each carrying its changed files.
	 *  Populated by the worker's UpdateStates on the game thread. */
	TArray<FArcLoreUnpushedCommit> Commits;

	/** True when the local branch holds revisions not on the remote (is_local_ahead).
	 *  False (with empty Commits) means nothing outgoing OR the remote tip is unknown. */
	bool bLocalAhead = false;
};

/** "LoreRevisionFiles" source-control operation — LOCAL-ONLY, no-file query that
 *  enumerates the files changed in a single revision versus its parent
 *  (lore_revision_info with delta=1). The panel sets Revision to the commit's
 *  hex before Execute; the worker fills Files with one entry per changed file
 *  (directory entries are skipped). An empty Revision yields an empty result. */
class FLoreRevisionFiles : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreRevisionFiles";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_RevisionFiles", "Listing revision files...");
	}

public:
	/** INPUT: the revision (full hex) whose changed files to list. Set by the panel
	 *  before Execute; empty yields an empty result. */
	FString Revision;

	/** OUTPUT: files changed in the revision, populated by the worker's UpdateStates
	 *  on the game thread. */
	TArray<FArcLoreUnpushedFile> Files;
};

/** "LoreUndoLastCommit" source-control operation — rewrites UNPUSHED local history.
 *  No-file / no-payload op: resets the local branch tip to its direct parent
 *  (lore_branch_reset), so the last local commit's files return to the working set
 *  (dirty/staged) and one unpushed commit is removed. The worker re-verifies the tip
 *  is strictly ahead of the remote server-side (lore_repository_status revision_only=1)
 *  before any reset, and refuses on a root commit (no parent). Destructive but
 *  fail-safe: nothing is mutated unless the ahead-gate and the parent lookup pass. */
class FLoreUndoLastCommit : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreUndoLastCommit";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_UndoLastCommit", "Undoing last local commit...");
	}
};

/** "LoreRemoveFromOutgoing" source-control operation — rewrites UNPUSHED local history
 *  to drop a single file from the LATEST unpushed commit. The panel sets FilePath
 *  (repo-relative) before Execute. The worker: re-verifies the tip is strictly ahead
 *  server-side; reads the latest commit's delta + message; if the target file is in
 *  that delta, resets the branch to its parent, discards the file, re-stages the
 *  commit's other files (scan=1) and re-commits with the original message; then
 *  self-verifies (a post-commit delta assertion) that the new tree excludes the file.
 *  A file NOT in the latest commit is reported as a soft "can't do that here" (bRemoved
 *  false, Note set, command success). Any failure after the reset is a HARD failure
 *  with an explicit recovery message — never a silent destructive half-state. */
class FLoreRemoveFromOutgoing : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreRemoveFromOutgoing";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_RemoveFromOutgoing", "Removing file from the latest unpushed commit...");
	}

public:
	/** INPUT: repo-relative path of the file to drop from the latest unpushed commit.
	 *  Set by the panel before Execute. */
	FString FilePath;

	/** OUTPUT: true only when the file was actually removed and the rewrite verified.
	 *  False with a non-empty Note means a valid "can't remove here" (file not in the
	 *  latest commit); false with a command ErrorMessage means a hard failure. */
	bool bRemoved = false;

	/** OUTPUT: human-readable note for the soft-skip case (set by the worker's
	 *  UpdateStates on the game thread). Empty on success or hard failure. */
	FString Note;
};

/** "LoreAmendLastCommit" source-control operation — rewrites the UNPUSHED local tip's
 *  commit MESSAGE only (lore_revision_amend). No-file op: the panel sets NewMessage
 *  before Execute. The worker re-verifies the tip is strictly ahead of the remote
 *  server-side (VerifyAheadOrFail) before any amend — amending a pushed commit would
 *  diverge the branch — and refuses an empty/whitespace message. Amend produces a NEW
 *  tip hash (history rewrite, LORE_EVENT_REVISION_COMMIT_REVISION); the panel's
 *  follow-up Refresh re-reads the new message. Fail-safe: nothing mutates unless the
 *  ahead-gate and the non-empty-message check pass. */
class FLoreAmendLastCommit : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreAmendLastCommit";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_AmendLastCommit", "Amending last local commit...");
	}

public:
	/** INPUT: the new commit message for the latest unpushed commit. Set by the panel
	 *  before Execute; an empty/whitespace value is refused by the worker. */
	FString NewMessage;
};

/** One lock row returned by lore_lock_file_query (POD; no USTRUCT). Path is
 *  repo-relative (forward-slash, as Lore emits); OwnerId is a RAW Lore user id (the
 *  panel resolves "me" by comparing it to the provider's resolved user id); LockedAt
 *  is the acquire time (server timestamp, seconds since the Unix epoch). */
struct FArcLoreLockEntry
{
	FString Path;
	FString OwnerId;
	FDateTime LockedAt;
};

/** "LoreListLocks" source-control operation — branch-wide lock listing backed by
 *  lore_lock_file_query (branch/owner/path filters all empty => every lock on the
 *  current branch). No-file op: the panel constructs this for its Locks tab; the
 *  worker fills Locks with one entry per locked path. AUTH-GATED — when the user is
 *  not logged in the result is simply empty (a valid outcome; the lock service is an
 *  authenticated endpoint). Soft: a failed query yields an empty list, never a hard
 *  failure. Mirrors FLoreRepositoryView/FLoreListUnpushed (result payload back to the
 *  caller). */
class FLoreListLocks : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreListLocks";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_ListLocks", "Listing locks...");
	}

public:
	/** Locks returned by lore_lock_file_query, populated by the worker's UpdateStates
	 *  on the game thread. */
	TArray<FArcLoreLockEntry> Locks;
};

/** "LoreLoginWithToken" source-control operation — non-interactive token login backed
 *  by lore_auth_login_with_token. One round-trip (no polling), so SAFE under RunLoreOp.
 *  The panel sets Token + TokenType before Execute; the worker authenticates, captures
 *  the resolved identity (LORE_EVENT_AUTH_USER_INFO id/name), and on success persists
 *  it onto the provider (UserName/UserId/AuthResolved). Interactive/browser login is a
 *  SEPARATE deferred op. */
class FLoreLoginWithToken : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreLoginWithToken";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_LoginWithToken", "Logging in with token...");
	}

public:
	/** INPUT: the authentication token. Set by the panel before Execute. */
	FString Token;

	/** INPUT: the token type ("api-key", "eg1", or "lore"). Set by the panel before Execute. */
	FString TokenType;

	/** OUTPUT: true when authentication succeeded. */
	bool bSuccess = false;

	/** OUTPUT: resolved display name (from the AUTH_USER_INFO event on success). */
	FString UserName;

	/** OUTPUT: resolved user id (from the AUTH_USER_INFO event on success). */
	FString UserId;
};

/** "LoreLoginInteractive" source-control operation — browser-based OAuth login backed
 *  by lore_auth_login_interactive (sync variant). No input fields: lore.dll opens the
 *  OS default browser and polls until the user completes sign-in. The op self-times-out
 *  (~150s) on the lore side; there is no cancel handle in the C ABI.
 *  Output fields mirror FLoreLoginWithToken (bSuccess/UserName/UserId). */
class FLoreLoginInteractive : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreLoginInteractive";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_LoginInteractive", "Signing in via browser...");
	}

public:
	/** OUTPUT: true when authentication succeeded. */
	bool bSuccess = false;

	/** OUTPUT: resolved display name (from the AUTH_USER_INFO event on success). */
	FString UserName;

	/** OUTPUT: resolved user id (from the AUTH_USER_INFO event on success). */
	FString UserId;
};

/** "LoreLogout" source-control operation — backed by lore_auth_logout. No payload:
 *  logs out the current identity (all logout args empty), then clears the provider's
 *  cached identity (UserName/UserId/AuthResolved) in UpdateStates. */
class FLoreLogout : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreLogout";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_Logout", "Logging out...");
	}
};

/** "LoreReleaseLock" source-control operation — STANDALONE lock release backed by
 *  lore_lock_file_release. Per-file (uses InCommand.Files): the panel constructs this
 *  to explicitly Unlock the supplied files WITHOUT pushing (Push releases as a side
 *  effect; this is the explicit release). branch/owner/owner_id are left empty so the
 *  release targets the current user on the current branch. A path with no matching lock
 *  (LORE_EVENT_LOCK_FILE_RELEASE_BEGIN with not_found=1) is a soft outcome, not an error. */
class FLoreReleaseLock : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreReleaseLock";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_ReleaseLock", "Releasing locks...");
	}
};

/** "BranchSwitch" source-control operation backed by lore_branch_switch — switches the
 *  working copy to a different branch (revision left empty == that branch's tip). The
 *  panel sets Branch (and optionally bReset) before Execute. With bReset=false Lore
 *  HARD-ERRORS on a staged revision or on dirty files the incoming diff would overwrite
 *  (LocalModifications); with bReset=true those local changes are DISCARDED (data loss).
 *  The worker captures Lore's failure text verbatim into ErrorText (never masked) so the
 *  panel can surface the exact reason. A successful switch broadcasts a state change so
 *  the panel refreshes. */
class FLoreBranchSwitch : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "BranchSwitch";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_BranchSwitch", "Switching branch...");
	}

public:
	/** INPUT: name of the branch to switch to. Set by the panel before Execute. */
	FString Branch;

	/** INPUT: when true, local modifications are RESET to match the incoming revision
	 *  (data loss). When false (default), a staged revision or conflicting dirty files
	 *  hard-error the switch. Set by the panel before Execute. */
	bool bReset = false;

	/** OUTPUT: true only when lore_branch_switch succeeded. */
	bool bSucceeded = false;

	/** OUTPUT: Lore's failure text captured verbatim on failure (e.g. "Unable to sync
	 *  when there is a staged state" / LocalModifications). Empty on success. */
	FString ErrorText;
};

/** "BranchCreate" source-control operation backed by lore_branch_create — creates a new
 *  branch from the CURRENT revision but does NOT switch to it. The panel sets Name (and
 *  optionally Category) before Execute. */
class FLoreBranchCreate : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "BranchCreate";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_BranchCreate", "Creating branch...");
	}

public:
	/** INPUT: name of the branch to create. Set by the panel before Execute. */
	FString Name;

	/** INPUT: category of the branch (optional). Set by the panel before Execute. */
	FString Category;

	/** OUTPUT: true only when lore_branch_create succeeded. */
	bool bSucceeded = false;
};

/** "LoreBranchGraph" source-control operation — assembles the whole cross-branch
 *  commit DAG for the Branch Graph tab. No-file op: the worker enumerates every
 *  branch via lore_branch_list, then walks each branch's history (bounded) via
 *  lore_revision_history, returning the raw input arrays the pure-C++ layout
 *  builder (FArcLoreGraphLayoutBuilder::BuildLayout) topo-sorts into a drawable
 *  layout. N branches => 1 + N serial RunLoreOp round-trips on the pool thread,
 *  each history call capped (see worker) — acceptable because async dispatch keeps
 *  the UI responsive. */
class FLoreBranchGraph : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "LoreBranchGraph";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("ArcLore_BranchGraph", "Building branch graph...");
	}

public:
	/** INPUT: when true, lore_branch_list is asked for archived branches too. */
	bool bIncludeArchived = false;

	/** OUTPUT: every branch (name, tip, full fork lineage, flags) from
	 *  lore_branch_list; populated by the worker's UpdateStates on the game thread. */
	TArray<FArcLoreBranchInput> Branches;

	/** OUTPUT: every revision collected across all per-branch history walks (with
	 *  duplicates — dedup happens in BuildLayout); populated by the worker's
	 *  UpdateStates on the game thread. */
	TArray<FArcLoreRevisionInput> Revisions;
};

#undef LOCTEXT_NAMESPACE

/** Called when first activated on a project, and then at project load time.
 *  Look for the root directory of the git repository (where the ".git/" subdirectory is located). */
class FArcLoreConnectWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreConnectWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FArcLoreSourceControlState> States;

	/** LFS tracking patterns parsed from .gitattributes */
	TArray<FString> LfsPatterns;

	/** Resolved Lore identity display name from lore_auth_local_user_info (empty if not logged in) */
	FString InfoUserName;

	/** Resolved Lore user id from lore_auth_local_user_info (empty if not logged in) */
	FString InfoUserId;

	/** Current branch name from lore_repository_status (empty if unavailable) */
	FString InfoBranchName;

	/** True if auth-info resolved a user (soft — failure does not fail Connect) */
	bool bAuthResolved = false;
};

/** Commit (check-in) a set of files: stage -> commit -> push -> release locks. */
class FArcLoreCheckInWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreCheckInWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** ABSOLUTE paths of the files that were committed (stage+commit succeeded). */
	TArray<FString> CommittedFiles;

	/** True only when stage+commit succeeded. Locks are RETAINED; push is a separate op. */
	bool bCheckInSucceeded = false;

	/** Remote-ahead flag re-queried after commit (status_revision is_remote_ahead). */
	bool bIsNewerOnRemote = false;

	/** Local-ahead flag re-queried after commit (status_revision is_local_ahead): a new
	 *  local commit normally sets this so the panel shows "to push". */
	bool bIsLocalAhead = false;
};

/** Add an untracked file to source control (so only a subset of the git add command). */
class FArcLoreMarkForAddWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreMarkForAddWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FArcLoreSourceControlState> States;
};

/** Handle the engine FCopy ("Copy") operation as a Lore dirty_move so an asset
 *  rename records as a history-preserving MOVE (one node, lineage follows the new
 *  path) instead of a delete-of-old + add-of-new pair. The editor's rename flow
 *  (AssetRenameManager -> USourceControlHelpers::BranchPackage) dispatches FCopy
 *  with CopyMethod==Branch BEFORE the new package is saved — fine here, because
 *  lore_file_dirty_move is filesystem-free (it reparents the staged node only).
 *  Source = InCommand.Files[0] (already absolute); destination = the FCopy op's
 *  GetDestination() (a full package filename) resolved to absolute the SAME way.
 *  CopyMethod==Add (duplication, not driven by rename) is a defensive no-op. */
class FArcLoreCopyWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreCopyWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** ABSOLUTE source path (the renamed-from asset), captured from InCommand.Files[0]
	 *  in Execute for use by UpdateStates. */
	FString SourceAbs;

	/** ABSOLUTE destination path (the renamed-to asset), resolved from the FCopy op's
	 *  GetDestination() in Execute for use by UpdateStates. */
	FString DestAbs;

	/** True only when a real dirty_move was issued (CopyMethod==Branch and the op
	 *  succeeded). False for the Add no-op or any failure — gates whether UpdateStates
	 *  stamps the destination state. */
	bool bMoved = false;
};

/** Remove files from the staged changeset via lore_file_unstage (inverse of the
 *  stage step performed by MarkForAdd). */
class FArcLoreUnstageWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreUnstageWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Per-file post-unstage record captured on the worker thread. */
	struct FUnstagedFile
	{
		/** ABSOLUTE path of the unstaged file. */
		FString AbsPath;

		/** True when the file's staged action was ADD (a new-file add). A former
		 *  staged-add becomes NotControlled (untracked) after unstaging; anything
		 *  else (modify/move/copy of a tracked file) becomes Modified. */
		bool bWasAdd = false;
	};

	/** Files removed from the staged changeset this op (from unstage_file events). */
	TArray<FUnstagedFile> UnstagedFiles;
};

/** Delete a file and remove it from source control. */
class FArcLoreDeleteWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreDeleteWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Map of filenames to state */
	TArray<FArcLoreSourceControlState> States;
};

/** Revert any change to a file to its state on the local depot. */
class FArcLoreRevertWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreRevertWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Map of filenames to state */
	TArray<FArcLoreSourceControlState> States;
};

/** Sync the working copy to the branch tip via lore_revision_sync. */
class FArcLoreSyncWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreSyncWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** ABSOLUTE paths of files touched by the sync (revision_sync_file events). */
	TArray<FString> TouchedFiles;

	/** ABSOLUTE paths Lore reports as still-conflicted (branch_merge_conflict_file). */
	TArray<FString> ConflictedFiles;

	/** Resulting-revision flags from the revision_sync_revision event. */
	bool bFlagMerge = false;
	bool bFlagConflict = false;
};

/** Get source control status of files on local working copy. */
class FArcLoreUpdateStatusWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreUpdateStatusWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

	/** Raw lock fields captured on the lore worker thread (owner id compared to the
	 *  provider's resolved user id on the game thread in UpdateStates to derive
	 *  bLockedByMe). OwnerId is a RAW Lore user id (not a display name); LockedAt is
	 *  the server timestamp in MILLISECONDS. */
	struct FLoreLockScratch
	{
		FString OwnerId;
		FString Branch;
		uint64  LockedAt = 0;
	};

public:
	/** True when Execute ran in REPO-WIDE mode (single repo-root sentinel file). In that
	 *  mode the status pass is authoritative over the whole working set, so UpdateStates
	 *  clears any cached state NOT reported this pass back to Unchanged (drops committed/
	 *  cleaned files from Dirty/Staged). Stays false for per-file queries (a subset). */
	bool bWasRepoWide = false;

	/** Temporary states for results, seeded Unchanged then overwritten from status events */
	TArray<FArcLoreSourceControlState> States;

	/** Map of ABSOLUTE filenames to history */
	TMap<FString, TArcLoreSourceControlHistory> Histories;

	/** Lock scratch keyed by ABSOLUTE file path */
	TMap<FString, FLoreLockScratch> Locks;

	/** Remote-ahead flag from the status_revision event (drives bIsNewerOnRemote). */
	bool bIsNewerOnRemote = false;

	/** Local-ahead flag from the status_revision event (is_local_ahead != 0): the
	 *  local branch holds revisions not on the remote history line (committed but
	 *  not pushed). Drives the provider's bCachedLocalAhead. */
	bool bIsLocalAhead = false;

	/** Raw revision numbers from the status_revision event. NOT differenced into an
	 *  unpushed count: is_local_ahead/is_remote_ahead are independent divergent-DAG
	 *  flags, so local-minus-remote is not a reliable unpushed-commit count. */
	uint64 RevisionLocalNumber = 0;
	uint64 RevisionRemoteNumber = 0;

	/** Local tip hex captured from the status_revision event (revision_local; empty if
	 *  zero/unknown). Repo-wide mode only — seeds the out-of-date diff source. */
	FString LocalTipHex;

	/** Remote tip hex captured from the status_revision event (revision_remote; empty if
	 *  zero/unknown). Repo-wide mode only — seeds the out-of-date diff target. */
	FString RemoteTipHex;

	/** Remote-ahead flag captured alongside the tip hexes (is_remote_ahead != 0). Gates
	 *  whether the per-asset out-of-date diff runs (repo-wide mode only). */
	bool bRemoteAhead = false;

	/** ABSOLUTE paths the REMOTE is ahead on, computed (repo-wide mode only) from a
	 *  lore_revision_diff(source=local tip, target=remote tip): the exact files the
	 *  remote holds a newer revision of. UpdateStates stamps bIsNewerOnRemote per-file
	 *  from membership in this set (true for these files, false for all others). */
	TSet<FString> OutdatedFiles;

	/** True only when the branch-wide lock query actually ran AND succeeded — gates the
	 *  authoritative "clear locks not in the map" pass so a skipped/failed query does
	 *  not wipe locks that were stamped directly by the CheckOut worker. */
	bool bLockQueryAuthoritative = false;
};

/** Mark merge-conflicted files as resolved via lore_branch_merge_resolve. */
class FArcLoreResolveWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreResolveWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

private:
	/** ABSOLUTE paths of files marked resolved this op (set Modified in UpdateStates). */
	TArray<FString> ResolvedFiles;
};

/** Acquire Lore file locks (lock-as-checkout) via lore_lock_file_acquire. */
class FArcLoreCheckOutWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreCheckOutWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
public:
	/** ABSOLUTE paths of files whose lock was successfully acquired this op. */
	TArray<FString> AcquiredFiles;
};

/** Release Lore file locks (standalone Unlock) via lore_lock_file_release. Per-file
 *  (InCommand.Files); branch/owner/owner_id left empty => current user, current branch.
 *  A not-found release (LOCK_FILE_RELEASE_BEGIN not_found=1) is soft (not an error). Successfully-
 *  released paths (collected on the lore worker thread; each echoed path mapped back to
 *  its matching InCommand.Files entry so the cache key equals the editor's absolute key)
 *  have their lock fields cleared in UpdateStates on the game thread. */
class FArcLoreReleaseLockWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreReleaseLockWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
public:
	/** ABSOLUTE paths of files whose lock was successfully released this op. */
	TArray<FString> ReleasedFiles;
};

/** Push committed-but-unpushed local revisions to the remote via lore_branch_push,
 *  then re-query ahead/behind state and (when files are supplied) release their locks. */
class FArcLorePushWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLorePushWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** True only when lore_branch_push succeeded (drives the UpdateStates broadcast). */
	bool bPushSucceeded = false;

	/** Remote-ahead flag re-queried after push (status_revision is_remote_ahead). */
	bool bIsNewerOnRemote = false;

	/** Local-ahead flag re-queried after push (status_revision is_local_ahead): with a
	 *  successful push this is normally cleared, but it is actually re-queried, not zeroed. */
	bool bIsLocalAhead = false;

	/** ABSOLUTE paths whose locks were released after a successful push (files supplied). */
	TArray<FString> ReleasedFiles;
};

/** Query the current branch's commit log via lore_revision_history and return the
 *  revisions to the caller (no-file op; soft — a failure does not fail the command). */
class FArcLoreRevisionLogWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreRevisionLogWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Revisions collected on the lore worker thread from revision_history_entry +
	 *  METADATA events; handed to FLoreRevisionLog::Revisions in UpdateStates. */
	TArray<TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>> Entries;

	/** The originating operation, captured in Execute so UpdateStates (which takes
	 *  no command) can write Entries back to FLoreRevisionLog::Revisions. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** Local repository view: enumerate branches (lore_branch_list) and tracked
 *  checkout instances (lore_repository_instance_list). Both ops are LOCAL reads
 *  under one FLoreGlobalArgsScope (no-file op; soft — a failure does not fail the
 *  command). */
class FArcLoreRepositoryViewWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreRepositoryViewWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Branches collected on the lore worker thread from branch_list events;
	 *  handed to FLoreRepositoryView::Branches in UpdateStates. */
	TArray<FArcLoreBranchEntry> Branches;

	/** Instances collected on the lore worker thread from repository_instance
	 *  events; handed to FLoreRepositoryView::Instances in UpdateStates. */
	TArray<FArcLoreInstanceEntry> Instances;

	/** The originating operation, captured in Execute so UpdateStates (which takes
	 *  no command) can write the scratch arrays back to FLoreRepositoryView. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** Remote repository listing: list the repositories on the server via
 *  lore_repository_list against the provider's remote URL. BLOCKS on the network
 *  with no timeout — dispatched only behind an explicit user action (no-file op;
 *  soft — a failure does not fail the command). */
class FArcLoreListRemoteReposWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreListRemoteReposWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Repositories collected on the lore worker thread from
	 *  repository_list_entry events; handed to FLoreListRemoteRepos::RemoteRepos
	 *  in UpdateStates. */
	TArray<FArcLoreRepoEntry> RemoteRepos;

	/** The originating operation, captured in Execute so UpdateStates (which takes
	 *  no command) can write RemoteRepos back to FLoreListRemoteRepos. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** List the committed-but-unpushed local revisions and the files each changed.
 *  Reads under one FLoreGlobalArgsScope:
 *    1) lore_repository_status (revision_only=1) — read is_local_ahead and the
 *       remote/local tip revision NUMBERS.
 *    2) lore_revision_history (from the local tip, bounded) — open one commit per
 *       LORE_EVENT_REVISION_HISTORY_ENTRY whose revision_number > RemoteNumber
 *       (those not yet on the remote), attaching trailing METADATA (message,
 *       timestamp). Stop including once revision_number <= RemoteNumber.
 *    3) For each kept commit: lore_revision_info (revision = commit hash, delta=1) —
 *       one REVISION_INFO_DELTA event per changed file (directories skipped).
 *  When not ahead or the remote tip is unknown, steps 2/3 are skipped and Commits
 *  stays empty (a valid result). No-file op; soft — a failed read does not hard-fail
 *  the command. Execute runs on the POOL thread (worker-local scratch only);
 *  UpdateStates runs on the GAME thread and writes the captured operation. */
class FArcLoreListUnpushedWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreListUnpushedWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Unpushed commits (newest first) collected on the lore worker thread from the
	 *  history walk + per-commit delta reads; handed to FLoreListUnpushed::Commits in
	 *  UpdateStates. */
	TArray<FArcLoreUnpushedCommit> Commits;

	/** Local-ahead flag from the status pre-pass (is_local_ahead != 0); handed to
	 *  FLoreListUnpushed::bLocalAhead in UpdateStates. */
	bool bAhead = false;

	/** Remote tip revision number from the status pre-pass (revision_remote_number):
	 *  the history-walk termination bound — only revisions numbered strictly above this
	 *  are unpushed. */
	uint64 RemoteNumber = 0;

	/** Local tip revision number from the status pre-pass (revision_local_number). */
	uint64 LocalNumber = 0;

	/** The originating operation, captured in Execute so UpdateStates (which takes
	 *  no command) can write the scratch back to FLoreListUnpushed. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** Branch-wide lock listing: one lore_lock_file_query (branch/owner/path filters all
 *  empty => every lock on the current branch) under one FLoreGlobalArgsScope. AUTH-GATED
 *  on Provider.IsAuthResolved() — when not logged in the query is skipped and Entries
 *  stays empty (a valid result; the lock service is an authenticated endpoint that fails
 *  with a ~2.1s transport error until a Lore identity is resolved). No-file op; soft — a
 *  failed query does not hard-fail the command. Execute runs on the POOL thread (worker-
 *  local scratch only); UpdateStates runs on the GAME thread and writes the captured
 *  operation. */
class FArcLoreListLocksWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreListLocksWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Locks collected on the lore worker thread from lock_file_query events; handed to
	 *  FLoreListLocks::Locks in UpdateStates. */
	TArray<FArcLoreLockEntry> Entries;

	/** The originating operation, captured in Execute so UpdateStates (which takes no
	 *  command) can write Entries back to FLoreListLocks. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** List the files changed in a single revision versus its parent via
 *  lore_revision_info (delta=1, metadata=0). One read under one
 *  FLoreGlobalArgsScope: each LORE_EVENT_REVISION_INFO_DELTA event is one changed
 *  path (directory entries — flag_file==0 — are skipped). The revision to inspect
 *  is read from the captured FLoreRevisionFiles::Revision; an empty revision yields
 *  an empty result. No-file op; soft — a failed read does not hard-fail the command.
 *  Execute runs on the POOL thread (worker-local scratch only); UpdateStates runs on
 *  the GAME thread and writes the captured operation. */
class FArcLoreRevisionFilesWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreRevisionFilesWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Changed files collected on the lore worker thread from revision_info_delta
	 *  events; handed to FLoreRevisionFiles::Files in UpdateStates. */
	TArray<FArcLoreUnpushedFile> Entries;

	/** The originating operation, captured in Execute so UpdateStates (which takes
	 *  no command) can write the scratch back to FLoreRevisionFiles. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** Undo the last UNPUSHED local commit by resetting the local branch tip to its
 *  direct parent (lore_branch_reset). DESTRUCTIVE history rewrite — fail-safe:
 *    0) re-verify the tip is strictly ahead of the remote server-side
 *       (lore_repository_status revision_only=1) — never trust the cached panel flag;
 *    1) read the current tip's direct parent (lore_revision_info, delta=0, metadata=0);
 *       a zero parent (root commit) hard-fails — nothing is reset;
 *    2) lore_branch_reset(revision = parent hex) — the commit's files reconcile back
 *       onto the working set (dirty/staged vs the new tip).
 *  No payload back to the caller; UpdateStates returns false (the follow-up Refresh
 *  re-reads the working set). Execute runs on the POOL thread (worker-local scratch
 *  only); UpdateStates runs on the GAME thread. */
class FArcLoreUndoLastCommitWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreUndoLastCommitWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
};

/** Amend the latest UNPUSHED local commit's MESSAGE only (lore_revision_amend).
 *  History rewrite — fail-safe:
 *    0) re-verify the tip is strictly ahead of the remote server-side
 *       (VerifyAheadOrFail) — never trust the cached panel flag;
 *    1) refuse an empty/whitespace NewMessage (nothing mutates);
 *    2) lore_revision_amend(message = NewMessage) — emits a NEW tip hash via
 *       LORE_EVENT_REVISION_COMMIT_REVISION (captured for a diagnostic log).
 *  No payload back to the caller; UpdateStates returns false (the follow-up Refresh
 *  re-reads the amended message). Execute runs on the POOL thread (worker-local scratch
 *  only); UpdateStates runs on the GAME thread. */
class FArcLoreAmendLastCommitWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreAmendLastCommitWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
};

/** Remove a single file from the LATEST unpushed commit, rewriting UNPUSHED local
 *  history. DESTRUCTIVE — fail-safe (S1/S4/B2/B3):
 *    0) re-verify the tip is strictly ahead server-side (VerifyAheadOrFail);
 *    1) read the latest commit's delta (lore_revision_info, delta=1, metadata=1):
 *       direct parent, the changed-file paths, and the commit message;
 *    2) if the target file matches no delta entry (normalized compare) → soft skip
 *       (bRemoved=false, Note set, command success). Root parent → hard fail;
 *    3) lore_branch_reset(parent hex); failure here → hard fail (nothing changed yet);
 *    4) lore_file_reset([target], purge=0) — discard the file against the new tip;
 *       capture LORE_EVENT_FILE_RESET_FILE to confirm it acted on the target;
 *    5) if other files remain: dirty + stage(scan=1) + commit(original message);
 *       if none remain: skip the recommit (sole-file commit removed — intended);
 *    6) POST-COMMIT ASSERTION: re-read the new tip's delta and confirm it excludes
 *       the target and equals Remaining; any mismatch / missing reset event / nonzero
 *       sub-step → HARD fail with an explicit "uncommitted on disk, re-stage manually"
 *       message — never a silent half-state;
 *    7) full success: bRemoved=true.
 *  Execute runs on the POOL thread (worker-local scratch only); UpdateStates runs on
 *  the GAME thread and writes bRemoved/Note onto the captured operation. */
class FArcLoreRemoveFromOutgoingWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreRemoveFromOutgoingWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Result of the removal, written back to FLoreRemoveFromOutgoing in UpdateStates. */
	bool bRemoved = false;

	/** Soft-skip note (set when the file was not in the latest unpushed commit). */
	FString Note;

	/** The originating operation, captured in Execute so UpdateStates (which takes no
	 *  command) can write bRemoved/Note back to FLoreRemoveFromOutgoing. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** Non-interactive token login via lore_auth_login_with_token. One round-trip (no
 *  polling) so SAFE under RunLoreOp. Reads Token/TokenType from the captured operation,
 *  authenticates, and captures the resolved identity (LORE_EVENT_AUTH_USER_INFO id/name)
 *  into worker scratch. On success UpdateStates writes the result onto the operation AND
 *  persists the identity to the provider (UserName/UserId/AuthResolved). Execute runs on
 *  the POOL thread (worker-local scratch only); UpdateStates runs on the GAME thread. */
class FArcLoreLoginWithTokenWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreLoginWithTokenWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** True only when lore_auth_login_with_token succeeded. */
	bool bSuccess = false;

	/** Resolved display name captured from the AUTH_USER_INFO event (empty on failure). */
	FString UserName;

	/** Resolved user id captured from the AUTH_USER_INFO event (empty on failure). */
	FString UserId;

	/** The originating operation, captured in Execute so UpdateStates (which takes no
	 *  command) can write the result back to FLoreLoginWithToken. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** Browser-based OAuth login via lore_auth_login_interactive (sync variant). The call
 *  blocks the pool thread until LORE_EVENT_END (~150s self-timeout); lore.dll opens the
 *  OS default browser automatically (no_browser=0). Captures the resolved identity
 *  (LORE_EVENT_AUTH_USER_INFO id/name) into worker scratch. On success UpdateStates writes
 *  the result onto FLoreLoginInteractive AND persists the identity to the provider.
 *
 *  NOTE: the C ABI exposes no cancel handle for the interactive call; the op self-times-out
 *  (~150s) on the lore side; no cooperative cancel is possible from the UE side.
 *
 *  Execute runs on the POOL thread (worker-local scratch only); UpdateStates runs on the
 *  GAME thread. */
class FArcLoreLoginInteractiveWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreLoginInteractiveWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** True only when lore_auth_login_interactive succeeded. */
	bool bSuccess = false;

	/** Resolved display name captured from the AUTH_USER_INFO event (empty on failure). */
	FString UserName;

	/** Resolved user id captured from the AUTH_USER_INFO event (empty on failure). */
	FString UserId;

	/** The originating operation, captured in Execute so UpdateStates (which takes no
	 *  command) can write the result back to FLoreLoginInteractive. */
	TSharedPtr<ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** Logout via lore_auth_logout (all args empty == log out the current identity). No
 *  payload; UpdateStates clears the provider's cached identity (UserName/UserId/
 *  AuthResolved). Execute runs on the POOL thread; UpdateStates runs on the GAME thread. */
class FArcLoreLogoutWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreLogoutWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** True only when lore_auth_logout succeeded (drives whether UpdateStates clears identity). */
	bool bLoggedOut = false;
};

/** Switch the working copy to a different branch via lore_branch_switch (revision empty
 *  == that branch's tip). With reset=0 Lore HARD-ERRORS on a staged revision or on dirty
 *  files the incoming diff would overwrite; the worker captures that failure text verbatim
 *  into the operation's ErrorText (never masked). LORE_EVENT_BRANCH_MULTIPLE_INSTANCE is a
 *  non-fatal warning (logged). On success UpdateStates returns true so a state-changed
 *  broadcast refreshes the panel. Execute runs on the POOL thread (worker-local scratch
 *  only); UpdateStates runs on the GAME thread and writes the captured operation. */
class FArcLoreBranchSwitchWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreBranchSwitchWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** True only when lore_branch_switch succeeded (drives the UpdateStates broadcast). */
	bool bSucceeded = false;

	/** Lore's failure text captured verbatim on failure (empty on success). */
	FString ErrorText;

	/** The originating operation, captured in Execute so UpdateStates (which takes no
	 *  command) can write bSucceeded/ErrorText back to FLoreBranchSwitch. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** Create a new branch from the current revision via lore_branch_create. Does NOT switch.
 *  No payload back beyond bSucceeded. Execute runs on the POOL thread (worker-local
 *  scratch only); UpdateStates runs on the GAME thread and writes the captured operation. */
class FArcLoreBranchCreateWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreBranchCreateWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** True only when lore_branch_create succeeded. */
	bool bSucceeded = false;

	/** The originating operation, captured in Execute so UpdateStates (which takes no
	 *  command) can write bSucceeded back to FLoreBranchCreate. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};

/** Cross-branch commit DAG fetch: enumerate every branch (lore_branch_list), then
 *  walk each branch's history (lore_revision_history, bounded) under one
 *  FLoreGlobalArgsScope. Produces the raw FArcLoreBranchInput / FArcLoreRevisionInput
 *  arrays the pure-C++ layout builder consumes. Best-effort: a single branch's history
 *  failure is recorded but does not fail the command as long as the branch enumeration
 *  succeeded. Execute runs on the POOL thread (worker-local scratch only — N+1 serial
 *  RunLoreOp round-trips); UpdateStates runs on the GAME thread and writes the captured
 *  operation. */
class FArcLoreBranchGraphWorker : public IArcLoreSourceControlWorker
{
public:
	virtual ~FArcLoreBranchGraphWorker() {}
	// IArcLoreSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FArcLoreSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Branches collected on the lore worker thread from branch_list events; handed to
	 *  FLoreBranchGraph::Branches in UpdateStates. */
	TArray<FArcLoreBranchInput> Branches;

	/** Revisions collected across all per-branch history walks (duplicates kept; deduped
	 *  later in BuildLayout); handed to FLoreBranchGraph::Revisions in UpdateStates. */
	TArray<FArcLoreRevisionInput> Revisions;

	/** The originating operation, captured in Execute so UpdateStates (which takes no
	 *  command) can write the scratch arrays back to FLoreBranchGraph. */
	TSharedPtr<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
};
