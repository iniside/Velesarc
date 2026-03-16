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

#pragma once

#include "Storage/ArcPersistenceBackend.h"
#include "Storage/ArcPersistenceTaskQueue.h"

/**
 * Filesystem-backed persistence storage.
 * Maps logical keys to JSON files under a base directory.
 * Example: Key "players/abc/inventory" -> BaseDirectory/players/abc/inventory.json
 *
 * All public methods enqueue work onto a serial task queue so file I/O
 * happens on a background thread without contention.
 */
class ARCPERSISTENCE_API FArcJsonFileBackend : public IArcPersistenceBackend
{
public:
	explicit FArcJsonFileBackend(const FString& InBaseDirectory);

	virtual TFuture<FArcPersistenceResult> SaveEntry(const FString& Key, TArray<uint8> Data) override;
	virtual TFuture<FArcPersistenceLoadResult> LoadEntry(const FString& Key) override;
	virtual TFuture<FArcPersistenceResult> DeleteEntry(const FString& Key) override;
	virtual TFuture<FArcPersistenceResult> EntryExists(const FString& Key) override;
	virtual TFuture<FArcPersistenceListResult> ListEntries(const FString& KeyPrefix) override;
	virtual TFuture<FArcPersistenceResult> SaveEntries(TArray<TPair<FString, TArray<uint8>>> Entries) override;
	virtual FName GetBackendName() const override { return FName("JsonFile"); }
	virtual void Flush() override;

private:
	FString BaseDirectory;
	FArcPersistenceTaskQueue TaskQueue;

	FString KeyToFilePath(const FString& Key) const;

	FArcPersistenceResult SaveEntrySync(const FString& Key, const TArray<uint8>& Data);
	FArcPersistenceLoadResult LoadEntrySync(const FString& Key);
	FArcPersistenceResult DeleteEntrySync(const FString& Key);
	FArcPersistenceResult EntryExistsSync(const FString& Key);
	FArcPersistenceListResult ListEntriesSync(const FString& KeyPrefix);
	FArcPersistenceResult SaveEntriesSync(const TArray<TPair<FString, TArray<uint8>>>& Entries);
};
