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
#include "Serialization/ArcReflectionSerializer.h"
#include "Serialization/ArcSerializerRegistry.h"
#include "Serialization/ArcPersistenceSerializer.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "Storage/ArcJsonFileBackend.h"
#include "ArcPersistenceTestTypes.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// =============================================================================
// Full pipeline tests — simulate subsystem data flow.
// Tests the complete save → backend → load → apply cycle.
// =============================================================================

TEST_CLASS(ArcPersistence_Pipeline, "ArcPersistence.Pipeline")
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

	TEST_METHOD(SaveAndLoad_StructThroughBackend)
	{
		const UScriptStruct* Type = FArcPersistenceTestStruct::StaticStruct();

		FArcPersistenceTestStruct Source;
		Source.Health = 250;
		Source.Name = TEXT("Pipeline");
		Source.Speed = 7.5f;
		Source.Id = FGuid::NewGuid();
		Source.Location = FVector(100.0, 200.0, 300.0);
		Source.Scores = {11, 22, 33};

		// Save: serialize → backend
		{
			FArcJsonSaveArchive SaveAr;
			uint32 SchemaVersion = FArcReflectionSerializer::ComputeSchemaVersion(Type);
			SaveAr.SetVersion(SchemaVersion);
			FArcReflectionSerializer::Save(Type, &Source, SaveAr);
			TArray<uint8> Data = SaveAr.Finalize();

			ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("test/pipeline"), Data)));
		}

		// Load: backend → deserialize
		FArcPersistenceTestStruct Target;
		{
			TArray<uint8> LoadedData;
			ASSERT_THAT(IsTrue(Backend->LoadEntry(TEXT("test/pipeline"), LoadedData)));

			FArcJsonLoadArchive LoadAr;
			ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(LoadedData)));

			uint32 ExpectedVersion = FArcReflectionSerializer::ComputeSchemaVersion(Type);
			ASSERT_THAT(AreEqual(ExpectedVersion, LoadAr.GetVersion()));

			FArcReflectionSerializer::Load(Type, &Target, LoadAr);
		}

		ASSERT_THAT(AreEqual(250, Target.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Pipeline")), Target.Name));
		ASSERT_THAT(IsNear(7.5f, Target.Speed, 0.001f));
		ASSERT_THAT(AreEqual(Source.Id, Target.Id));
		ASSERT_THAT(IsNear(100.0, Target.Location.X, 0.001));
		ASSERT_THAT(AreEqual(3, Target.Scores.Num()));
		ASSERT_THAT(AreEqual(11, Target.Scores[0]));
	}

	TEST_METHOD(VersionMismatch_DiscardsData)
	{
		const UScriptStruct* Type = FArcPersistenceTestStruct::StaticStruct();

		// Save with a bogus version
		{
			FArcJsonSaveArchive SaveAr;
			FArcPersistenceTestStruct EmptyData;
			SaveAr.SetVersion(99999);  // Not the real schema version
			FArcReflectionSerializer::Save(Type, &EmptyData, SaveAr);
			TArray<uint8> Data = SaveAr.Finalize();

			ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("test/version"), Data)));
		}

		// Load and check version mismatch
		{
			TArray<uint8> LoadedData;
			ASSERT_THAT(IsTrue(Backend->LoadEntry(TEXT("test/version"), LoadedData)));

			FArcJsonLoadArchive LoadAr;
			ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(LoadedData)));

			uint32 ExpectedVersion = FArcReflectionSerializer::ComputeSchemaVersion(Type);

			// Version should NOT match — caller discards the data
			ASSERT_THAT(AreNotEqual(ExpectedVersion, LoadAr.GetVersion()));
		}
	}

	TEST_METHOD(PlayerDataFlow_MultipleDomains)
	{
		const FGuid PlayerId = FGuid::NewGuid();
		const FString PlayerPrefix = FString::Printf(TEXT("players/%s"), *PlayerId.ToString());
		const UScriptStruct* Type = FArcPersistenceTestStruct::StaticStruct();

		// Save "inventory" domain
		{
			FArcPersistenceTestStruct Inventory;
			Inventory.Health = 10;
			Inventory.Name = TEXT("Sword");

			FArcJsonSaveArchive SaveAr;
			SaveAr.SetVersion(FArcReflectionSerializer::ComputeSchemaVersion(Type));
			FArcReflectionSerializer::Save(Type, &Inventory, SaveAr);
			TArray<uint8> Data = SaveAr.Finalize();

			Backend->SaveEntry(PlayerPrefix / TEXT("inventory"), Data);
		}

		// Save "attributes" domain
		{
			FArcPersistenceTestStruct Attributes;
			Attributes.Health = 500;
			Attributes.Name = TEXT("Warrior");

			FArcJsonSaveArchive SaveAr;
			SaveAr.SetVersion(FArcReflectionSerializer::ComputeSchemaVersion(Type));
			FArcReflectionSerializer::Save(Type, &Attributes, SaveAr);
			TArray<uint8> Data = SaveAr.Finalize();

			Backend->SaveEntry(PlayerPrefix / TEXT("attributes"), Data);
		}

		// List entries for this player
		TArray<FString> Keys = Backend->ListEntries(PlayerPrefix);
		ASSERT_THAT(AreEqual(2, Keys.Num()));

		// Load each and verify
		for (const FString& Key : Keys)
		{
			TArray<uint8> Data;
			ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, Data)));

			FArcJsonLoadArchive LoadAr;
			ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

			FArcPersistenceTestStruct Target;
			FArcReflectionSerializer::Load(Type, &Target, LoadAr);

			FString Domain;
			Key.Split(TEXT("/"), nullptr, &Domain, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

			if (Domain == TEXT("inventory"))
			{
				ASSERT_THAT(AreEqual(10, Target.Health));
				ASSERT_THAT(AreEqual(FString(TEXT("Sword")), Target.Name));
			}
			else if (Domain == TEXT("attributes"))
			{
				ASSERT_THAT(AreEqual(500, Target.Health));
				ASSERT_THAT(AreEqual(FString(TEXT("Warrior")), Target.Name));
			}
		}
	}

	TEST_METHOD(WorldActorFlow_SaveAndReload)
	{
		const FGuid WorldId = FGuid::NewGuid();
		const FGuid ActorGuid = FGuid::NewGuid();
		const FString Key = FString::Printf(TEXT("world/%s/actors/%s"),
			*WorldId.ToString(), *ActorGuid.ToString());
		const UScriptStruct* Type = FArcPersistenceTestStruct::StaticStruct();

		// Save
		{
			FArcPersistenceTestStruct ActorData;
			ActorData.Health = 75;
			ActorData.Name = TEXT("Barrel");
			ActorData.Location = FVector(1.0, 2.0, 3.0);

			FArcJsonSaveArchive SaveAr;

			SaveAr.BeginStruct(FName("_meta"));
			SaveAr.WriteProperty(FName("type"), FString(TEXT("FArcPersistenceTestStruct")));
			SaveAr.EndStruct();

			SaveAr.SetVersion(FArcReflectionSerializer::ComputeSchemaVersion(Type));
			FArcReflectionSerializer::Save(Type, &ActorData, SaveAr);
			TArray<uint8> Data = SaveAr.Finalize();

			ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, Data)));
		}

		// Load and parse metadata
		{
			TArray<uint8> Data;
			ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, Data)));

			FArcJsonLoadArchive LoadAr;
			ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

			FString TypeName;
			ASSERT_THAT(IsTrue(LoadAr.BeginStruct(FName("_meta"))));
			ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("type"), TypeName)));
			LoadAr.EndStruct();
			ASSERT_THAT(AreEqual(FString(TEXT("FArcPersistenceTestStruct")), TypeName));

			FArcPersistenceTestStruct Target;
			FArcReflectionSerializer::Load(Type, &Target, LoadAr);

			ASSERT_THAT(AreEqual(75, Target.Health));
			ASSERT_THAT(AreEqual(FString(TEXT("Barrel")), Target.Name));
			ASSERT_THAT(IsNear(1.0, Target.Location.X, 0.001));
		}
	}

	TEST_METHOD(TombstoneFlow_MarksDestruction)
	{
		const FGuid WorldId = FGuid::NewGuid();
		const FGuid ActorGuid = FGuid::NewGuid();
		const FString Key = FString::Printf(TEXT("world/%s/actors/%s"),
			*WorldId.ToString(), *ActorGuid.ToString());

		// Save tombstone marker
		{
			FArcJsonSaveArchive SaveAr;
			SaveAr.SetVersion(1);

			SaveAr.BeginStruct(FName("_meta"));
			SaveAr.WriteProperty(FName("type"), FString(TEXT("ADestroyedBarrel")));
			SaveAr.WriteProperty(FName("tombstoned"), true);
			SaveAr.EndStruct();

			TArray<uint8> Data = SaveAr.Finalize();
			ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, Data)));
		}

		// Load and detect tombstone
		{
			TArray<uint8> Data;
			ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, Data)));

			FArcJsonLoadArchive LoadAr;
			ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

			ASSERT_THAT(IsTrue(LoadAr.BeginStruct(FName("_meta"))));

			bool bTombstoned = false;
			ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("tombstoned"), bTombstoned)));
			ASSERT_THAT(IsTrue(bTombstoned));

			FString TypeName;
			ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("type"), TypeName)));
			ASSERT_THAT(AreEqual(FString(TEXT("ADestroyedBarrel")), TypeName));

			LoadAr.EndStruct();
		}
	}

	TEST_METHOD(TransactionalSave_MultipleEntries)
	{
		Backend->BeginTransaction();

		TArray<uint8> Data1 = {0x01, 0x02};
		TArray<uint8> Data2 = {0x03, 0x04};

		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/actors/a1"), Data1)));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/w1/actors/a2"), Data2)));

		Backend->CommitTransaction();

		TArray<uint8> Loaded1, Loaded2;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(TEXT("world/w1/actors/a1"), Loaded1)));
		ASSERT_THAT(IsTrue(Backend->LoadEntry(TEXT("world/w1/actors/a2"), Loaded2)));
		ASSERT_THAT(AreEqual(2, Loaded1.Num()));
		ASSERT_THAT(AreEqual(2, Loaded2.Num()));
	}
};
