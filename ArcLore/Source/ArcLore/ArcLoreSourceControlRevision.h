// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlRevision.h"
#include "Misc/DateTime.h"
#include "ArcLoreApi.h"

/** Revision of a file, linked to a specific Lore revision. */
class FArcLoreSourceControlRevision : public ISourceControlRevision
{
public:
	FArcLoreSourceControlRevision()
		: RevisionNumber(0)
		, FileSize(0)
	{
	}

	/** ISourceControlRevision interface */
	virtual bool Get(FString& InOutFilename, EConcurrency::Type InConcurrency = EConcurrency::Synchronous) const override;
	virtual bool GetAnnotated( TArray<FAnnotationLine>& OutLines ) const override;
	virtual bool GetAnnotated( FString& InOutFilename ) const override;
	virtual const FString& GetFilename() const override;
	virtual int32 GetRevisionNumber() const override;
	virtual const FString& GetRevision() const override;
	virtual const FString& GetDescription() const override;
	virtual const FString& GetUserName() const override;
	virtual const FString& GetClientSpec() const override;
	virtual const FString& GetAction() const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetBranchSource() const override;
	virtual const FDateTime& GetDate() const override;
	virtual int32 GetCheckInIdentifier() const override;
	virtual int32 GetFileSize() const override;

	/**
	 * Populate core fields from a lore_file_history_event_data_t.
	 * Called by the Step-8 UpdateStatus worker for each LORE_EVENT_FILE_HISTORY event.
	 * Metadata fields (Description, UserName, Date) are set separately via the
	 * individual setters below when the worker processes LORE_EVENT_METADATA events.
	 */
	void InitFromHistory(const lore_file_history_event_data_t& Entry);

	/** Called by the Step-8 worker when it sees a METADATA key="message" event. */
	void SetDescription(const FString& InDescription) { Description = InDescription; }

	/** Called by the Step-8 worker when it sees a METADATA key="created-by" event. */
	void SetUserName(const FString& InUserName) { UserName = InUserName; }

	/**
	 * Called by the Step-8 worker when it sees a METADATA key="timestamp" event.
	 * The Lore timestamp is a Unix epoch value in MILLISECONDS (u64,
	 * chrono::Utc::now().timestamp_millis()); divide by 1000 for the seconds API.
	 */
	void SetDateFromEpochMillis(uint64 EpochMillis)
	{
		Date = FDateTime::FromUnixTimestamp(static_cast<int64>(EpochMillis / 1000));
	}

	/**
	 * True when this revision is committed locally but NOT yet present on the
	 * remote (revision_number greater than the remote branch tip number, with the
	 * remote known). Only the revision-log worker sets this; file-history
	 * revisions leave it false and render normally.
	 */
	bool IsLocalOnly() const { return bIsLocalOnly; }

public:

	/** The filename this revision refers to (repo-relative path). */
	FString Filename;

	/**
	 * Full 64-hex-character string representation of the lore_hash_t revision.
	 * Used for stable identity and as the revision arg to lore_file_write.
	 */
	FString RevisionHashHex;

	/**
	 * Short display string — first 8 hex characters of RevisionHashHex.
	 * Returned by GetRevision().
	 */
	FString ShortRevisionHashHex;

	/**
	 * Sequential revision number from lore_file_history_event_data_t::revision_number.
	 * Cast from uint64 to int32 for the ISourceControlRevision interface; values
	 * beyond INT32_MAX are extremely unlikely in practice but the truncation is noted.
	 */
	int32 RevisionNumber;

	/** Commit message; populated from METADATA key="message". */
	FString Description;

	/** Author identity; populated from METADATA key="created-by". */
	FString UserName;

	/** Action string ("add", "edit", "delete", "branch"). */
	FString Action;

	/** Timestamp; populated from METADATA key="timestamp" (Unix epoch milliseconds). */
	FDateTime Date;

	/** Size of the file in bytes at this revision (from lore_file_history_event_data_t::size). */
	int32 FileSize;

	/**
	 * True when this revision is committed locally but not yet on the remote.
	 * Set only by FArcLoreRevisionLogWorker (after a revision_only repository
	 * status pre-pass); defaults false so revisions that never get marked
	 * (e.g. per-file history) render as normal/pushed.
	 */
	bool bIsLocalOnly = false;
};

/** History composed of the last N revisions of the file */
typedef TArray< TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe> >	TArcLoreSourceControlHistory;
