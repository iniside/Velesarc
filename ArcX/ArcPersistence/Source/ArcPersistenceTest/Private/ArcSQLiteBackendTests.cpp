// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Storage/ArcSQLiteBackend.h"
#include "Storage/ArcPersistenceResult.h"
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

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, SourceData).Get().bSuccess));

		FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(Key).Get();
		ASSERT_THAT(IsTrue(LoadResult.bSuccess));
		ASSERT_THAT(AreEqual(SourceData.Num(), LoadResult.Data.Num()));

		for (int32 i = 0; i < SourceData.Num(); ++i)
		{
			ASSERT_THAT(AreEqual(SourceData[i], LoadResult.Data[i]));
		}
	}

	TEST_METHOD(SaveAndLoad_PlayerEntry_RoundTrip)
	{
		const FString Key = TEXT("players/player1/inventory");
		const TArray<uint8> SourceData = {'{', '"', 'b', '"', ':', '2', '}'};

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, SourceData).Get().bSuccess));

		FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(Key).Get();
		ASSERT_THAT(IsTrue(LoadResult.bSuccess));
		ASSERT_THAT(AreEqual(SourceData.Num(), LoadResult.Data.Num()));
	}

	TEST_METHOD(Overwrite_LatestDataWins)
	{
		const FString Key = TEXT("world/w1/overwrite");
		const TArray<uint8> DataV1 = {'v', '1'};
		const TArray<uint8> DataV2 = {'v', '2', '!'};

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, DataV1).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, DataV2).Get().bSuccess));

		FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(Key).Get();
		ASSERT_THAT(IsTrue(LoadResult.bSuccess));
		ASSERT_THAT(AreEqual(3, LoadResult.Data.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>('v'), LoadResult.Data[0]));
		ASSERT_THAT(AreEqual(static_cast<uint8>('2'), LoadResult.Data[1]));
	}

	TEST_METHOD(LoadMissingKey_ReturnsFalse)
	{
		FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(TEXT("world/w1/nonexistent")).Get();
		ASSERT_THAT(IsFalse(LoadResult.bSuccess));
	}

	TEST_METHOD(EntryExists_TrueForPresent)
	{
		const FString Key = TEXT("world/w1/exists");
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, {'1'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key).Get().bSuccess));
	}

	TEST_METHOD(EntryExists_FalseForMissing)
	{
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("world/w1/nope")).Get().bSuccess));
	}

	TEST_METHOD(DeleteEntry_RemovesFromDB)
	{
		const FString Key = TEXT("players/p1/to_delete");
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, {1, 2, 3}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key).Get().bSuccess));

		ASSERT_THAT(IsTrue(Backend->DeleteEntry(Key).Get().bSuccess));
		ASSERT_THAT(IsFalse(Backend->EntryExists(Key).Get().bSuccess));
	}

	TEST_METHOD(ListEntries_FindsWorldPrefix)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/cells/0_0"), {'1'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/cells/1_0"), {'2'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/cells/0_1"), {'3'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/inv"), {'4'}).Get().bSuccess));

		FArcPersistenceListResult ListResult = Backend->ListEntries(TEXT("world/w1/cells")).Get();
		ASSERT_THAT(IsTrue(ListResult.bSuccess));
		ASSERT_THAT(AreEqual(3, ListResult.Keys.Num()));
	}

	TEST_METHOD(ListEntries_FindsPlayerPrefix)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/inv"), {'1'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/stats"), {'2'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/a"), {'3'}).Get().bSuccess));

		FArcPersistenceListResult ListResult = Backend->ListEntries(TEXT("players/p1")).Get();
		ASSERT_THAT(IsTrue(ListResult.bSuccess));
		ASSERT_THAT(AreEqual(2, ListResult.Keys.Num()));
	}

	TEST_METHOD(ListEntries_EmptyPrefix_FindsAll)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/a"), {'1'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/b"), {'2'}).Get().bSuccess));

		FArcPersistenceListResult ListResult = Backend->ListEntries(TEXT("")).Get();
		ASSERT_THAT(IsTrue(ListResult.bSuccess));
		ASSERT_THAT(AreEqual(2, ListResult.Keys.Num()));
	}

	TEST_METHOD(InvalidKey_SaveReturnsFalse)
	{
		ASSERT_THAT(IsFalse(Backend->SaveEntry(TEXT("invalid_key"), {'1'}).Get().bSuccess));
	}
};

// =============================================================================
// Batch save (replaces transaction tests)
// =============================================================================

TEST_CLASS(ArcSQLiteBackend_BatchSave, "ArcPersistence.SQLiteBackend.BatchSave")
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

	TEST_METHOD(SaveEntries_DataPersists)
	{
		TArray<TPair<FString, TArray<uint8>>> Entries;
		Entries.Emplace(TEXT("world/w1/tx_a"), TArray<uint8>{1, 2});
		Entries.Emplace(TEXT("world/w1/tx_b"), TArray<uint8>{3, 4});

		ASSERT_THAT(IsTrue(Backend->SaveEntries(MoveTemp(Entries)).Get().bSuccess));

		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("world/w1/tx_a")).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("world/w1/tx_b")).Get().bSuccess));
	}

	TEST_METHOD(MultipleBatchSaves_Independent)
	{
		{
			TArray<TPair<FString, TArray<uint8>>> Entries;
			Entries.Emplace(TEXT("world/w1/batch1"), TArray<uint8>{1});
			ASSERT_THAT(IsTrue(Backend->SaveEntries(MoveTemp(Entries)).Get().bSuccess));
		}

		{
			TArray<TPair<FString, TArray<uint8>>> Entries;
			Entries.Emplace(TEXT("world/w1/batch2"), TArray<uint8>{2});
			ASSERT_THAT(IsTrue(Backend->SaveEntries(MoveTemp(Entries)).Get().bSuccess));
		}

		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("world/w1/batch1")).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("world/w1/batch2")).Get().bSuccess));
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
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/a"), {'1'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w2/b"), {'2'}).Get().bSuccess));

		FArcPersistenceListResult ListResult = Backend->ListWorlds().Get();
		ASSERT_THAT(IsTrue(ListResult.bSuccess));
		ASSERT_THAT(AreEqual(2, ListResult.Keys.Num()));
		ASSERT_THAT(IsTrue(ListResult.Keys.Contains(TEXT("w1"))));
		ASSERT_THAT(IsTrue(ListResult.Keys.Contains(TEXT("w2"))));
	}

	TEST_METHOD(DeleteWorld_RemovesAllWorldEntries)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/a"), {'1'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/b"), {'2'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w2/c"), {'3'}).Get().bSuccess));

		ASSERT_THAT(IsTrue(Backend->DeleteWorld(TEXT("w1")).Get().bSuccess));

		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("world/w1/a")).Get().bSuccess));
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("world/w1/b")).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("world/w2/c")).Get().bSuccess));

		FArcPersistenceListResult ListResult = Backend->ListWorlds().Get();
		ASSERT_THAT(IsTrue(ListResult.bSuccess));
		ASSERT_THAT(AreEqual(1, ListResult.Keys.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("w2")), ListResult.Keys[0]));
	}

	TEST_METHOD(ListPlayers_ReturnsDistinctPlayerIds)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/inv"), {'1'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/stats"), {'2'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p2/inv"), {'3'}).Get().bSuccess));

		FArcPersistenceListResult ListResult = Backend->ListPlayers().Get();
		ASSERT_THAT(IsTrue(ListResult.bSuccess));
		ASSERT_THAT(AreEqual(2, ListResult.Keys.Num()));
		ASSERT_THAT(IsTrue(ListResult.Keys.Contains(TEXT("p1"))));
		ASSERT_THAT(IsTrue(ListResult.Keys.Contains(TEXT("p2"))));
	}

	TEST_METHOD(DeletePlayer_RemovesAllPlayerEntries)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/inv"), {'1'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p1/stats"), {'2'}).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/p2/inv"), {'3'}).Get().bSuccess));

		ASSERT_THAT(IsTrue(Backend->DeletePlayer(TEXT("p1")).Get().bSuccess));

		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("players/p1/inv")).Get().bSuccess));
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("players/p1/stats")).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("players/p2/inv")).Get().bSuccess));
	}

	TEST_METHOD(WorldAutoCreated_OnFirstSave)
	{
		FArcPersistenceListResult EmptyResult = Backend->ListWorlds().Get();
		ASSERT_THAT(IsTrue(EmptyResult.bSuccess));
		ASSERT_THAT(AreEqual(0, EmptyResult.Keys.Num()));

		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/auto_w/a"), {'1'}).Get().bSuccess));
		FArcPersistenceListResult OneResult = Backend->ListWorlds().Get();
		ASSERT_THAT(IsTrue(OneResult.bSuccess));
		ASSERT_THAT(AreEqual(1, OneResult.Keys.Num()));

		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/auto_w/b"), {'2'}).Get().bSuccess));
		FArcPersistenceListResult StillOneResult = Backend->ListWorlds().Get();
		ASSERT_THAT(IsTrue(StillOneResult.bSuccess));
		ASSERT_THAT(AreEqual(1, StillOneResult.Keys.Num()));
	}
};
