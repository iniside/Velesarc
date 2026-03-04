// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Storage/ArcSQLiteBackend.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// =============================================================================
// CRUD operations
// =============================================================================

TEST_CLASS(ArcSQLiteBackend_CRUD, "ArcPersistence.SQLiteBackend.CRUD")
{
	FString TestDbPath;
	TUniquePtr<FArcSQLiteBackend> Backend;

	BEFORE_EACH()
	{
		TestDbPath = FPaths::ProjectSavedDir() / TEXT("ArcPersistenceSQLiteTests")
			/ FGuid::NewGuid().ToString() / TEXT("test.db");
		Backend = MakeUnique<FArcSQLiteBackend>(TestDbPath);
	}

	AFTER_EACH()
	{
		Backend.Reset();
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(TestDbPath), false, true);
	}

	TEST_METHOD(IsValid_AfterConstruction)
	{
		ASSERT_THAT(IsTrue(Backend->IsValid()));
	}

	TEST_METHOD(SaveAndLoad_WorldEntry_RoundTrip)
	{
		const FString Key = TEXT("world/testworld/actors/npc_01");
		const TArray<uint8> SourceData = {'{', '"', 'a', '"', ':', '1', '}'};

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, SourceData)));

		TArray<uint8> LoadedData;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, LoadedData)));
		ASSERT_THAT(AreEqual(SourceData.Num(), LoadedData.Num()));

		for (int32 i = 0; i < SourceData.Num(); ++i)
		{
			ASSERT_THAT(AreEqual(SourceData[i], LoadedData[i]));
		}
	}

	TEST_METHOD(SaveAndLoad_PlayerEntry_RoundTrip)
	{
		const FString Key = TEXT("players/player1/inventory");
		const TArray<uint8> SourceData = {'{', '"', 'b', '"', ':', '2', '}'};

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, SourceData)));

		TArray<uint8> LoadedData;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, LoadedData)));
		ASSERT_THAT(AreEqual(SourceData.Num(), LoadedData.Num()));
	}

	TEST_METHOD(Overwrite_LatestDataWins)
	{
		const FString Key = TEXT("world/w1/overwrite");
		const TArray<uint8> DataV1 = {'v', '1'};
		const TArray<uint8> DataV2 = {'v', '2', '!'};

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, DataV1)));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, DataV2)));

		TArray<uint8> Loaded;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, Loaded)));
		ASSERT_THAT(AreEqual(3, Loaded.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>('v'), Loaded[0]));
		ASSERT_THAT(AreEqual(static_cast<uint8>('2'), Loaded[1]));
	}

	TEST_METHOD(LoadMissingKey_ReturnsFalse)
	{
		TArray<uint8> Loaded;
		ASSERT_THAT(IsFalse(Backend->LoadEntry(TEXT("world/w1/nonexistent"), Loaded)));
	}

	TEST_METHOD(EntryExists_TrueForPresent)
	{
		const FString Key = TEXT("world/w1/exists");
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, {'1'})));
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key)));
	}

	TEST_METHOD(EntryExists_FalseForMissing)
	{
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("world/w1/nope"))));
	}

	TEST_METHOD(DeleteEntry_RemovesFromDB)
	{
		const FString Key = TEXT("players/p1/to_delete");
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, {1, 2, 3})));
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key)));

		ASSERT_THAT(IsTrue(Backend->DeleteEntry(Key)));
		ASSERT_THAT(IsFalse(Backend->EntryExists(Key)));
	}

	TEST_METHOD(ListEntries_FindsWorldPrefix)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/cells/0_0"), {'1'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/cells/1_0"), {'2'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/cells/0_1"), {'3'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/inv"), {'4'})));

		TArray<FString> CellKeys = Backend->ListEntries(TEXT("world/w1/cells"));
		ASSERT_THAT(AreEqual(3, CellKeys.Num()));
	}

	TEST_METHOD(ListEntries_FindsPlayerPrefix)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/inv"), {'1'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/stats"), {'2'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/a"), {'3'})));

		TArray<FString> PlayerKeys = Backend->ListEntries(TEXT("players/p1"));
		ASSERT_THAT(AreEqual(2, PlayerKeys.Num()));
	}

	TEST_METHOD(ListEntries_EmptyPrefix_FindsAll)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/a"), {'1'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/b"), {'2'})));

		TArray<FString> AllKeys = Backend->ListEntries(TEXT(""));
		ASSERT_THAT(AreEqual(2, AllKeys.Num()));
	}

	TEST_METHOD(InvalidKey_SaveReturnsFalse)
	{
		ASSERT_THAT(IsFalse(Backend->SaveEntry(TEXT("invalid_key"), {'1'})));
	}
};

// =============================================================================
// Transactions
// =============================================================================

TEST_CLASS(ArcSQLiteBackend_Transactions, "ArcPersistence.SQLiteBackend.Transactions")
{
	FString TestDbPath;
	TUniquePtr<FArcSQLiteBackend> Backend;

	BEFORE_EACH()
	{
		TestDbPath = FPaths::ProjectSavedDir() / TEXT("ArcPersistenceSQLiteTests")
			/ FGuid::NewGuid().ToString() / TEXT("test.db");
		Backend = MakeUnique<FArcSQLiteBackend>(TestDbPath);
	}

	AFTER_EACH()
	{
		Backend.Reset();
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(TestDbPath), false, true);
	}

	TEST_METHOD(Commit_DataPersists)
	{
		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/tx_a"), {1, 2})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/tx_b"), {3, 4})));
		Backend->CommitTransaction();

		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("world/w1/tx_a"))));
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("world/w1/tx_b"))));
	}

	TEST_METHOD(Rollback_DataDisappears)
	{
		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/roll_a"), {10})));
		Backend->RollbackTransaction();

		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("world/w1/roll_a"))));
	}

	TEST_METHOD(MultipleTransactions_Independent)
	{
		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/batch1"), {1})));
		Backend->CommitTransaction();

		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/batch2"), {2})));
		Backend->RollbackTransaction();

		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("world/w1/batch1"))));
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("world/w1/batch2"))));
	}
};

// =============================================================================
// Extended API
// =============================================================================

TEST_CLASS(ArcSQLiteBackend_ExtendedAPI, "ArcPersistence.SQLiteBackend.ExtendedAPI")
{
	FString TestDbPath;
	TUniquePtr<FArcSQLiteBackend> Backend;

	BEFORE_EACH()
	{
		TestDbPath = FPaths::ProjectSavedDir() / TEXT("ArcPersistenceSQLiteTests")
			/ FGuid::NewGuid().ToString() / TEXT("test.db");
		Backend = MakeUnique<FArcSQLiteBackend>(TestDbPath);
	}

	AFTER_EACH()
	{
		Backend.Reset();
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(TestDbPath), false, true);
	}

	TEST_METHOD(ListWorlds_ReturnsCreatedWorlds)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/a"), {'1'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w2/b"), {'2'})));

		TArray<FString> Worlds = Backend->ListWorlds();
		ASSERT_THAT(AreEqual(2, Worlds.Num()));
		ASSERT_THAT(IsTrue(Worlds.Contains(TEXT("w1"))));
		ASSERT_THAT(IsTrue(Worlds.Contains(TEXT("w2"))));
	}

	TEST_METHOD(DeleteWorld_RemovesAllWorldEntries)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/a"), {'1'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/b"), {'2'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w2/c"), {'3'})));

		ASSERT_THAT(IsTrue(Backend->DeleteWorld(TEXT("w1"))));

		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("world/w1/a"))));
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("world/w1/b"))));
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("world/w2/c"))));

		TArray<FString> Worlds = Backend->ListWorlds();
		ASSERT_THAT(AreEqual(1, Worlds.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("w2")), Worlds[0]));
	}

	TEST_METHOD(ListPlayers_ReturnsDistinctPlayerIds)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/inv"), {'1'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/stats"), {'2'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p2/inv"), {'3'})));

		TArray<FString> Players = Backend->ListPlayers();
		ASSERT_THAT(AreEqual(2, Players.Num()));
		ASSERT_THAT(IsTrue(Players.Contains(TEXT("p1"))));
		ASSERT_THAT(IsTrue(Players.Contains(TEXT("p2"))));
	}

	TEST_METHOD(DeletePlayer_RemovesAllPlayerEntries)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/inv"), {'1'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/stats"), {'2'})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p2/inv"), {'3'})));

		ASSERT_THAT(IsTrue(Backend->DeletePlayer(TEXT("p1"))));

		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("players/p1/inv"))));
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("players/p1/stats"))));
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("players/p2/inv"))));
	}

	TEST_METHOD(WorldAutoCreated_OnFirstSave)
	{
		ASSERT_THAT(AreEqual(0, Backend->ListWorlds().Num()));

		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/auto_w/a"), {'1'})));
		ASSERT_THAT(AreEqual(1, Backend->ListWorlds().Num()));

		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/auto_w/b"), {'2'})));
		ASSERT_THAT(AreEqual(1, Backend->ListWorlds().Num()));
	}
};
