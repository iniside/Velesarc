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
#include "SQLiteDatabase.h"

/**
 * SQLite-backed persistence storage.
 *
 * Uses three tables:
 *   worlds         - metadata for saved worlds
 *   world_entries  - world-scoped key/value pairs (key routed from "world/{id}/...")
 *   player_entries - player-scoped key/value pairs (key routed from "players/{id}/...")
 *
 * Data is stored as JSON TEXT. Keys are routed to tables via ParseKey().
 *
 * All public methods enqueue work onto a serial background task queue and return
 * TFuture<> immediately. The actual SQL operations run on a background thread.
 */
class ARCPERSISTENCE_API FArcSQLiteBackend : public IArcPersistenceBackend
{
public:
	explicit FArcSQLiteBackend(const FString& InDatabasePath);
	virtual ~FArcSQLiteBackend() override;

	// IArcPersistenceBackend
	virtual TFuture<FArcPersistenceResult> SaveEntry(const FString& Key, TArray<uint8> Data) override;
	virtual TFuture<FArcPersistenceLoadResult> LoadEntry(const FString& Key) override;
	virtual TFuture<FArcPersistenceResult> DeleteEntry(const FString& Key) override;
	virtual TFuture<FArcPersistenceResult> EntryExists(const FString& Key) override;
	virtual TFuture<FArcPersistenceListResult> ListEntries(const FString& KeyPrefix) override;
	virtual TFuture<FArcPersistenceResult> SaveEntries(TArray<TPair<FString, TArray<uint8>>> Entries) override;
	virtual FName GetBackendName() const override { return FName("SQLite"); }
	virtual void Flush() override;

	// Extended API — also async
	TFuture<FArcPersistenceResult> DeleteWorld(const FString& WorldId);
	TFuture<FArcPersistenceResult> DeletePlayer(const FString& PlayerId);
	TFuture<FArcPersistenceListResult> ListWorlds();
	TFuture<FArcPersistenceListResult> ListPlayers();

	/** Check if the database was opened successfully. */
	bool IsValid() const;

private:
	FSQLiteDatabase Database;
	FArcPersistenceTaskQueue TaskQueue;

	// Cached prepared statements
	FSQLitePreparedStatement StmtSaveWorldEntry, StmtLoadWorldEntry, StmtDeleteWorldEntry, StmtExistsWorldEntry, StmtListWorldEntries;
	FSQLitePreparedStatement StmtSavePlayerEntry, StmtLoadPlayerEntry, StmtDeletePlayerEntry, StmtExistsPlayerEntry, StmtListPlayerEntries;
	FSQLitePreparedStatement StmtEnsureWorld;

	bool CreateSchema();
	bool PrepareStatements();

	// Sync implementations (run on background thread via TaskQueue)
	FArcPersistenceResult SaveEntrySync(const FString& Key, const TArray<uint8>& Data);
	FArcPersistenceLoadResult LoadEntrySync(const FString& Key);
	FArcPersistenceResult DeleteEntrySync(const FString& Key);
	FArcPersistenceResult EntryExistsSync(const FString& Key);
	FArcPersistenceListResult ListEntriesSync(const FString& KeyPrefix);
	FArcPersistenceResult SaveEntriesSync(const TArray<TPair<FString, TArray<uint8>>>& Entries);
	FArcPersistenceResult DeleteWorldSync(const FString& WorldId);
	FArcPersistenceResult DeletePlayerSync(const FString& PlayerId);
	FArcPersistenceListResult ListWorldsSync();
	FArcPersistenceListResult ListPlayersSync();
};
