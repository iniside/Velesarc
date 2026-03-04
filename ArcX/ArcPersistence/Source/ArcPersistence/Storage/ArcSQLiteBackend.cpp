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

#include "Storage/ArcSQLiteBackend.h"
#include "Storage/ArcPersistenceKeyConvention.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

FArcSQLiteBackend::FArcSQLiteBackend(const FString& InDatabasePath)
{
	const FString Dir = FPaths::GetPath(InDatabasePath);
	IFileManager::Get().MakeDirectory(*Dir, true);

	if (!Database.Open(*InDatabasePath, ESQLiteDatabaseOpenMode::ReadWriteCreate))
	{
		UE_LOG(LogTemp, Error, TEXT("ArcSQLiteBackend: Failed to open database at %s: %s"),
			*InDatabasePath, *Database.GetLastError());
		return;
	}

	Database.Execute(TEXT("PRAGMA journal_mode=WAL"));
	Database.Execute(TEXT("PRAGMA foreign_keys=ON"));

	if (!CreateSchema())
	{
		UE_LOG(LogTemp, Error, TEXT("ArcSQLiteBackend: Failed to create schema"));
		return;
	}

	if (!PrepareStatements())
	{
		UE_LOG(LogTemp, Error, TEXT("ArcSQLiteBackend: Failed to prepare statements"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ArcSQLiteBackend: Opened database at %s"), *InDatabasePath);
}

FArcSQLiteBackend::~FArcSQLiteBackend()
{
	StmtSaveWorldEntry.Destroy();
	StmtLoadWorldEntry.Destroy();
	StmtDeleteWorldEntry.Destroy();
	StmtExistsWorldEntry.Destroy();
	StmtListWorldEntries.Destroy();

	StmtSavePlayerEntry.Destroy();
	StmtLoadPlayerEntry.Destroy();
	StmtDeletePlayerEntry.Destroy();
	StmtExistsPlayerEntry.Destroy();
	StmtListPlayerEntries.Destroy();

	StmtEnsureWorld.Destroy();

	Database.Close();
}

bool FArcSQLiteBackend::IsValid() const
{
	return Database.IsValid();
}

bool FArcSQLiteBackend::CreateSchema()
{
	const bool bWorlds = Database.Execute(TEXT(
		"CREATE TABLE IF NOT EXISTS worlds ("
		"  world_id   TEXT PRIMARY KEY,"
		"  created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
		")"
	));

	const bool bWorldEntries = Database.Execute(TEXT(
		"CREATE TABLE IF NOT EXISTS world_entries ("
		"  key       TEXT PRIMARY KEY,"
		"  world_id  TEXT NOT NULL,"
		"  data      TEXT NOT NULL,"
		"  FOREIGN KEY (world_id) REFERENCES worlds(world_id)"
		")"
	));

	const bool bWorldIdx = Database.Execute(TEXT(
		"CREATE INDEX IF NOT EXISTS idx_world_entries_world ON world_entries(world_id)"
	));

	const bool bPlayerEntries = Database.Execute(TEXT(
		"CREATE TABLE IF NOT EXISTS player_entries ("
		"  key       TEXT PRIMARY KEY,"
		"  player_id TEXT NOT NULL,"
		"  data      TEXT NOT NULL"
		")"
	));

	const bool bPlayerIdx = Database.Execute(TEXT(
		"CREATE INDEX IF NOT EXISTS idx_player_entries_player ON player_entries(player_id)"
	));

	return bWorlds && bWorldEntries && bWorldIdx && bPlayerEntries && bPlayerIdx;
}

bool FArcSQLiteBackend::PrepareStatements()
{
	const auto Flags = ESQLitePreparedStatementFlags::Persistent;

	StmtEnsureWorld = Database.PrepareStatement(
		TEXT("INSERT OR IGNORE INTO worlds (world_id) VALUES (?)"), Flags);

	StmtSaveWorldEntry = Database.PrepareStatement(
		TEXT("INSERT OR REPLACE INTO world_entries (key, world_id, data) VALUES (?, ?, ?)"), Flags);
	StmtLoadWorldEntry = Database.PrepareStatement(
		TEXT("SELECT data FROM world_entries WHERE key = ?"), Flags);
	StmtDeleteWorldEntry = Database.PrepareStatement(
		TEXT("DELETE FROM world_entries WHERE key = ?"), Flags);
	StmtExistsWorldEntry = Database.PrepareStatement(
		TEXT("SELECT 1 FROM world_entries WHERE key = ? LIMIT 1"), Flags);
	StmtListWorldEntries = Database.PrepareStatement(
		TEXT("SELECT key FROM world_entries WHERE key LIKE ? || '%'"), Flags);

	StmtSavePlayerEntry = Database.PrepareStatement(
		TEXT("INSERT OR REPLACE INTO player_entries (key, player_id, data) VALUES (?, ?, ?)"), Flags);
	StmtLoadPlayerEntry = Database.PrepareStatement(
		TEXT("SELECT data FROM player_entries WHERE key = ?"), Flags);
	StmtDeletePlayerEntry = Database.PrepareStatement(
		TEXT("DELETE FROM player_entries WHERE key = ?"), Flags);
	StmtExistsPlayerEntry = Database.PrepareStatement(
		TEXT("SELECT 1 FROM player_entries WHERE key = ? LIMIT 1"), Flags);
	StmtListPlayerEntries = Database.PrepareStatement(
		TEXT("SELECT key FROM player_entries WHERE key LIKE ? || '%'"), Flags);

	return StmtEnsureWorld.IsValid()
		&& StmtSaveWorldEntry.IsValid() && StmtLoadWorldEntry.IsValid()
		&& StmtDeleteWorldEntry.IsValid() && StmtExistsWorldEntry.IsValid()
		&& StmtListWorldEntries.IsValid()
		&& StmtSavePlayerEntry.IsValid() && StmtLoadPlayerEntry.IsValid()
		&& StmtDeletePlayerEntry.IsValid() && StmtExistsPlayerEntry.IsValid()
		&& StmtListPlayerEntries.IsValid();
}

// ---------------------------------------------------------------------------
// Core CRUD
// ---------------------------------------------------------------------------

bool FArcSQLiteBackend::SaveEntry(const FString& Key, const TArray<uint8>& Data)
{
	using namespace UE::ArcPersistence;

	FParsedKey Parsed = ParseKey(Key);

	// Convert binary data to FString (it's UTF-8 JSON)
	const FString DataStr = FString(Data.Num(), reinterpret_cast<const ANSICHAR*>(Data.GetData()));

	if (Parsed.Category == EKeyCategory::World)
	{
		StmtEnsureWorld.SetBindingValueByIndex(1, Parsed.OwnerId);
		StmtEnsureWorld.Execute();
		StmtEnsureWorld.ClearBindings();
		StmtEnsureWorld.Reset();

		StmtSaveWorldEntry.SetBindingValueByIndex(1, Key);
		StmtSaveWorldEntry.SetBindingValueByIndex(2, Parsed.OwnerId);
		StmtSaveWorldEntry.SetBindingValueByIndex(3, DataStr);
		const bool bOk = StmtSaveWorldEntry.Execute();
		StmtSaveWorldEntry.ClearBindings();
		StmtSaveWorldEntry.Reset();
		return bOk;
	}
	else if (Parsed.Category == EKeyCategory::Player)
	{
		StmtSavePlayerEntry.SetBindingValueByIndex(1, Key);
		StmtSavePlayerEntry.SetBindingValueByIndex(2, Parsed.OwnerId);
		StmtSavePlayerEntry.SetBindingValueByIndex(3, DataStr);
		const bool bOk = StmtSavePlayerEntry.Execute();
		StmtSavePlayerEntry.ClearBindings();
		StmtSavePlayerEntry.Reset();
		return bOk;
	}
	else
	{
		ValidateKey(Key);
		return false;
	}
}

bool FArcSQLiteBackend::LoadEntry(const FString& Key, TArray<uint8>& OutData)
{
	using namespace UE::ArcPersistence;

	FParsedKey Parsed = ParseKey(Key);
	FSQLitePreparedStatement* Stmt = nullptr;

	if (Parsed.Category == EKeyCategory::World)
	{
		Stmt = &StmtLoadWorldEntry;
	}
	else if (Parsed.Category == EKeyCategory::Player)
	{
		Stmt = &StmtLoadPlayerEntry;
	}
	else
	{
		ValidateKey(Key);
		return false;
	}

	Stmt->SetBindingValueByIndex(1, Key);

	bool bFound = false;
	Stmt->Execute([&](const FSQLitePreparedStatement& Row) -> ESQLitePreparedStatementExecuteRowResult
	{
		FString DataStr;
		if (Row.GetColumnValueByIndex(0, DataStr))
		{
			const FTCHARToUTF8 Utf8(*DataStr);
			OutData.SetNum(Utf8.Length());
			FMemory::Memcpy(OutData.GetData(), Utf8.Get(), Utf8.Length());
			bFound = true;
		}
		return ESQLitePreparedStatementExecuteRowResult::Stop;
	});

	Stmt->ClearBindings();
	Stmt->Reset();
	return bFound;
}

bool FArcSQLiteBackend::DeleteEntry(const FString& Key)
{
	using namespace UE::ArcPersistence;

	FParsedKey Parsed = ParseKey(Key);
	FSQLitePreparedStatement* Stmt = nullptr;

	if (Parsed.Category == EKeyCategory::World)
	{
		Stmt = &StmtDeleteWorldEntry;
	}
	else if (Parsed.Category == EKeyCategory::Player)
	{
		Stmt = &StmtDeletePlayerEntry;
	}
	else
	{
		ValidateKey(Key);
		return false;
	}

	Stmt->SetBindingValueByIndex(1, Key);
	const bool bOk = Stmt->Execute();
	Stmt->ClearBindings();
	Stmt->Reset();
	return bOk;
}

bool FArcSQLiteBackend::EntryExists(const FString& Key)
{
	using namespace UE::ArcPersistence;

	FParsedKey Parsed = ParseKey(Key);
	FSQLitePreparedStatement* Stmt = nullptr;

	if (Parsed.Category == EKeyCategory::World)
	{
		Stmt = &StmtExistsWorldEntry;
	}
	else if (Parsed.Category == EKeyCategory::Player)
	{
		Stmt = &StmtExistsPlayerEntry;
	}
	else
	{
		return false;
	}

	Stmt->SetBindingValueByIndex(1, Key);

	bool bExists = false;
	Stmt->Execute([&](const FSQLitePreparedStatement&) -> ESQLitePreparedStatementExecuteRowResult
	{
		bExists = true;
		return ESQLitePreparedStatementExecuteRowResult::Stop;
	});

	Stmt->ClearBindings();
	Stmt->Reset();
	return bExists;
}

TArray<FString> FArcSQLiteBackend::ListEntries(const FString& KeyPrefix)
{
	TArray<FString> Keys;

	auto CollectKeys = [&](FSQLitePreparedStatement& Stmt)
	{
		Stmt.SetBindingValueByIndex(1, KeyPrefix);
		Stmt.Execute([&](const FSQLitePreparedStatement& Row) -> ESQLitePreparedStatementExecuteRowResult
		{
			FString Key;
			if (Row.GetColumnValueByIndex(0, Key))
			{
				Keys.Add(MoveTemp(Key));
			}
			return ESQLitePreparedStatementExecuteRowResult::Continue;
		});
		Stmt.ClearBindings();
		Stmt.Reset();
	};

	CollectKeys(StmtListWorldEntries);
	CollectKeys(StmtListPlayerEntries);

	return Keys;
}

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

void FArcSQLiteBackend::BeginTransaction()
{
	bInTransaction = true;
	Database.Execute(TEXT("BEGIN TRANSACTION"));
}

void FArcSQLiteBackend::CommitTransaction()
{
	Database.Execute(TEXT("COMMIT"));
	bInTransaction = false;
}

void FArcSQLiteBackend::RollbackTransaction()
{
	Database.Execute(TEXT("ROLLBACK"));
	bInTransaction = false;
}

// ---------------------------------------------------------------------------
// Extended API
// ---------------------------------------------------------------------------

bool FArcSQLiteBackend::DeleteWorld(const FString& WorldId)
{
	FSQLitePreparedStatement StmtDelEntries = Database.PrepareStatement(
		TEXT("DELETE FROM world_entries WHERE world_id = ?"));
	StmtDelEntries.SetBindingValueByIndex(1, WorldId);
	const bool bEntries = StmtDelEntries.Execute();
	StmtDelEntries.Destroy();

	FSQLitePreparedStatement StmtDelWorld = Database.PrepareStatement(
		TEXT("DELETE FROM worlds WHERE world_id = ?"));
	StmtDelWorld.SetBindingValueByIndex(1, WorldId);
	const bool bWorld = StmtDelWorld.Execute();
	StmtDelWorld.Destroy();

	return bEntries && bWorld;
}

bool FArcSQLiteBackend::DeletePlayer(const FString& PlayerId)
{
	FSQLitePreparedStatement Stmt = Database.PrepareStatement(
		TEXT("DELETE FROM player_entries WHERE player_id = ?"));
	Stmt.SetBindingValueByIndex(1, PlayerId);
	const bool bOk = Stmt.Execute();
	Stmt.Destroy();
	return bOk;
}

TArray<FString> FArcSQLiteBackend::ListWorlds()
{
	TArray<FString> Worlds;
	Database.Execute(TEXT("SELECT world_id FROM worlds"),
		[&](const FSQLitePreparedStatement& Row) -> ESQLitePreparedStatementExecuteRowResult
	{
		FString WorldId;
		if (Row.GetColumnValueByIndex(0, WorldId))
		{
			Worlds.Add(MoveTemp(WorldId));
		}
		return ESQLitePreparedStatementExecuteRowResult::Continue;
	});
	return Worlds;
}

TArray<FString> FArcSQLiteBackend::ListPlayers()
{
	TArray<FString> Players;
	Database.Execute(TEXT("SELECT DISTINCT player_id FROM player_entries"),
		[&](const FSQLitePreparedStatement& Row) -> ESQLitePreparedStatementExecuteRowResult
	{
		FString PlayerId;
		if (Row.GetColumnValueByIndex(0, PlayerId))
		{
			Players.Add(MoveTemp(PlayerId));
		}
		return ESQLitePreparedStatementExecuteRowResult::Continue;
	});
	return Players;
}
