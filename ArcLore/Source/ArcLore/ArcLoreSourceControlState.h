// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ArcLoreSourceControlRevision.h"
#include "ArcLoreApi.h"

namespace EArcLoreState
{
	// Working-copy state derived from Lore repository-status flags. Git-only
	// Missing/Copied were dropped: a Lore COPY action maps to Modified, and a
	// locally-removed file maps to Deleted (see DeriveWorkingState).
	// Renamed is used for Lore MOVE actions (source path becomes the new dest path).
	enum Type
	{
		Unknown,
		Unchanged, // called "clean" in SVN, "Pristine" in Perforce
		Added,
		Deleted,
		Modified,
		Renamed,
		Conflicted,
		NotControlled,
		Ignored,
	};
}

// Derives the working-copy state from a single Lore repository-status file event.
// Defined here so the (later) status worker can call it; implemented in the .cpp
// inside a unique file-scoped namespace.
EArcLoreState::Type DeriveWorkingState(const lore_repository_status_file_event_data_t& FileEvent);

class FArcLoreSourceControlState : public ISourceControlState
{
public:
	FArcLoreSourceControlState( const FString& InLocalFilename )
		: LocalFilename(InLocalFilename)
		, WorkingCopyState(EArcLoreState::Unknown)
		, TimeStamp(0)
	{
	}

	FArcLoreSourceControlState(const FArcLoreSourceControlState& Other);
	FArcLoreSourceControlState(FArcLoreSourceControlState&& Other) noexcept;
	FArcLoreSourceControlState& operator=(const FArcLoreSourceControlState& Other);
	FArcLoreSourceControlState& operator=(FArcLoreSourceControlState&& Other) noexcept;

	/** ISourceControlState interface */
	virtual int32 GetHistorySize() const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetHistoryItem(int32 HistoryIndex) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision(int32 RevisionNumber) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision(const FString& InRevision) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetCurrentRevision() const override;
	virtual FResolveInfo GetResolveInfo() const override;
#if SOURCE_CONTROL_WITH_SLATE
	virtual FSlateIcon GetIcon() const override;

	/** Returns the icon for any EArcLoreState::Type value without needing a full state object.
	 *  GetIcon() delegates here after resolving any pre-switch special cases (lock-as-checkout). */
	static FSlateIcon GetIconForState(EArcLoreState::Type State);
#endif //SOURCE_CONTROL_WITH_SLATE
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayTooltip() const override;
	virtual const FString& GetFilename() const override;
	virtual const FDateTime& GetTimeStamp() const override;
	virtual bool CanCheckIn() const override;
	virtual bool CanCheckout() const override;
	virtual bool IsCheckedOut() const override;
	virtual bool IsCheckedOutOther(FString* Who = nullptr) const override;
	// Lore uses single-holder locks and does not model cross-branch modification, so these are always false.
	virtual bool IsCheckedOutInOtherBranch(const FString& CurrentBranch = FString()) const override { return false; }
	virtual bool IsModifiedInOtherBranch(const FString& CurrentBranch = FString()) const override { return false; }
	virtual bool IsCheckedOutOrModifiedInOtherBranch(const FString& CurrentBranch = FString()) const override { return false; }
	virtual TArray<FString> GetCheckedOutBranches() const override;
	virtual FString GetOtherUserBranchCheckedOuts() const override { return FString(); }
	virtual bool GetOtherBranchHeadModification(FString& HeadBranchOut, FString& ActionOut, int32& HeadChangeListOut) const override { return false; }
	virtual bool IsCurrent() const override;
	virtual bool IsSourceControlled() const override;
	virtual bool IsAdded() const override;
	virtual bool IsDeleted() const override;
	virtual bool IsIgnored() const override;
	virtual bool CanEdit() const override;
	virtual bool IsUnknown() const override;
	virtual bool IsModified() const override;
	virtual bool CanAdd() const override;
	virtual bool CanDelete() const override;
	virtual bool IsConflicted() const override;
	virtual bool CanRevert() const override;

	/** True when any user holds a lock on this path (convenience for panel display). */
	bool IsLocked() const { return bIsLocked; }

	/** Dedicated lock predicates (NOT on ISourceControlState — reach via downcast).
	 *  Decoupled from UE checkout so saving a tracked asset never prompts. */
	bool CanLock() const;
	bool IsLockedByMe() const;

public:
	/** History of the item, if any */
	TArcLoreSourceControlHistory History;

	/** Filename on disk */
	FString LocalFilename;

	/** Pending rev info with which a file must be resolved, invalid if no resolve pending */
	FResolveInfo PendingResolveInfo;

	UE_DEPRECATED(5.3, "Use PendingResolveInfo.BaseRevision instead")
	FString PendingMergeBaseFileHash;

	/** State of the working copy */
	EArcLoreState::Type WorkingCopyState;

	// true when the change is in Lore's staged changeset (any action), independent of WorkingCopyState
	bool bStaged = false;

	/** The timestamp of the last update */
	FDateTime TimeStamp;

	// Lock cache (lock-as-checkout). Populated by the status/lock worker (Step 8).
	bool      bIsLocked = false;       // true when any user holds a lock on this path
	FString   LockOwner;               // user id holding the lock (empty when unlocked)
	FString   LockBranch;              // branch the lock was acquired on (empty for status-only queries)
	FDateTime LockAcquiredAt;          // displayable server time, from lore locked_at
	double    CachedAtSeconds = 0.0;   // FPlatformTime::Seconds() at refresh, for TTL math
	bool      bLockedByMe = false;     // set by the worker by comparing LockOwner to the provider's user id

	// Remote tracking: this path has a newer revision on the remote (drives IsCurrent()).
	bool bIsNewerOnRemote = false;
};
