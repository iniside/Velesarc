// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreSourceControlRevision.h"

#include "ISourceControlModule.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ArcLoreSourceControlModule.h"
#include "ArcLoreSourceControlUtils.h"

#define LOCTEXT_NAMESPACE "ArcLoreSourceControl"

// ---------------------------------------------------------------------------
// File-scoped helpers (unique namespace, not exposed in the header)
// ---------------------------------------------------------------------------
namespace ArcLoreRevisionHelpers
{
	/** Render a lore_hash_t as a 64-character lowercase hex string. */
	FString HashToHex(const lore_hash_t& Hash)
	{
		FString Result;
		Result.Reserve(64);
		for (int32 i = 0; i < 32; ++i)
		{
			Result += FString::Printf(TEXT("%02x"), Hash.data[i]);
		}
		return Result;
	}

	/** Map a lore_file_action_t to the action string used by ISourceControlRevision. */
	FString ActionToString(lore_file_action_t FileAction)
	{
		switch (FileAction)
		{
		case LORE_FILE_ACTION_ADD:    return TEXT("add");
		case LORE_FILE_ACTION_DELETE: return TEXT("delete");
		case LORE_FILE_ACTION_MOVE:   return TEXT("branch");
		case LORE_FILE_ACTION_COPY:   return TEXT("branch");
		case LORE_FILE_ACTION_KEEP:   // intentional fall-through
		default:                      return TEXT("edit");
		}
	}
} // namespace ArcLoreRevisionHelpers

// ---------------------------------------------------------------------------
// FArcLoreSourceControlRevision
// ---------------------------------------------------------------------------

void FArcLoreSourceControlRevision::InitFromHistory(const lore_file_history_event_data_t& Entry)
{
	Filename         = ArcLoreApi::LoreStringToFString(Entry.path);
	RevisionHashHex  = ArcLoreRevisionHelpers::HashToHex(Entry.revision);
	ShortRevisionHashHex = RevisionHashHex.Left(8);

	// revision_number is uint64; cast to int32 for the interface.
	// Values > INT32_MAX are extremely unlikely in normal repository usage.
	RevisionNumber   = static_cast<int32>(Entry.revision_number);

	// size is uint64; clamp to INT32_MAX to satisfy GetFileSize() return type.
	FileSize         = static_cast<int32>(FMath::Min<uint64>(Entry.size, static_cast<uint64>(INT32_MAX)));

	Action           = ArcLoreRevisionHelpers::ActionToString(Entry.action);
}

bool FArcLoreSourceControlRevision::Get(FString& InOutFilename, EConcurrency::Type InConcurrency) const
{
	if (InConcurrency != EConcurrency::Synchronous)
	{
		UE_LOGF(LogSourceControl, Warning,"Only EConcurrency::Synchronous is tested/supported for this operation.");
	}

	if (!ArcLoreApi::IsReady())
	{
		UE_LOGF(LogSourceControl, Warning,"FArcLoreSourceControlRevision::Get: Lore API not ready.");
		return false;
	}

	if (RevisionHashHex.IsEmpty())
	{
		UE_LOGF(LogSourceControl, Warning,"FArcLoreSourceControlRevision::Get: RevisionHashHex is empty — revision not populated.");
		return false;
	}

	// Generate a temp path if the caller did not supply one.
	if (InOutFilename.IsEmpty())
	{
		const FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Temp"), TEXT("LoreDiff"));
		IFileManager::Get().MakeDirectory(*TempDir, /*Tree=*/true);
		InOutFilename = FPaths::Combine(TempDir,
			FString::Printf(TEXT("temp-%s-%s"), *ShortRevisionHashHex, *FPaths::GetCleanFilename(Filename)));
		InOutFilename = FPaths::ConvertRelativePathToFull(InOutFilename);
	}

	// Reuse the temp file if it already exists (same logic as the old git path).
	if (FPaths::FileExists(InOutFilename))
	{
		return true;
	}

	FArcLoreSourceControlModule& LoreModule =
		FModuleManager::LoadModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	const FString RepoRoot = LoreModule.GetProvider().GetPathToRepositoryRoot();
	// GetUserName() holds the authenticated Lore identity (resolved by UpdateStatus).
	const FString Identity = LoreModule.GetProvider().GetUserName();

	// Build args. lore_file_write writes repo-path@revision to an output filesystem path.
	// lore resolves a RELATIVE source path against the process CWD (…/Engine/Binaries/Win64),
	// NOT the repo. Filename is the repo-relative path lore echoed on the history event
	// (InitFromHistory), so make it absolute (repo root + relative) before passing it.
	const FString AbsSourcePath =
		FPaths::IsRelative(Filename) ? FPaths::Combine(RepoRoot, Filename) : Filename;
	ArcLoreApi::FLoreUtf8String LorePath(AbsSourcePath);
	ArcLoreApi::FLoreUtf8String LoreRevision(RevisionHashHex);
	ArcLoreApi::FLoreUtf8String LoreOutput(InOutFilename);

	lore_file_write_args_t WriteArgs{};
	// address is left empty — we address by path + revision.
	WriteArgs.address  = ArcLoreApi::EmptyLoreString();
	WriteArgs.path     = LorePath.View();
	WriteArgs.revision = LoreRevision.View();
	WriteArgs.output   = LoreOutput.View();

	ArcLoreApi::FLoreGlobalArgsScope GlobalArgs(RepoRoot, Identity);

	FString Error;
	const int32 Result = ArcLoreApi::RunLoreOp(
		[&GlobalArgs, &WriteArgs](const lore_event_callback_config_t& Config) -> int32
		{
			return lore_file_write(&GlobalArgs.Args(), &WriteArgs, Config);
		},
		[](const lore_event_t& /*Event*/) { /* no domain events to handle */ },
		Error
	);

	if (Result != 0)
	{
		UE_LOGF(LogSourceControl, Warning,"FArcLoreSourceControlRevision::Get: lore_file_write failed for %ls@%ls: %ls",
			*Filename, *RevisionHashHex, *Error);
		return false;
	}

	return true;
}

bool FArcLoreSourceControlRevision::GetAnnotated( TArray<FAnnotationLine>& OutLines ) const
{
	// TODO: implement annotation (git-blame equivalent) via Lore API — deferred MVP.
	return false;
}

bool FArcLoreSourceControlRevision::GetAnnotated( FString& InOutFilename ) const
{
	// TODO: implement annotation (git-blame to file equivalent) via Lore API — deferred MVP.
	return false;
}

const FString& FArcLoreSourceControlRevision::GetFilename() const
{
	return Filename;
}

int32 FArcLoreSourceControlRevision::GetRevisionNumber() const
{
	return RevisionNumber;
}

const FString& FArcLoreSourceControlRevision::GetRevision() const
{
	// Short hash (first 8 hex chars) is the human-readable display identifier.
	return ShortRevisionHashHex;
}

const FString& FArcLoreSourceControlRevision::GetDescription() const
{
	return Description;
}

const FString& FArcLoreSourceControlRevision::GetUserName() const
{
	return UserName;
}

const FString& FArcLoreSourceControlRevision::GetClientSpec() const
{
	// Lore has no client-spec concept.
	static const FString EmptyString(TEXT(""));
	return EmptyString;
}

const FString& FArcLoreSourceControlRevision::GetAction() const
{
	return Action;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FArcLoreSourceControlRevision::GetBranchSource() const
{
	// MVP: branch-source tracking deferred.
	return nullptr;
}

const FDateTime& FArcLoreSourceControlRevision::GetDate() const
{
	return Date;
}

int32 FArcLoreSourceControlRevision::GetCheckInIdentifier() const
{
	// Map to RevisionNumber — Lore's sequential revision index is the closest
	// analogue to Perforce's check-in identifier.
	return RevisionNumber;
}

int32 FArcLoreSourceControlRevision::GetFileSize() const
{
	return FileSize;
}

#undef LOCTEXT_NAMESPACE
