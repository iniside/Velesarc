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

FString FArcJsonFileBackend::KeyToTempPath(const FString& Key) const
{
	return BaseDirectory / Key + TEXT(".tmp");
}

// ---------------------------------------------------------------------------
// Core CRUD
// ---------------------------------------------------------------------------

bool FArcJsonFileBackend::SaveEntry(const FString& Key, const TArray<uint8>& Data)
{
	if (bInTransaction)
	{
		const FString TempPath = KeyToTempPath(Key);
		const FString FinalPath = KeyToFilePath(Key);

		// Ensure parent directory exists for the temp file
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(TempPath), true);

		if (!FFileHelper::SaveArrayToFile(Data, *TempPath))
		{
			return false;
		}

		PendingWrites.Emplace(TempPath, FinalPath);
		return true;
	}

	const FString FilePath = KeyToFilePath(Key);

	// Ensure parent directory exists
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);

	return FFileHelper::SaveArrayToFile(Data, *FilePath);
}

bool FArcJsonFileBackend::LoadEntry(const FString& Key, TArray<uint8>& OutData)
{
	const FString FilePath = KeyToFilePath(Key);

	if (!IFileManager::Get().FileExists(*FilePath))
	{
		return false;
	}

	return FFileHelper::LoadFileToArray(OutData, *FilePath);
}

bool FArcJsonFileBackend::DeleteEntry(const FString& Key)
{
	const FString FilePath = KeyToFilePath(Key);
	IFileManager::Get().Delete(*FilePath);
	return true;
}

bool FArcJsonFileBackend::EntryExists(const FString& Key)
{
	const FString FilePath = KeyToFilePath(Key);
	return IFileManager::Get().FileExists(*FilePath);
}

// ---------------------------------------------------------------------------
// Prefix scan
// ---------------------------------------------------------------------------

TArray<FString> FArcJsonFileBackend::ListEntries(const FString& KeyPrefix)
{
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

	TArray<FString> Keys;
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

		Keys.Add(MoveTemp(KeyPart));
	}

	return Keys;
}

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

void FArcJsonFileBackend::BeginTransaction()
{
	bInTransaction = true;
	PendingWrites.Empty();
}

void FArcJsonFileBackend::CommitTransaction()
{
	for (const TPair<FString, FString>& Pending : PendingWrites)
	{
		const FString& TempPath = Pending.Key;
		const FString& FinalPath = Pending.Value;

		// Ensure parent directory exists for the final path
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FinalPath), true);

		// Move temp file to final location
		IFileManager::Get().Move(*FinalPath, *TempPath);
	}

	PendingWrites.Empty();
	bInTransaction = false;
}

void FArcJsonFileBackend::RollbackTransaction()
{
	for (const TPair<FString, FString>& Pending : PendingWrites)
	{
		const FString& TempPath = Pending.Key;

		// Clean up temp files
		IFileManager::Get().Delete(*TempPath);
	}

	PendingWrites.Empty();
	bInTransaction = false;
}
