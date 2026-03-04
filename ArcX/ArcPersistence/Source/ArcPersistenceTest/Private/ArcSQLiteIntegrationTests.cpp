// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcPersistenceTestTypes.h"
#include "Storage/ArcSQLiteBackend.h"
#include "Serialization/ArcReflectionSerializer.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

TEST_CLASS(ArcSQLiteBackend_StructRoundTrip, "ArcPersistence.SQLiteBackend.StructRoundTrip")
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

	template<typename T>
	bool SaveStruct(const FString& Key, const T& Source)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(T::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();
		return Backend->SaveEntry(Key, Data);
	}

	template<typename T>
	bool LoadStruct(const FString& Key, T& Target)
	{
		TArray<uint8> Data;
		if (!Backend->LoadEntry(Key, Data))
		{
			return false;
		}

		FArcJsonLoadArchive LoadAr;
		if (!LoadAr.InitializeFromData(Data))
		{
			return false;
		}

		FArcReflectionSerializer::Load(T::StaticStruct(), &Target, LoadAr);
		return true;
	}

	TEST_METHOD(WorldStruct_RoundTrip)
	{
		FArcPersistenceTestStruct Source;
		Source.Health = 250;
		Source.Name = TEXT("SQLiteWarrior");
		Source.Speed = 7.5f;
		Source.Id = FGuid::NewGuid();
		Source.Location = FVector(100.0, 200.0, 300.0);
		Source.Scores = {10, 20, 30};

		const FString Key = TEXT("world/testworld/actors/warrior");
		ASSERT_THAT(IsTrue(SaveStruct(Key, Source)));

		FArcPersistenceTestStruct Target;
		ASSERT_THAT(IsTrue(LoadStruct(Key, Target)));

		ASSERT_THAT(AreEqual(250, Target.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("SQLiteWarrior")), Target.Name));
		ASSERT_THAT(IsNear(7.5f, Target.Speed, 0.001f));
		ASSERT_THAT(AreEqual(Source.Id, Target.Id));
		ASSERT_THAT(AreEqual(3, Target.Scores.Num()));
	}

	TEST_METHOD(PlayerStruct_RoundTrip)
	{
		FArcPersistenceTestStruct Source;
		Source.Health = 100;
		Source.Name = TEXT("Player1");

		const FString Key = TEXT("players/p1/attributes");
		ASSERT_THAT(IsTrue(SaveStruct(Key, Source)));

		FArcPersistenceTestStruct Target;
		ASSERT_THAT(IsTrue(LoadStruct(Key, Target)));

		ASSERT_THAT(AreEqual(100, Target.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Player1")), Target.Name));
	}

	TEST_METHOD(MixedWorldAndPlayer_Independent)
	{
		FArcPersistenceTestStruct WorldData;
		WorldData.Health = 500;
		WorldData.Name = TEXT("WorldNPC");

		FArcPersistenceTestStruct PlayerData;
		PlayerData.Health = 100;
		PlayerData.Name = TEXT("Player");

		ASSERT_THAT(IsTrue(SaveStruct(TEXT("world/w1/npc"), WorldData)));
		ASSERT_THAT(IsTrue(SaveStruct(TEXT("players/p1/stats"), PlayerData)));

		FArcPersistenceTestStruct LoadedWorld;
		ASSERT_THAT(IsTrue(LoadStruct(TEXT("world/w1/npc"), LoadedWorld)));
		ASSERT_THAT(AreEqual(500, LoadedWorld.Health));

		FArcPersistenceTestStruct LoadedPlayer;
		ASSERT_THAT(IsTrue(LoadStruct(TEXT("players/p1/stats"), LoadedPlayer)));
		ASSERT_THAT(AreEqual(100, LoadedPlayer.Health));

		// Delete world doesn't affect player
		ASSERT_THAT(IsTrue(Backend->DeleteWorld(TEXT("w1"))));
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("world/w1/npc"))));
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("players/p1/stats"))));
	}

	TEST_METHOD(TransactionCommit_StructDataPersists)
	{
		FArcPersistenceTestStruct Source;
		Source.Health = 999;
		Source.Name = TEXT("TxTest");

		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(SaveStruct(TEXT("world/w1/tx_struct"), Source)));
		Backend->CommitTransaction();

		FArcPersistenceTestStruct Target;
		ASSERT_THAT(IsTrue(LoadStruct(TEXT("world/w1/tx_struct"), Target)));
		ASSERT_THAT(AreEqual(999, Target.Health));
	}
};
