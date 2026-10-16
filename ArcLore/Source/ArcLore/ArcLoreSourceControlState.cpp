// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreSourceControlState.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Textures/SlateIcon.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "ArcLoreSourceControl.State"

namespace ArcLoreSourceControlState_Internal
{
	// Maps a Lore repository-status file event to the working-copy enum.
	// Conflict flags win first, then the action verb. MOVE maps to Renamed;
	// COPY collapses to Modified (content-copied, not a rename); flag_dirty on
	// an otherwise-KEEP entry also reports Modified.
	EArcLoreState::Type DeriveWorkingStateImpl(const lore_repository_status_file_event_data_t& FileEvent)
	{
		if (FileEvent.flag_conflict != 0)
		{
			return EArcLoreState::Conflicted;
		}

		switch (FileEvent.action)
		{
		case LORE_FILE_ACTION_ADD:
			// A STAGED add is a file explicitly marked for addition (Added). An
			// UNSTAGED add is an untracked file the scan discovered (it has no
			// tracked node) — report it NotControlled so ISourceControlState::CanAdd()
			// returns true and the editor enables "Mark for Add". (Flag split is the
			// most-likely mapping; confirm against the [UpdateStatus/diag] logs.)
			return (FileEvent.flag_staged != 0) ? EArcLoreState::Added : EArcLoreState::NotControlled;
		case LORE_FILE_ACTION_DELETE:
			return EArcLoreState::Deleted;
		case LORE_FILE_ACTION_MOVE:
			return EArcLoreState::Renamed;
		case LORE_FILE_ACTION_COPY:
			return EArcLoreState::Modified;
		case LORE_FILE_ACTION_KEEP:
		default:
			return (FileEvent.flag_dirty != 0) ? EArcLoreState::Modified : EArcLoreState::Unchanged;
		}
	}
}

EArcLoreState::Type DeriveWorkingState(const lore_repository_status_file_event_data_t& FileEvent)
{
	return ArcLoreSourceControlState_Internal::DeriveWorkingStateImpl(FileEvent);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FArcLoreSourceControlState::FArcLoreSourceControlState(const FArcLoreSourceControlState& Other) = default;
FArcLoreSourceControlState::FArcLoreSourceControlState(FArcLoreSourceControlState&& Other) noexcept = default;
FArcLoreSourceControlState& FArcLoreSourceControlState::operator=(const FArcLoreSourceControlState& Other) = default;
FArcLoreSourceControlState& FArcLoreSourceControlState::operator=(FArcLoreSourceControlState&& Other) noexcept = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

int32 FArcLoreSourceControlState::GetHistorySize() const
{
	return History.Num();
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FArcLoreSourceControlState::GetHistoryItem( int32 HistoryIndex ) const
{
	check(History.IsValidIndex(HistoryIndex));
	return History[HistoryIndex];
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FArcLoreSourceControlState::FindHistoryRevision( int32 RevisionNumber ) const
{
	for(const TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>& Revision : History)
	{
		if(Revision->GetRevisionNumber() == RevisionNumber)
		{
			return Revision;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FArcLoreSourceControlState::FindHistoryRevision(const FString& InRevision) const
{
	// short hash must be >= 7 characters to have a reasonable probability of finding the correct revision
	if (!ensure(InRevision.Len() >= 7))
	{
		return nullptr;
	}

	for(const TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>& Revision : History)
	{
		// TODO Step 8: match against RevisionHashHex (Lore) once the UpdateStatus
		// worker populates history via InitFromHistory. Old CommitId field removed.
		const int32 Len = FMath::Min(Revision->RevisionHashHex.Len(), InRevision.Len());

		if(Revision->RevisionHashHex.Left(Len) == InRevision.Left(Len))
		{
			return Revision;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FArcLoreSourceControlState::GetCurrentRevision() const
{
	return nullptr;
}

ISourceControlState::FResolveInfo FArcLoreSourceControlState::GetResolveInfo() const
{
	return PendingResolveInfo;
}

#if SOURCE_CONTROL_WITH_SLATE

FSlateIcon FArcLoreSourceControlState::GetIconForState(EArcLoreState::Type State)
{
	switch (State)
	{
	case EArcLoreState::Modified:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOut");
	case EArcLoreState::Renamed:
		// A renamed file is a pending structural change; reuse the checked-out brush
		// (same pending-edit family as Modified) until a dedicated rename brush exists.
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOut");
	case EArcLoreState::Added:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.OpenForAdd");
	case EArcLoreState::Deleted: // Deleted files do not show in Content Browser
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.MarkedForDelete");
	case EArcLoreState::Conflicted:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Conflicted");
	case EArcLoreState::NotControlled:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.NotInDepot");
	case EArcLoreState::Unknown:
	case EArcLoreState::Unchanged: // Unchanged is the same as "Pristine" (not checked out) for Perforce, ie no icon
	case EArcLoreState::Ignored:
	default:
		return FSlateIcon();
	}
}

FSlateIcon FArcLoreSourceControlState::GetIcon() const
{
	// A lock held by ANOTHER user dominates — the file cannot be edited/committed locally,
	// so that blocking state is surfaced over any local working-copy state.
	if (IsCheckedOutOther())
	{
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOutByOtherUser");
	}

	// Action wins over a lock held BY ME: a local working-copy action (Added/Deleted/
	// Modified/Renamed/Conflicted/NotControlled) is shown even when this file is also locked
	// by me. A new/added file I happen to hold a lock on must read as its action icon (e.g.
	// "+"), not as a plain checked-out green check. GetIconForState returns an unset icon for
	// Unchanged/Unknown/Ignored, so IsSet() means "there is an action to surface".
	const FSlateIcon ActionIcon = GetIconForState(WorkingCopyState);
	if (ActionIcon.IsSet())
	{
		return ActionIcon;
	}

	// No working-copy action to surface: a lock held by me reads as checked-out (a pure
	// checkout of an otherwise-unchanged tracked file).
	if (IsLockedByMe())
	{
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOut");
	}

	// Out-of-date badge: the remote holds a newer revision of THIS asset (IsCurrent() is
	// !bIsNewerOnRemote). Only when there is no local change/lock to surface instead.
	if (!IsCurrent()
		&& (WorkingCopyState == EArcLoreState::Unchanged || WorkingCopyState == EArcLoreState::Unknown))
	{
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.NotAtHeadRevision");
	}

	return ActionIcon; // unset (Unchanged/Unknown/Ignored)
}

#endif //SOURCE_CONTROL_WITH_SLATE


FText FArcLoreSourceControlState::GetDisplayName() const
{
	switch(WorkingCopyState)
	{
	case EArcLoreState::Unknown:
		return LOCTEXT("Unknown", "Unknown");
	case EArcLoreState::Unchanged:
		return LOCTEXT("Unchanged", "Unchanged");
	case EArcLoreState::Added:
		return LOCTEXT("Added", "Added");
	case EArcLoreState::Deleted:
		return LOCTEXT("Deleted", "Deleted");
	case EArcLoreState::Modified:
		return LOCTEXT("Modified", "Modified");
	case EArcLoreState::Renamed:
		return LOCTEXT("Renamed", "Renamed");
	case EArcLoreState::Conflicted:
		return LOCTEXT("ContentsConflict", "Contents Conflict");
	case EArcLoreState::Ignored:
		return LOCTEXT("Ignored", "Ignored");
	case EArcLoreState::NotControlled:
		return LOCTEXT("NotControlled", "Not Under Revision Control");
	}

	return FText();
}

FText FArcLoreSourceControlState::GetDisplayTooltip() const
{
	// Locked-by-other surfaces who holds the lock and on which branch.
	if (bIsLocked && !bLockedByMe)
	{
		return FText::Format(
			LOCTEXT("LockedByOther_Tooltip", "Locked by {0} on {1}"),
			FText::FromString(LockOwner),
			FText::FromString(LockBranch));
	}

	switch(WorkingCopyState)
	{
	case EArcLoreState::Unknown:
		return LOCTEXT("Unknown_Tooltip", "Unknown revision control state");
	case EArcLoreState::Unchanged:
		return LOCTEXT("Pristine_Tooltip", "There are no modifications");
	case EArcLoreState::Added:
		return LOCTEXT("Added_Tooltip", "Item is scheduled for addition");
	case EArcLoreState::Deleted:
		return LOCTEXT("Deleted_Tooltip", "Item is scheduled for deletion");
	case EArcLoreState::Modified:
		return LOCTEXT("Modified_Tooltip", "Item has been modified");
	case EArcLoreState::Renamed:
		return LOCTEXT("Renamed_Tooltip", "Item has been renamed (moved) and is pending commit.");
	case EArcLoreState::Conflicted:
		return LOCTEXT("ContentsConflict_Tooltip", "The contents of the item conflict with updates received from the repository.");
	case EArcLoreState::Ignored:
		return LOCTEXT("Ignored_Tooltip", "Item is being ignored.");
	case EArcLoreState::NotControlled:
		return LOCTEXT("NotControlled_Tooltip", "Item is not under version control.");
	}

	return FText();
}

const FString& FArcLoreSourceControlState::GetFilename() const
{
	return LocalFilename;
}

const FDateTime& FArcLoreSourceControlState::GetTimeStamp() const
{
	// Lock-as-checkout: surface the lock acquisition time as the state's timestamp.
	return TimeStamp;
}

bool FArcLoreSourceControlState::CanCheckIn() const
{
	// Submittable when there are pending local changes to commit (added / modified
	// / deleted), OR when this user holds the lock (lock-as-checkout edit of an
	// otherwise-unchanged file). A newly Added file is never locked, so requiring a
	// lock here made it impossible to submit; pending-change states must qualify too.
	// Conflicted is excluded: it must be resolved before it can be checked in.
	const bool bHasPendingChange =
		WorkingCopyState == EArcLoreState::Added
		|| WorkingCopyState == EArcLoreState::Modified
		|| WorkingCopyState == EArcLoreState::Renamed
		|| WorkingCopyState == EArcLoreState::Deleted;
	return bHasPendingChange;
}

bool FArcLoreSourceControlState::CanLock() const
{
	return !bIsLocked;
}

bool FArcLoreSourceControlState::IsLockedByMe() const
{
	return bIsLocked && bLockedByMe;
}

bool FArcLoreSourceControlState::CanCheckout() const
{
	return false;
}

bool FArcLoreSourceControlState::IsCheckedOut() const
{
	return IsSourceControlled() && !IsCheckedOutOther();
}

bool FArcLoreSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if (bIsLocked && !bLockedByMe)
	{
		if (Who) { *Who = LockOwner; }
		return true;
	}
	return false;
}

TArray<FString> FArcLoreSourceControlState::GetCheckedOutBranches() const
{
	// Lock-as-checkout: a lock held by another user is reported as a checkout on its branch.
	if (bIsLocked && !bLockedByMe && !LockBranch.IsEmpty())
	{
		return TArray<FString>{ LockBranch };
	}
	return TArray<FString>();
}

bool FArcLoreSourceControlState::IsCurrent() const
{
	return !bIsNewerOnRemote;
}

bool FArcLoreSourceControlState::IsSourceControlled() const
{
	return WorkingCopyState != EArcLoreState::NotControlled && WorkingCopyState != EArcLoreState::Ignored && WorkingCopyState != EArcLoreState::Unknown;
}

bool FArcLoreSourceControlState::IsAdded() const
{
	return WorkingCopyState == EArcLoreState::Added;
}

bool FArcLoreSourceControlState::IsDeleted() const
{
	return WorkingCopyState == EArcLoreState::Deleted;
}

bool FArcLoreSourceControlState::IsIgnored() const
{
	return WorkingCopyState == EArcLoreState::Ignored;
}

bool FArcLoreSourceControlState::CanEdit() const
{
	return IsSourceControlled();
}

bool FArcLoreSourceControlState::CanDelete() const
{
	return IsSourceControlled() && IsCurrent();
}

bool FArcLoreSourceControlState::IsUnknown() const
{
	return WorkingCopyState == EArcLoreState::Unknown;
}

bool FArcLoreSourceControlState::IsModified() const
{
	return WorkingCopyState == EArcLoreState::Added
		|| WorkingCopyState == EArcLoreState::Deleted
		|| WorkingCopyState == EArcLoreState::Modified
		|| WorkingCopyState == EArcLoreState::Renamed
		|| WorkingCopyState == EArcLoreState::Conflicted;
}


bool FArcLoreSourceControlState::CanAdd() const
{
	return WorkingCopyState == EArcLoreState::NotControlled;
}

bool FArcLoreSourceControlState::IsConflicted() const
{
	return WorkingCopyState == EArcLoreState::Conflicted;
}

bool FArcLoreSourceControlState::CanRevert() const
{
	return CanCheckIn();
}

#undef LOCTEXT_NAMESPACE
