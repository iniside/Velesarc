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

/**
 * Filesystem-backed persistence storage.
 * Maps logical keys to JSON files under a base directory.
 * Example: Key "players/abc/inventory" -> BaseDirectory/players/abc/inventory.json
 */
class ARCPERSISTENCE_API FArcJsonFileBackend : public IArcPersistenceBackend
{
public:
	explicit FArcJsonFileBackend(const FString& InBaseDirectory);

	// IArcPersistenceBackend
	virtual bool SaveEntry(const FString& Key, const TArray<uint8>& Data) override;
	virtual bool LoadEntry(const FString& Key, TArray<uint8>& OutData) override;
	virtual bool DeleteEntry(const FString& Key) override;
	virtual bool EntryExists(const FString& Key) override;
	virtual TArray<FString> ListEntries(const FString& KeyPrefix) override;
	virtual void BeginTransaction() override;
	virtual void CommitTransaction() override;
	virtual void RollbackTransaction() override;
	virtual FName GetBackendName() const override { return FName("JsonFile"); }

private:
	FString BaseDirectory;
	bool bInTransaction = false;
	TArray<TPair<FString, FString>> PendingWrites; // TempPath -> FinalPath

	FString KeyToFilePath(const FString& Key) const;
	FString KeyToTempPath(const FString& Key) const;
};
