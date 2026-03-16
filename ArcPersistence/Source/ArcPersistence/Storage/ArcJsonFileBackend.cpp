/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Storage/ArcJsonFileBackend.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FArcJsonFileBackend::FArcJsonFileBackend(const FString& InBaseDirectory)
	: BaseDirectory(InBaseDirectory)
{
	// Normalize the base directory path
	FPaths::NormalizeDirectoryName(BaseDirectory);
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

FString FArcJsonFileBackend::KeyToFilePath(const FString& Key) const
{
	return BaseDirectory / Key + TEXT(".json");
}

// ---------------------------------------------------------------------------
// Public async API — enqueue onto the serial task queue
// ---------------------------------------------------------------------------

TFuture<FArcPersistenceResult> FArcJsonFileBackend::SaveEntry(const FString& Key, TArray<uint8> Data)
{
	return TaskQueue.Enqueue<FArcPersistenceResult>(
		[this, Key, Data = MoveTemp(Data)]()
		{
			return SaveEntrySync(Key, Data);
		});
}

TFuture<FArcPersistenceLoadResult> FArcJsonFileBackend::LoadEntry(const FString& Key)
{
	return TaskQueue.Enqueue<FArcPersistenceLoadResult>(
		[this, Key]()
		{
			return LoadEntrySync(Key);
		});
}

TFuture<FArcPersistenceResult> FArcJsonFileBackend::DeleteEntry(const FString& Key)
{
	return TaskQueue.Enqueue<FArcPersistenceResult>(
		[this, Key]()
		{
			return DeleteEntrySync(Key);
		});
}

TFuture<FArcPersistenceResult> FArcJsonFileBackend::EntryExists(const FString& Key)
{
	return TaskQueue.Enqueue<FArcPersistenceResult>(
		[this, Key]()
		{
			return EntryExistsSync(Key);
		});
}

TFuture<FArcPersistenceListResult> FArcJsonFileBackend::ListEntries(const FString& KeyPrefix)
{
	return TaskQueue.Enqueue<FArcPersistenceListResult>(
		[this, KeyPrefix]()
		{
			return ListEntriesSync(KeyPrefix);
		});
}

TFuture<FArcPersistenceResult> FArcJsonFileBackend::SaveEntries(TArray<TPair<FString, TArray<uint8>>> Entries)
{
	return TaskQueue.Enqueue<FArcPersistenceResult>(
		[this, Entries = MoveTemp(Entries)]()
		{
			return SaveEntriesSync(Entries);
		});
}

void FArcJsonFileBackend::Flush()
{
	TaskQueue.Flush();
}

// ---------------------------------------------------------------------------
// Synchronous implementations (run on the task queue thread)
// ---------------------------------------------------------------------------

FArcPersistenceResult FArcJsonFileBackend::SaveEntrySync(const FString& Key, const TArray<uint8>& Data)
{
	const FString FilePath = KeyToFilePath(Key);

	// Ensure parent directory exists
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);

	if (!FFileHelper::SaveArrayToFile(Data, *FilePath))
	{
		return FArcPersistenceResult::Failure(FString::Printf(TEXT("Failed to save file: %s"), *FilePath));
	}

	return FArcPersistenceResult::Success();
}

FArcPersistenceLoadResult FArcJsonFileBackend::LoadEntrySync(const FString& Key)
{
	FArcPersistenceLoadResult Result;
	const FString FilePath = KeyToFilePath(Key);

	if (!IFileManager::Get().FileExists(*FilePath))
	{
		Result.bSuccess = false;
		Result.Error = FString::Printf(TEXT("File not found: %s"), *FilePath);
		return Result;
	}

	if (!FFileHelper::LoadFileToArray(Result.Data, *FilePath))
	{
		Result.bSuccess = false;
		Result.Error = FString::Printf(TEXT("Failed to load file: %s"), *FilePath);
		return Result;
	}

	Result.bSuccess = true;
	return Result;
}

FArcPersistenceResult FArcJsonFileBackend::DeleteEntrySync(const FString& Key)
{
	const FString FilePath = KeyToFilePath(Key);
	IFileManager::Get().Delete(*FilePath);
	return FArcPersistenceResult::Success();
}

FArcPersistenceResult FArcJsonFileBackend::EntryExistsSync(const FString& Key)
{
	const FString FilePath = KeyToFilePath(Key);

	if (IFileManager::Get().FileExists(*FilePath))
	{
		return FArcPersistenceResult::Success();
	}

	return FArcPersistenceResult::Failure(FString::Printf(TEXT("Entry does not exist: %s"), *Key));
}

FArcPersistenceListResult FArcJsonFileBackend::ListEntriesSync(const FString& KeyPrefix)
{
	FArcPersistenceListResult Result;

	FString SearchDir = BaseDirectory;
	if (!KeyPrefix.IsEmpty())
	{
		SearchDir = BaseDirectory / KeyPrefix;
	}

	TArray<FString> FoundFiles;
	IFileManager::Get().FindFilesRecursive(FoundFiles, *SearchDir, TEXT("*.json"), true, false);

	// Build normalized base with trailing slash for prefix stripping.
	// BaseDirectory is already normalized in the constructor.
	FString NormalizedBase = BaseDirectory + TEXT("/");

	for (const FString& FilePath : FoundFiles)
	{
		// Normalize to forward slashes so stripping works cross-platform
		FString NormalizedFilePath = FilePath;
		FPaths::NormalizeFilename(NormalizedFilePath);

		// Strip the base directory prefix
		if (!NormalizedFilePath.StartsWith(NormalizedBase))
		{
			continue;
		}

		FString KeyPart = NormalizedFilePath.Mid(NormalizedBase.Len());

		// Strip .json extension
		if (KeyPart.EndsWith(TEXT(".json")))
		{
			KeyPart.LeftChopInline(5);
		}

		Result.Keys.Add(MoveTemp(KeyPart));
	}

	Result.bSuccess = true;
	return Result;
}

FArcPersistenceResult FArcJsonFileBackend::SaveEntriesSync(const TArray<TPair<FString, TArray<uint8>>>& Entries)
{
	// Phase 1: Write all entries to .tmp files
	TArray<TPair<FString, FString>> TempToFinal; // TempPath -> FinalPath
	TempToFinal.Reserve(Entries.Num());

	for (const TPair<FString, TArray<uint8>>& Entry : Entries)
	{
		const FString FinalPath = KeyToFilePath(Entry.Key);
		const FString TempPath = FinalPath + TEXT(".tmp");

		// Ensure parent directory exists
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(TempPath), true);

		if (!FFileHelper::SaveArrayToFile(Entry.Value, *TempPath))
		{
			// Rollback: delete any temp files we already wrote
			for (const TPair<FString, FString>& Written : TempToFinal)
			{
				IFileManager::Get().Delete(*Written.Key);
			}

			return FArcPersistenceResult::Failure(
				FString::Printf(TEXT("Failed to write temp file for key: %s"), *Entry.Key));
		}

		TempToFinal.Emplace(TempPath, FinalPath);
	}

	// Phase 2: Move all temp files to their final locations
	for (const TPair<FString, FString>& Pending : TempToFinal)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Pending.Value), true);
		IFileManager::Get().Move(*Pending.Value, *Pending.Key);
	}

	return FArcPersistenceResult::Success();
}
