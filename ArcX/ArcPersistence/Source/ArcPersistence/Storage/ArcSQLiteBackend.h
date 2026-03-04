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
 */
class ARCPERSISTENCE_API FArcSQLiteBackend : public IArcPersistenceBackend
{
public:
	explicit FArcSQLiteBackend(const FString& InDatabasePath);
	virtual ~FArcSQLiteBackend() override;

	// IArcPersistenceBackend
	virtual bool SaveEntry(const FString& Key, const TArray<uint8>& Data) override;
	virtual bool LoadEntry(const FString& Key, TArray<uint8>& OutData) override;
	virtual bool DeleteEntry(const FString& Key) override;
	virtual bool EntryExists(const FString& Key) override;
	virtual TArray<FString> ListEntries(const FString& KeyPrefix) override;
	virtual void BeginTransaction() override;
	virtual void CommitTransaction() override;
	virtual void RollbackTransaction() override;
	virtual FName GetBackendName() const override { return FName("SQLite"); }

	// Extended API (SQLite-specific)

	/** Delete all entries for a world and its metadata row. */
	bool DeleteWorld(const FString& WorldId);

	/** Delete all entries for a player. */
	bool DeletePlayer(const FString& PlayerId);

	/** List all saved world IDs. */
	TArray<FString> ListWorlds();

	/** List all distinct player IDs. */
	TArray<FString> ListPlayers();

	/** Check if the database was opened successfully. */
	bool IsValid() const;

private:
	FSQLiteDatabase Database;
	bool bInTransaction = false;

	// Cached prepared statements
	FSQLitePreparedStatement StmtSaveWorldEntry;
	FSQLitePreparedStatement StmtLoadWorldEntry;
	FSQLitePreparedStatement StmtDeleteWorldEntry;
	FSQLitePreparedStatement StmtExistsWorldEntry;
	FSQLitePreparedStatement StmtListWorldEntries;

	FSQLitePreparedStatement StmtSavePlayerEntry;
	FSQLitePreparedStatement StmtLoadPlayerEntry;
	FSQLitePreparedStatement StmtDeletePlayerEntry;
	FSQLitePreparedStatement StmtExistsPlayerEntry;
	FSQLitePreparedStatement StmtListPlayerEntries;

	FSQLitePreparedStatement StmtEnsureWorld;

	bool CreateSchema();
	bool PrepareStatements();
};
