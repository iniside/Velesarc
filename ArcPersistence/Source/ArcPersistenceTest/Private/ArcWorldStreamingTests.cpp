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
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "Storage/ArcJsonFileBackend.h"
#include "Storage/ArcPersistenceResult.h"
#include "Storage/ArcPersistenceKeyConvention.h"
#include "ArcPersistenceTestTypes.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// =============================================================================
// World Partition streaming data-flow tests.
// Validates stream-out (save) and stream-in (load) cycles at the backend level
// using the same patterns as the pipeline tests — no live subsystem needed.
// =============================================================================

TEST_CLASS(ArcPersistence_StreamingFlow, "ArcPersistence.Streaming")
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

	// -------------------------------------------------------------------------
	// Test 1: Full stream-out save then stream-in load restores all state.
	// Mirrors the World Partition flow: actor streams out (serialize + save),
	// then streams back in (load + parse metadata + deserialize).
	// -------------------------------------------------------------------------
	TEST_METHOD(StreamOutSave_ThenStreamInLoad_RestoresState)
	{
		const UScriptStruct* Type = FArcPersistenceTestStruct::StaticStruct();
		const FGuid WorldId = FGuid::NewGuid();
		const FGuid ActorGuid = FGuid::NewGuid();
		const FString Key = UE::ArcPersistence::MakeWorldKey(
			WorldId.ToString(), FString::Printf(TEXT("actors/%s"), *ActorGuid.ToString()));

		FArcPersistenceTestStruct Source;
		Source.Health = 350;
		Source.Name = TEXT("StreamedBarrel");
		Source.Speed = 12.5f;
		Source.Id = ActorGuid;
		Source.Location = FVector(500.0, -200.0, 75.0);
		Source.Scores = {10, 20, 30, 40};

		// --- Stream-out: serialize with _meta header and save to backend ---
		{
			FArcJsonSaveArchive SaveAr;

			SaveAr.BeginStruct(FName("_meta"));
			SaveAr.WriteProperty(FName("type"), FString(TEXT("FArcPersistenceTestStruct")));
			SaveAr.EndStruct();

			uint32 SchemaVersion = FArcReflectionSerializer::ComputeSchemaVersion(Type);
			SaveAr.SetVersion(SchemaVersion);
			FArcReflectionSerializer::Save(Type, &Source, SaveAr);
			TArray<uint8> Data = SaveAr.Finalize();

			ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, MoveTemp(Data)).Get().bSuccess));
		}

		// --- Stream-in: load from backend, parse metadata, deserialize ---
		FArcPersistenceTestStruct Target;
		{
			FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(Key).Get();
			ASSERT_THAT(IsTrue(LoadResult.bSuccess));

			FArcJsonLoadArchive LoadAr;
			ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(LoadResult.Data)));

			// Parse _meta header
			FString TypeName;
			ASSERT_THAT(IsTrue(LoadAr.BeginStruct(FName("_meta"))));
			ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("type"), TypeName)));
			LoadAr.EndStruct();
			ASSERT_THAT(AreEqual(FString(TEXT("FArcPersistenceTestStruct")), TypeName));

			// Verify schema version
			uint32 ExpectedVersion = FArcReflectionSerializer::ComputeSchemaVersion(Type);
			ASSERT_THAT(AreEqual(ExpectedVersion, LoadAr.GetVersion()));

			// Deserialize payload
			FArcReflectionSerializer::Load(Type, &Target, LoadAr);
		}

		// Verify all fields match
		ASSERT_THAT(AreEqual(350, Target.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("StreamedBarrel")), Target.Name));
		ASSERT_THAT(IsNear(12.5f, Target.Speed, 0.001f));
		ASSERT_THAT(AreEqual(Source.Id, Target.Id));
		ASSERT_THAT(IsNear(500.0, Target.Location.X, 0.001));
		ASSERT_THAT(IsNear(-200.0, Target.Location.Y, 0.001));
		ASSERT_THAT(IsNear(75.0, Target.Location.Z, 0.001));
		ASSERT_THAT(AreEqual(4, Target.Scores.Num()));
		ASSERT_THAT(AreEqual(10, Target.Scores[0]));
		ASSERT_THAT(AreEqual(20, Target.Scores[1]));
		ASSERT_THAT(AreEqual(30, Target.Scores[2]));
		ASSERT_THAT(AreEqual(40, Target.Scores[3]));
	}

	// -------------------------------------------------------------------------
	// Test 2: Stream-out save updates an existing entry (same key).
	// Simulates an actor streaming out, being modified, then streaming out again.
	// The latest state must overwrite the previous one.
	// -------------------------------------------------------------------------
	TEST_METHOD(StreamOutSave_UpdatesExistingEntry)
	{
		const UScriptStruct* Type = FArcPersistenceTestStruct::StaticStruct();
		const FGuid WorldId = FGuid::NewGuid();
		const FString Key = UE::ArcPersistence::MakeWorldKey(
			WorldId.ToString(), TEXT("actors/crate_01"));

		// --- First stream-out: initial state ---
		{
			FArcPersistenceTestStruct Initial;
			Initial.Health = 100;
			Initial.Name = TEXT("WoodenCrate");
			Initial.Speed = 0.0f;
			Initial.Location = FVector(10.0, 20.0, 0.0);

			FArcJsonSaveArchive SaveAr;

			SaveAr.BeginStruct(FName("_meta"));
			SaveAr.WriteProperty(FName("type"), FString(TEXT("FArcPersistenceTestStruct")));
			SaveAr.EndStruct();

			SaveAr.SetVersion(FArcReflectionSerializer::ComputeSchemaVersion(Type));
			FArcReflectionSerializer::Save(Type, &Initial, SaveAr);
			TArray<uint8> Data = SaveAr.Finalize();

			ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, MoveTemp(Data)).Get().bSuccess));
		}

		// --- Second stream-out: updated state to same key ---
		{
			FArcPersistenceTestStruct Updated;
			Updated.Health = 50;
			Updated.Name = TEXT("DamagedCrate");
			Updated.Speed = 3.0f;
			Updated.Location = FVector(15.0, 25.0, 1.0);

			FArcJsonSaveArchive SaveAr;

			SaveAr.BeginStruct(FName("_meta"));
			SaveAr.WriteProperty(FName("type"), FString(TEXT("FArcPersistenceTestStruct")));
			SaveAr.EndStruct();

			SaveAr.SetVersion(FArcReflectionSerializer::ComputeSchemaVersion(Type));
			FArcReflectionSerializer::Save(Type, &Updated, SaveAr);
			TArray<uint8> Data = SaveAr.Finalize();

			ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, MoveTemp(Data)).Get().bSuccess));
		}

		// --- Stream-in: load and verify we get the updated state ---
		{
			FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(Key).Get();
			ASSERT_THAT(IsTrue(LoadResult.bSuccess));

			FArcJsonLoadArchive LoadAr;
			ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(LoadResult.Data)));

			// Skip _meta
			ASSERT_THAT(IsTrue(LoadAr.BeginStruct(FName("_meta"))));
			LoadAr.EndStruct();

			FArcPersistenceTestStruct Target;
			FArcReflectionSerializer::Load(Type, &Target, LoadAr);

			ASSERT_THAT(AreEqual(50, Target.Health));
			ASSERT_THAT(AreEqual(FString(TEXT("DamagedCrate")), Target.Name));
			ASSERT_THAT(IsNear(3.0f, Target.Speed, 0.001f));
			ASSERT_THAT(IsNear(15.0, Target.Location.X, 0.001));
			ASSERT_THAT(IsNear(25.0, Target.Location.Y, 0.001));
			ASSERT_THAT(IsNear(1.0, Target.Location.Z, 0.001));
		}
	}

	// -------------------------------------------------------------------------
	// Test 3: On-demand load of a non-existent key returns failure.
	// Simulates an actor streaming in that has no persisted data.
	// -------------------------------------------------------------------------
	TEST_METHOD(OnDemandLoad_NonExistentKey_ReturnsFailure)
	{
		const FGuid WorldId = FGuid::NewGuid();
		const FString Key = UE::ArcPersistence::MakeWorldKey(
			WorldId.ToString(), TEXT("actors/does_not_exist"));

		FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(Key).Get();
		ASSERT_THAT(IsFalse(LoadResult.bSuccess));
	}
};
