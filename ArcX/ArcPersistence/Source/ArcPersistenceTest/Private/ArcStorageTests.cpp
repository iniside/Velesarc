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

#include "CQTest.h"
#include "Storage/ArcJsonFileBackend.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

TEST_CLASS(ArcJsonFileBackend_Basic, "ArcPersistence.Storage.JsonFileBackend")
{
	FString TestDir;
	TUniquePtr<FArcJsonFileBackend> Backend;

	BEFORE_EACH()
	{
		TestDir = FPaths::ProjectSavedDir() / TEXT("ArcPersistenceTest") / FGuid::NewGuid().ToString();
		Backend = MakeUnique<FArcJsonFileBackend>(TestDir);
	}

	AFTER_EACH()
	{
		Backend.Reset();
		IFileManager::Get().DeleteDirectory(*TestDir, false, true);
	}

	TEST_METHOD(SaveAndLoad_RoundTrips)
	{
		const FString Key = TEXT("test/entry");
		TArray<uint8> Original;
		Original.Add(0x48); // H
		Original.Add(0x65); // e
		Original.Add(0x6C); // l
		Original.Add(0x6C); // l
		Original.Add(0x6F); // o

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, Original)));

		TArray<uint8> Loaded;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, Loaded)));
		ASSERT_THAT(AreEqual(Original.Num(), Loaded.Num()));

		for (int32 i = 0; i < Original.Num(); ++i)
		{
			ASSERT_THAT(AreEqual(Original[i], Loaded[i]));
		}
	}

	TEST_METHOD(EntryExists_TrueAfterSave)
	{
		const FString Key = TEXT("exists/check");
		TArray<uint8> Data;
		Data.Add(0x01);

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, Data)));
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key)));
	}

	TEST_METHOD(EntryExists_FalseBeforeSave)
	{
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("nonexistent/key"))));
	}

	TEST_METHOD(LoadNonexistent_ReturnsFalse)
	{
		TArray<uint8> OutData;
		ASSERT_THAT(IsFalse(Backend->LoadEntry(TEXT("missing/key"), OutData)));
	}

	TEST_METHOD(DeleteEntry_Removes)
	{
		const FString Key = TEXT("delete/me");
		TArray<uint8> Data;
		Data.Add(0xFF);

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, Data)));
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key)));

		Backend->DeleteEntry(Key);

		ASSERT_THAT(IsFalse(Backend->EntryExists(Key)));
	}

	TEST_METHOD(ListEntries_FindsByPrefix)
	{
		TArray<uint8> Data;
		Data.Add(0x01);

		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/abc/inventory"), Data)));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/abc/stats"), Data)));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/xyz/inventory"), Data)));

		TArray<FString> Results = Backend->ListEntries(TEXT("players/abc"));
		ASSERT_THAT(AreEqual(2, Results.Num()));

		// Verify both keys are present (order not guaranteed)
		bool bFoundInventory = Results.Contains(TEXT("players/abc/inventory"));
		bool bFoundStats = Results.Contains(TEXT("players/abc/stats"));
		ASSERT_THAT(IsTrue(bFoundInventory));
		ASSERT_THAT(IsTrue(bFoundStats));
	}

	TEST_METHOD(ListEntries_EmptyPrefix)
	{
		TArray<uint8> Data;
		Data.Add(0x01);

		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("alpha/one"), Data)));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("beta/two"), Data)));

		TArray<FString> Results = Backend->ListEntries(TEXT(""));
		ASSERT_THAT(AreEqual(2, Results.Num()));

		bool bFoundAlpha = Results.Contains(TEXT("alpha/one"));
		bool bFoundBeta = Results.Contains(TEXT("beta/two"));
		ASSERT_THAT(IsTrue(bFoundAlpha));
		ASSERT_THAT(IsTrue(bFoundBeta));
	}

	TEST_METHOD(Transaction_CommitPersists)
	{
		const FString Key = TEXT("txn/commit");
		TArray<uint8> Data;
		Data.Add(0xAA);
		Data.Add(0xBB);

		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, Data)));
		Backend->CommitTransaction();

		// After commit, entry should be loadable
		TArray<uint8> Loaded;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, Loaded)));
		ASSERT_THAT(AreEqual(2, Loaded.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0xAA), Loaded[0]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0xBB), Loaded[1]));
	}

	TEST_METHOD(Transaction_RollbackDiscards)
	{
		const FString Key = TEXT("txn/rollback");
		TArray<uint8> Data;
		Data.Add(0xCC);

		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, Data)));
		Backend->RollbackTransaction();

		// After rollback, entry should not exist
		ASSERT_THAT(IsFalse(Backend->EntryExists(Key)));

		TArray<uint8> Loaded;
		ASSERT_THAT(IsFalse(Backend->LoadEntry(Key, Loaded)));
	}

	TEST_METHOD(NestedDirectoryKeys_Work)
	{
		const FString Key = TEXT("world/abc123/actors/def456");
		TArray<uint8> Original;
		Original.Add(0xDE);
		Original.Add(0xAD);
		Original.Add(0xBE);
		Original.Add(0xEF);

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, Original)));

		TArray<uint8> Loaded;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, Loaded)));
		ASSERT_THAT(AreEqual(Original.Num(), Loaded.Num()));

		for (int32 i = 0; i < Original.Num(); ++i)
		{
			ASSERT_THAT(AreEqual(Original[i], Loaded[i]));
		}
	}
};
