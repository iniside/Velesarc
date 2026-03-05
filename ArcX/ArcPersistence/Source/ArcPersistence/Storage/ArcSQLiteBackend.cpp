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
	TaskQueue.Flush();

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

void FArcSQLiteBackend::Flush()
{
	TaskQueue.Flush();
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
// Public async API — enqueue onto TaskQueue
// ---------------------------------------------------------------------------

TFuture<FArcPersistenceResult> FArcSQLiteBackend::SaveEntry(const FString& Key, TArray<uint8> Data)
{
	return TaskQueue.Enqueue<FArcPersistenceResult>(
		[this, Key, Data = MoveTemp(Data)]() mutable -> FArcPersistenceResult
		{
			return SaveEntrySync(Key, Data);
		});
}

TFuture<FArcPersistenceLoadResult> FArcSQLiteBackend::LoadEntry(const FString& Key)
{
	return TaskQueue.Enqueue<FArcPersistenceLoadResult>(
		[this, Key]() -> FArcPersistenceLoadResult
		{
			return LoadEntrySync(Key);
		});
}

TFuture<FArcPersistenceResult> FArcSQLiteBackend::DeleteEntry(const FString& Key)
{
	return TaskQueue.Enqueue<FArcPersistenceResult>(
		[this, Key]() -> FArcPersistenceResult
		{
			return DeleteEntrySync(Key);
		});
}

TFuture<FArcPersistenceResult> FArcSQLiteBackend::EntryExists(const FString& Key)
{
	return TaskQueue.Enqueue<FArcPersistenceResult>(
		[this, Key]() -> FArcPersistenceResult
		{
			return EntryExistsSync(Key);
		});
}

TFuture<FArcPersistenceListResult> FArcSQLiteBackend::ListEntries(const FString& KeyPrefix)
{
	return TaskQueue.Enqueue<FArcPersistenceListResult>(
		[this, KeyPrefix]() -> FArcPersistenceListResult
		{
			return ListEntriesSync(KeyPrefix);
		});
}

TFuture<FArcPersistenceResult> FArcSQLiteBackend::SaveEntries(TArray<TPair<FString, TArray<uint8>>> Entries)
{
	return TaskQueue.Enqueue<FArcPersistenceResult>(
		[this, Entries = MoveTemp(Entries)]() -> FArcPersistenceResult
		{
			return SaveEntriesSync(Entries);
		});
}

TFuture<FArcPersistenceResult> FArcSQLiteBackend::DeleteWorld(const FString& WorldId)
{
	return TaskQueue.Enqueue<FArcPersistenceResult>(
		[this, WorldId]() -> FArcPersistenceResult
		{
			return DeleteWorldSync(WorldId);
		});
}

TFuture<FArcPersistenceResult> FArcSQLiteBackend::DeletePlayer(const FString& PlayerId)
{
	return TaskQueue.Enqueue<FArcPersistenceResult>(
		[this, PlayerId]() -> FArcPersistenceResult
		{
			return DeletePlayerSync(PlayerId);
		});
}

TFuture<FArcPersistenceListResult> FArcSQLiteBackend::ListWorlds()
{
	return TaskQueue.Enqueue<FArcPersistenceListResult>(
		[this]() -> FArcPersistenceListResult
		{
			return ListWorldsSync();
		});
}

TFuture<FArcPersistenceListResult> FArcSQLiteBackend::ListPlayers()
{
	return TaskQueue.Enqueue<FArcPersistenceListResult>(
		[this]() -> FArcPersistenceListResult
		{
			return ListPlayersSync();
		});
}

// ---------------------------------------------------------------------------
// Sync implementations (run on background thread)
// ---------------------------------------------------------------------------

FArcPersistenceResult FArcSQLiteBackend::SaveEntrySync(const FString& Key, const TArray<uint8>& Data)
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

		return bOk
			? FArcPersistenceResult::Success()
			: FArcPersistenceResult::Failure(TEXT("Failed to save world entry"));
	}
	else if (Parsed.Category == EKeyCategory::Player)
	{
		StmtSavePlayerEntry.SetBindingValueByIndex(1, Key);
		StmtSavePlayerEntry.SetBindingValueByIndex(2, Parsed.OwnerId);
		StmtSavePlayerEntry.SetBindingValueByIndex(3, DataStr);
		const bool bOk = StmtSavePlayerEntry.Execute();
		StmtSavePlayerEntry.ClearBindings();
		StmtSavePlayerEntry.Reset();

		return bOk
			? FArcPersistenceResult::Success()
			: FArcPersistenceResult::Failure(TEXT("Failed to save player entry"));
	}
	else
	{
		ValidateKey(Key);
		return FArcPersistenceResult::Failure(FString::Printf(TEXT("Unknown key category: %s"), *Key));
	}
}

FArcPersistenceLoadResult FArcSQLiteBackend::LoadEntrySync(const FString& Key)
{
	using namespace UE::ArcPersistence;

	FArcPersistenceLoadResult Result;

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
		Result.bSuccess = false;
		Result.Error = FString::Printf(TEXT("Unknown key category: %s"), *Key);
		return Result;
	}

	Stmt->SetBindingValueByIndex(1, Key);

	bool bFound = false;
	Stmt->Execute([&](const FSQLitePreparedStatement& Row) -> ESQLitePreparedStatementExecuteRowResult
	{
		FString DataStr;
		if (Row.GetColumnValueByIndex(0, DataStr))
		{
			const FTCHARToUTF8 Utf8(*DataStr);
			Result.Data.SetNum(Utf8.Length());
			FMemory::Memcpy(Result.Data.GetData(), Utf8.Get(), Utf8.Length());
			bFound = true;
		}
		return ESQLitePreparedStatementExecuteRowResult::Stop;
	});

	Stmt->ClearBindings();
	Stmt->Reset();

	Result.bSuccess = bFound;
	if (!bFound)
	{
		Result.Error = FString::Printf(TEXT("Entry not found: %s"), *Key);
	}
	return Result;
}

FArcPersistenceResult FArcSQLiteBackend::DeleteEntrySync(const FString& Key)
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
		return FArcPersistenceResult::Failure(FString::Printf(TEXT("Unknown key category: %s"), *Key));
	}

	Stmt->SetBindingValueByIndex(1, Key);
	const bool bOk = Stmt->Execute();
	Stmt->ClearBindings();
	Stmt->Reset();

	return bOk
		? FArcPersistenceResult::Success()
		: FArcPersistenceResult::Failure(FString::Printf(TEXT("Failed to delete entry: %s"), *Key));
}

FArcPersistenceResult FArcSQLiteBackend::EntryExistsSync(const FString& Key)
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
		return FArcPersistenceResult::Failure(FString::Printf(TEXT("Unknown key category: %s"), *Key));
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

	return bExists
		? FArcPersistenceResult::Success()
		: FArcPersistenceResult::Failure(FString::Printf(TEXT("Entry does not exist: %s"), *Key));
}

FArcPersistenceListResult FArcSQLiteBackend::ListEntriesSync(const FString& KeyPrefix)
{
	FArcPersistenceListResult Result;

	auto CollectKeys = [&](FSQLitePreparedStatement& Stmt)
	{
		Stmt.SetBindingValueByIndex(1, KeyPrefix);
		Stmt.Execute([&](const FSQLitePreparedStatement& Row) -> ESQLitePreparedStatementExecuteRowResult
		{
			FString Key;
			if (Row.GetColumnValueByIndex(0, Key))
			{
				Result.Keys.Add(MoveTemp(Key));
			}
			return ESQLitePreparedStatementExecuteRowResult::Continue;
		});
		Stmt.ClearBindings();
		Stmt.Reset();
	};

	CollectKeys(StmtListWorldEntries);
	CollectKeys(StmtListPlayerEntries);

	Result.bSuccess = true;
	return Result;
}

FArcPersistenceResult FArcSQLiteBackend::SaveEntriesSync(const TArray<TPair<FString, TArray<uint8>>>& Entries)
{
	Database.Execute(TEXT("BEGIN TRANSACTION"));

	for (const auto& [Key, Data] : Entries)
	{
		FArcPersistenceResult EntryResult = SaveEntrySync(Key, Data);
		if (!EntryResult.bSuccess)
		{
			Database.Execute(TEXT("ROLLBACK"));
			return FArcPersistenceResult::Failure(
				FString::Printf(TEXT("Batch save failed on key '%s': %s"), *Key, *EntryResult.Error));
		}
	}

	Database.Execute(TEXT("COMMIT"));
	return FArcPersistenceResult::Success();
}

// ---------------------------------------------------------------------------
// Extended sync implementations
// ---------------------------------------------------------------------------

FArcPersistenceResult FArcSQLiteBackend::DeleteWorldSync(const FString& WorldId)
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

	return (bEntries && bWorld)
		? FArcPersistenceResult::Success()
		: FArcPersistenceResult::Failure(FString::Printf(TEXT("Failed to delete world: %s"), *WorldId));
}

FArcPersistenceResult FArcSQLiteBackend::DeletePlayerSync(const FString& PlayerId)
{
	FSQLitePreparedStatement Stmt = Database.PrepareStatement(
		TEXT("DELETE FROM player_entries WHERE player_id = ?"));
	Stmt.SetBindingValueByIndex(1, PlayerId);
	const bool bOk = Stmt.Execute();
	Stmt.Destroy();

	return bOk
		? FArcPersistenceResult::Success()
		: FArcPersistenceResult::Failure(FString::Printf(TEXT("Failed to delete player: %s"), *PlayerId));
}

FArcPersistenceListResult FArcSQLiteBackend::ListWorldsSync()
{
	FArcPersistenceListResult Result;
	Database.Execute(TEXT("SELECT world_id FROM worlds"),
		[&](const FSQLitePreparedStatement& Row) -> ESQLitePreparedStatementExecuteRowResult
	{
		FString WorldId;
		if (Row.GetColumnValueByIndex(0, WorldId))
		{
			Result.Keys.Add(MoveTemp(WorldId));
		}
		return ESQLitePreparedStatementExecuteRowResult::Continue;
	});
	Result.bSuccess = true;
	return Result;
}

FArcPersistenceListResult FArcSQLiteBackend::ListPlayersSync()
{
	FArcPersistenceListResult Result;
	Database.Execute(TEXT("SELECT DISTINCT player_id FROM player_entries"),
		[&](const FSQLitePreparedStatement& Row) -> ESQLitePreparedStatementExecuteRowResult
	{
		FString PlayerId;
		if (Row.GetColumnValueByIndex(0, PlayerId))
		{
			Result.Keys.Add(MoveTemp(PlayerId));
		}
		return ESQLitePreparedStatementExecuteRowResult::Continue;
	});
	Result.bSuccess = true;
	return Result;
}
