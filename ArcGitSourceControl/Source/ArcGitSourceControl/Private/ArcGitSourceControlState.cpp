// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcGitSourceControlState.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Textures/SlateIcon.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "ArcGitSourceControl.State"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FArcGitSourceControlState::FArcGitSourceControlState(const FArcGitSourceControlState& Other) = default;
FArcGitSourceControlState::FArcGitSourceControlState(FArcGitSourceControlState&& Other) noexcept = default;
FArcGitSourceControlState& FArcGitSourceControlState::operator=(const FArcGitSourceControlState& Other) = default;
FArcGitSourceControlState& FArcGitSourceControlState::operator=(FArcGitSourceControlState&& Other) noexcept = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

int32 FArcGitSourceControlState::GetHistorySize() const
{
	return History.Num();
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FArcGitSourceControlState::GetHistoryItem( int32 HistoryIndex ) const
{
	check(History.IsValidIndex(HistoryIndex));
	return History[HistoryIndex];
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FArcGitSourceControlState::FindHistoryRevision( int32 RevisionNumber ) const
{
	for(const auto& Revision : History)
	{
		if(Revision->GetRevisionNumber() == RevisionNumber)
		{
			return Revision;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FArcGitSourceControlState::FindHistoryRevision(const FString& InRevision) const
{
	// short hash must be >= 7 characters to have a reasonable probability of finding the correct revision
	if (!ensure(InRevision.Len() >= 7))
	{
		return nullptr;
	}

	for(const auto& Revision : History)
	{
		// support for short hashes
		const int32 Len = FMath::Min(Revision->CommitId.Len(), InRevision.Len());

		if(Revision->CommitId.Left(Len) == InRevision.Left(Len))
		{
			return Revision;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FArcGitSourceControlState::GetCurrentRevision() const
{
	return nullptr;
}

ISourceControlState::FResolveInfo FArcGitSourceControlState::GetResolveInfo() const
{
	return PendingResolveInfo;
}

#if SOURCE_CONTROL_WITH_SLATE

FSlateIcon FArcGitSourceControlState::GetIcon() const
{
	switch (WorkingCopyState)
	{
	case EArcGitState::Modified:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOut");
	case EArcGitState::Added:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.OpenForAdd");
	case EArcGitState::Renamed:
	case EArcGitState::Copied:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Branched");
	case EArcGitState::Deleted: // Deleted & Missing files does not show in Content Browser
	case EArcGitState::Missing:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.MarkedForDelete");
	case EArcGitState::Conflicted:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Conflicted");
	case EArcGitState::NotControlled:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.NotInDepot");
	case EArcGitState::Unknown:
	case EArcGitState::Unchanged: // Unchanged is the same as "Pristine" (not checked out) for Perforce, ie no icon
	case EArcGitState::Ignored:
	default:
		return FSlateIcon();
	}
}

#endif //SOURCE_CONTROL_WITH_SLATE


FText FArcGitSourceControlState::GetDisplayName() const
{
	switch(WorkingCopyState)
	{
	case EArcGitState::Unknown:
		return LOCTEXT("Unknown", "Unknown");
	case EArcGitState::Unchanged:
		return LOCTEXT("Unchanged", "Unchanged");
	case EArcGitState::Added:
		return LOCTEXT("Added", "Added");
	case EArcGitState::Deleted:
		return LOCTEXT("Deleted", "Deleted");
	case EArcGitState::Modified:
		return LOCTEXT("Modified", "Modified");
	case EArcGitState::Renamed:
		return LOCTEXT("Renamed", "Renamed");
	case EArcGitState::Copied:
		return LOCTEXT("Copied", "Copied");
	case EArcGitState::Conflicted:
		return LOCTEXT("ContentsConflict", "Contents Conflict");
	case EArcGitState::Ignored:
		return LOCTEXT("Ignored", "Ignored");
	case EArcGitState::NotControlled:
		return LOCTEXT("NotControlled", "Not Under Revision Control");
	case EArcGitState::Missing:
		return LOCTEXT("Missing", "Missing");
	}

	return FText();
}

FText FArcGitSourceControlState::GetDisplayTooltip() const
{
	switch(WorkingCopyState)
	{
	case EArcGitState::Unknown:
		return LOCTEXT("Unknown_Tooltip", "Unknown revision control state");
	case EArcGitState::Unchanged:
		return LOCTEXT("Pristine_Tooltip", "There are no modifications");
	case EArcGitState::Added:
		return LOCTEXT("Added_Tooltip", "Item is scheduled for addition");
	case EArcGitState::Deleted:
		return LOCTEXT("Deleted_Tooltip", "Item is scheduled for deletion");
	case EArcGitState::Modified:
		return LOCTEXT("Modified_Tooltip", "Item has been modified");
	case EArcGitState::Renamed:
		return LOCTEXT("Renamed_Tooltip", "Item has been renamed");
	case EArcGitState::Copied:
		return LOCTEXT("Copied_Tooltip", "Item has been copied");
	case EArcGitState::Conflicted:
		return LOCTEXT("ContentsConflict_Tooltip", "The contents of the item conflict with updates received from the repository.");
	case EArcGitState::Ignored:
		return LOCTEXT("Ignored_Tooltip", "Item is being ignored.");
	case EArcGitState::NotControlled:
		return LOCTEXT("NotControlled_Tooltip", "Item is not under version control.");
	case EArcGitState::Missing:
		return LOCTEXT("Missing_Tooltip", "Item is missing (e.g., you moved or deleted it without using Git). This also indicates that a directory is incomplete (a checkout or update was interrupted).");
	}

	return FText();
}

const FString& FArcGitSourceControlState::GetFilename() const
{
	return LocalFilename;
}

const FDateTime& FArcGitSourceControlState::GetTimeStamp() const
{
	return TimeStamp;
}

// Deleted and Missing assets cannot appear in the Content Browser, but the do in the Submit files to Source Control window!
bool FArcGitSourceControlState::CanCheckIn() const
{
	return WorkingCopyState == EArcGitState::Added
		|| WorkingCopyState == EArcGitState::Deleted
		|| WorkingCopyState == EArcGitState::Missing
		|| WorkingCopyState == EArcGitState::Modified
		|| WorkingCopyState == EArcGitState::Renamed;
}

bool FArcGitSourceControlState::CanCheckout() const
{
	return bIsLfsTracked && LockState == EArcGitLockState::NotLocked;
}

bool FArcGitSourceControlState::IsCheckedOut() const
{
	return LockState == EArcGitLockState::LockedByMe;
}

bool FArcGitSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if (LockState == EArcGitLockState::LockedByOther)
	{
		if (Who) { *Who = LockUser; }
		return true;
	}
	return false;
}

bool FArcGitSourceControlState::IsCurrent() const
{
	return !bIsNewerOnRemote;
}

bool FArcGitSourceControlState::IsSourceControlled() const
{
	return WorkingCopyState != EArcGitState::NotControlled && WorkingCopyState != EArcGitState::Ignored && WorkingCopyState != EArcGitState::Unknown;
}

bool FArcGitSourceControlState::IsAdded() const
{
	return WorkingCopyState == EArcGitState::Added;
}

bool FArcGitSourceControlState::IsDeleted() const
{
	return WorkingCopyState == EArcGitState::Deleted || WorkingCopyState == EArcGitState::Missing;
}

bool FArcGitSourceControlState::IsIgnored() const
{
	return WorkingCopyState == EArcGitState::Ignored;
}

bool FArcGitSourceControlState::CanEdit() const
{
	return !bIsLfsTracked || LockState == EArcGitLockState::LockedByMe || !IFileManager::Get().IsReadOnly(*LocalFilename);
}

bool FArcGitSourceControlState::CanDelete() const
{
	return IsSourceControlled() && IsCurrent();
}

bool FArcGitSourceControlState::IsUnknown() const
{
	return WorkingCopyState == EArcGitState::Unknown;
}

bool FArcGitSourceControlState::IsModified() const
{
	// Warning: for Perforce, a checked-out file is locked for modification (whereas with Git all tracked files are checked-out),
	// so for a clean "check-in" (commit) checked-out files unmodified should be removed from the changeset (the index)
	// http://stackoverflow.com/questions/12357971/what-does-revert-unchanged-files-mean-in-perforce
	//
	// Thus, before check-in UE Editor call RevertUnchangedFiles() in PromptForCheckin() and CheckinFiles().
	//
	// So here we must take care to enumerate all states that need to be commited,
	// all other will be discarded :
	//  - Unknown
	//  - Unchanged
	//  - NotControlled
	//  - Ignored
	return WorkingCopyState == EArcGitState::Added
		|| WorkingCopyState == EArcGitState::Deleted
		|| WorkingCopyState == EArcGitState::Modified
		|| WorkingCopyState == EArcGitState::Renamed
		|| WorkingCopyState == EArcGitState::Copied
		|| WorkingCopyState == EArcGitState::Conflicted
		|| WorkingCopyState == EArcGitState::Missing;
}


bool FArcGitSourceControlState::CanAdd() const
{
	return WorkingCopyState == EArcGitState::NotControlled;
}

bool FArcGitSourceControlState::IsConflicted() const
{
	return WorkingCopyState == EArcGitState::Conflicted;
}

bool FArcGitSourceControlState::CanRevert() const
{
	return CanCheckIn() || IsCheckedOut();
}

#undef LOCTEXT_NAMESPACE
