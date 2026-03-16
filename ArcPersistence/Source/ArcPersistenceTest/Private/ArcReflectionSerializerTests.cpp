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
#include "ArcPersistenceTestTypes.h"
#include "ArcJsonIncludes.h"

// =============================================================================
// TEST_CLASS 1: Basic reflection serializer tests
// =============================================================================

TEST_CLASS(ArcReflectionSerializer_Basic, "ArcPersistence.Serialization.Reflection.Basic")
{
	TEST_METHOD(ComputeSchemaVersion_IsDeterministic)
	{
		const uint32 Hash1 = FArcReflectionSerializer::ComputeSchemaVersion(FArcPersistenceTestStruct::StaticStruct());
		const uint32 Hash2 = FArcReflectionSerializer::ComputeSchemaVersion(FArcPersistenceTestStruct::StaticStruct());
		ASSERT_THAT(AreEqual(Hash1, Hash2));
	}

	TEST_METHOD(ComputeSchemaVersion_DiffersPerType)
	{
		const uint32 HashA = FArcReflectionSerializer::ComputeSchemaVersion(FArcPersistenceTestStruct::StaticStruct());
		const uint32 HashB = FArcReflectionSerializer::ComputeSchemaVersion(FArcPersistenceTestNestedStruct::StaticStruct());
		ASSERT_THAT(AreNotEqual(HashA, HashB));
	}

	TEST_METHOD(RoundTrips_SimpleStruct)
	{
		// Set up source data
		FArcPersistenceTestStruct Source;
		Source.Health = 200;
		Source.Name = TEXT("Hero");
		Source.Speed = 5.5f;
		Source.Id = FGuid::NewGuid();
		Source.Location = FVector(1.0, 2.0, 3.0);
		Source.Scores = {10, 20, 30};
		Source.NotSaved = 99;

		// Save
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(FArcPersistenceTestStruct::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		// Load
		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		FArcPersistenceTestStruct Target; // Fresh instance with defaults
		FArcReflectionSerializer::Load(FArcPersistenceTestStruct::StaticStruct(), &Target, LoadAr);

		// Verify SaveGame properties round-tripped
		ASSERT_THAT(AreEqual(200, Target.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Hero")), Target.Name));
		ASSERT_THAT(IsNear(5.5f, Target.Speed, 0.001f));
		ASSERT_THAT(AreEqual(Source.Id, Target.Id));
		ASSERT_THAT(IsNear(1.0, Target.Location.X, 0.001));
		ASSERT_THAT(IsNear(2.0, Target.Location.Y, 0.001));
		ASSERT_THAT(IsNear(3.0, Target.Location.Z, 0.001));
		ASSERT_THAT(AreEqual(3, Target.Scores.Num()));
		ASSERT_THAT(AreEqual(10, Target.Scores[0]));
		ASSERT_THAT(AreEqual(20, Target.Scores[1]));
		ASSERT_THAT(AreEqual(30, Target.Scores[2]));

		// NotSaved should be default (42), not the source's 99
		ASSERT_THAT(AreEqual(42, Target.NotSaved));
	}

	TEST_METHOD(MissingProperties_KeepDefaults)
	{
		// Save an empty (default) struct
		FArcPersistenceTestStruct Source;

		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(FArcPersistenceTestStruct::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		// Load into a struct that has been modified from defaults
		FArcPersistenceTestStruct Target;
		Target.Health = 999;
		Target.Name = TEXT("Modified");
		Target.Speed = 42.0f;

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));
		FArcReflectionSerializer::Load(FArcPersistenceTestStruct::StaticStruct(), &Target, LoadAr);

		// The loaded values should match what was saved (the defaults), not what was pre-set
		ASSERT_THAT(AreEqual(100, Target.Health));
		ASSERT_THAT(AreEqual(FString(), Target.Name));
		ASSERT_THAT(IsNear(0.0f, Target.Speed, 0.001f));
	}

	TEST_METHOD(ExtraProperties_Ignored)
	{
		// Save a struct normally
		FArcPersistenceTestStruct Source;
		Source.Health = 150;

		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(FArcPersistenceTestStruct::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		// Parse the JSON, inject an extra key into the data section, re-serialize
		const std::string JsonStr(reinterpret_cast<const char*>(Data.GetData()), Data.Num());
		nlohmann::json Envelope = nlohmann::json::parse(JsonStr);
		Envelope["data"]["BogusExtraField"] = 12345;
		Envelope["data"]["AnotherFakeProperty"] = "should be ignored";

		const std::string ModifiedStr = Envelope.dump();
		TArray<uint8> ModifiedData;
		ModifiedData.Append(reinterpret_cast<const uint8*>(ModifiedStr.data()), static_cast<int32>(ModifiedStr.size()));

		// Load — should not crash and extra keys should be silently ignored
		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(ModifiedData)));

		FArcPersistenceTestStruct Target;
		FArcReflectionSerializer::Load(FArcPersistenceTestStruct::StaticStruct(), &Target, LoadAr);

		// Verify the real data loaded correctly
		ASSERT_THAT(AreEqual(150, Target.Health));
	}
};

// =============================================================================
// TEST_CLASS 2: Nested and array struct reflection tests
// =============================================================================

TEST_CLASS(ArcReflectionSerializer_Nested, "ArcPersistence.Serialization.Reflection.Nested")
{
	TEST_METHOD(RoundTrips_NestedStruct)
	{
		FArcPersistenceTestNestedStruct Source;
		Source.Inner.Health = 50;
		Source.Inner.Name = TEXT("Sub");
		Source.Inner.Speed = 3.0f;
		Source.Inner.Id = FGuid::NewGuid();
		Source.Inner.Location = FVector(10.0, 20.0, 30.0);
		Source.Inner.Scores = {5, 15};
		Source.Multiplier = 2.5f;

		// Save
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(FArcPersistenceTestNestedStruct::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		// Load
		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		FArcPersistenceTestNestedStruct Target;
		FArcReflectionSerializer::Load(FArcPersistenceTestNestedStruct::StaticStruct(), &Target, LoadAr);

		// Verify nested fields
		ASSERT_THAT(AreEqual(50, Target.Inner.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Sub")), Target.Inner.Name));
		ASSERT_THAT(IsNear(3.0f, Target.Inner.Speed, 0.001f));
		ASSERT_THAT(AreEqual(Source.Inner.Id, Target.Inner.Id));
		ASSERT_THAT(IsNear(10.0, Target.Inner.Location.X, 0.001));
		ASSERT_THAT(IsNear(20.0, Target.Inner.Location.Y, 0.001));
		ASSERT_THAT(IsNear(30.0, Target.Inner.Location.Z, 0.001));
		ASSERT_THAT(AreEqual(2, Target.Inner.Scores.Num()));
		ASSERT_THAT(AreEqual(5, Target.Inner.Scores[0]));
		ASSERT_THAT(AreEqual(15, Target.Inner.Scores[1]));

		// Verify outer field
		ASSERT_THAT(IsNear(2.5f, Target.Multiplier, 0.001f));
	}

	TEST_METHOD(RoundTrips_ArrayOfStructs)
	{
		FArcPersistenceTestArrayStruct Source;
		Source.Count = 2;

		FArcPersistenceTestStruct Item0;
		Item0.Health = 80;
		Item0.Name = TEXT("Warrior");
		Item0.Speed = 4.0f;
		Item0.Scores = {100};

		FArcPersistenceTestStruct Item1;
		Item1.Health = 60;
		Item1.Name = TEXT("Mage");
		Item1.Speed = 2.0f;
		Item1.Scores = {200, 300};

		Source.Items.Add(Item0);
		Source.Items.Add(Item1);

		// Save
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(FArcPersistenceTestArrayStruct::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		// Load
		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		FArcPersistenceTestArrayStruct Target;
		FArcReflectionSerializer::Load(FArcPersistenceTestArrayStruct::StaticStruct(), &Target, LoadAr);

		// Verify count and array length
		ASSERT_THAT(AreEqual(2, Target.Count));
		ASSERT_THAT(AreEqual(2, Target.Items.Num()));

		// Verify Item 0
		ASSERT_THAT(AreEqual(80, Target.Items[0].Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Warrior")), Target.Items[0].Name));
		ASSERT_THAT(IsNear(4.0f, Target.Items[0].Speed, 0.001f));
		ASSERT_THAT(AreEqual(1, Target.Items[0].Scores.Num()));
		ASSERT_THAT(AreEqual(100, Target.Items[0].Scores[0]));

		// Verify Item 1
		ASSERT_THAT(AreEqual(60, Target.Items[1].Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Mage")), Target.Items[1].Name));
		ASSERT_THAT(IsNear(2.0f, Target.Items[1].Speed, 0.001f));
		ASSERT_THAT(AreEqual(2, Target.Items[1].Scores.Num()));
		ASSERT_THAT(AreEqual(200, Target.Items[1].Scores[0]));
		ASSERT_THAT(AreEqual(300, Target.Items[1].Scores[1]));
	}
};

// =============================================================================
// TEST_CLASS 3: Serializer registry tests
// =============================================================================

TEST_CLASS(ArcSerializerRegistry_Basic, "ArcPersistence.Serialization.Registry")
{
	TEST_METHOD(FindReturnsNull_ForUnregisteredType)
	{
		const FArcPersistenceSerializerInfo* Found = FArcSerializerRegistry::Get().Find(TBaseStructure<FVector>::Get());
		ASSERT_THAT(IsNull(Found));
	}

	TEST_METHOD(RegisterAndFind_Works)
	{
		const FName TestName(TEXT("TestRegistryType_RegisterAndFind"));

		FArcPersistenceSerializerInfo Info;
		Info.StructName = TestName;
		Info.Version = 7;
		Info.SaveFunc = nullptr;
		Info.LoadFunc = nullptr;

		FArcSerializerRegistry::Get().Register(TestName, Info);

		const FArcPersistenceSerializerInfo* Found = FArcSerializerRegistry::Get().FindByName(TestName);
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(AreEqual(7u, Found->Version));
		ASSERT_THAT(AreEqual(TestName, Found->StructName));

		// Clean up
		FArcSerializerRegistry::Get().Unregister(TestName);
	}

	TEST_METHOD(UnregisterRemoves)
	{
		const FName TestName(TEXT("TestRegistryType_UnregisterRemoves"));

		FArcPersistenceSerializerInfo Info;
		Info.StructName = TestName;
		Info.Version = 3;
		Info.SaveFunc = nullptr;
		Info.LoadFunc = nullptr;

		// Register and verify it exists
		FArcSerializerRegistry::Get().Register(TestName, Info);
		const FArcPersistenceSerializerInfo* Found = FArcSerializerRegistry::Get().FindByName(TestName);
		ASSERT_THAT(IsNotNull(Found));

		// Unregister and verify it's gone
		FArcSerializerRegistry::Get().Unregister(TestName);
		const FArcPersistenceSerializerInfo* FoundAfter = FArcSerializerRegistry::Get().FindByName(TestName);
		ASSERT_THAT(IsNull(FoundAfter));
	}
};
