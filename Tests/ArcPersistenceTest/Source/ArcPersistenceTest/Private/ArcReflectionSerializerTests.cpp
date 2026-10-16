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
#include "ArcYyJson.h"
#include "NativeGameplayTags.h"

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
		ArcJson::FDocPtr ParsedDoc = ArcJson::Parse(Data);
		ASSERT_THAT(IsNotNull(ParsedDoc.Get()));
		ArcJson::FMutDocPtr MutDoc(yyjson_doc_mut_copy(ParsedDoc.Get(), nullptr));
		ASSERT_THAT(IsNotNull(MutDoc.Get()));
		yyjson_mut_val* MutRoot = yyjson_mut_doc_get_root(MutDoc.Get());
		yyjson_mut_val* MutData = yyjson_mut_obj_get(MutRoot, "data");
		ASSERT_THAT(IsNotNull(MutData));
		yyjson_mut_obj_add_int(MutDoc.Get(), MutData, "BogusExtraField", 12345);
		yyjson_mut_obj_add_str(MutDoc.Get(), MutData, "AnotherFakeProperty", "should be ignored");

		TArray<uint8> ModifiedData = ArcJson::WritePretty(MutDoc.Get());

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

// ─────────────────────────────────────────────────────────────────────────────
// Gameplay tags for delta/cache tests (file-static; one TU only)
// ─────────────────────────────────────────────────────────────────────────────
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcDeltaTest_Active, "ArcDeltaTest.Active");

// =============================================================================
// Helper: round-trip FArcPersistenceTestDeltaStruct via Save → Load
// =============================================================================

namespace ArcReflectionSerializerTestsPrivate
{
	static void RoundTripDelta(const FArcPersistenceTestDeltaStruct& Source,
	                            FArcPersistenceTestDeltaStruct& Target)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(
			FArcPersistenceTestDeltaStruct::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		check(LoadAr.InitializeFromData(Data));
		FArcReflectionSerializer::Load(
			FArcPersistenceTestDeltaStruct::StaticStruct(), &Target, LoadAr);
	}
} // namespace ArcReflectionSerializerTestsPrivate

// =============================================================================
// TEST_CLASS 4: Cached-plan enhancement (#1) — Step 8
// =============================================================================

TEST_CLASS(ArcReflectionSerializer_CachedPlan, "ArcPersistence.Serialization.Reflection.CachedPlan")
{
	TEST_METHOD(RoundTrip_FullDeltaStruct)
	{
		// Populate source with fully non-default values across all branch kinds.
		FArcPersistenceTestDeltaStruct Source;
		Source.IntVal     = 42;
		Source.FloatVal   = 3.14f;
		Source.EnumVal    = EArcPersistenceDeltaTestEnum::Active;
		Source.GuidVal    = FGuid(1, 2, 3, 4);
		Source.TagVal     = TAG_ArcDeltaTest_Active;
		Source.VecVal     = FVector(10.0, 20.0, 30.0);
		Source.AssetIdVal = FPrimaryAssetId(FPrimaryAssetType("Weapon"), FName("Sword01"));
		Source.Nested.Health = 77;
		Source.Nested.Name   = TEXT("Nested");
		Source.Nested.Speed  = 2.5f;
		Source.IntArray      = {1, 2, 3};
		Source.NameFloatMap.Add(FName("A"), 1.0f);
		Source.NameFloatMap.Add(FName("B"), 2.0f);

		FArcPersistenceTestStruct StructEntry;
		StructEntry.Health = 5;
		StructEntry.Name   = TEXT("MapEntry");
		Source.NameStructMap.Add(FName("X"), StructEntry);

		FArcPersistenceTestDeltaStruct Target;
		ArcReflectionSerializerTestsPrivate::RoundTripDelta(Source, Target);

		ASSERT_THAT(AreEqual(42, Target.IntVal));
		ASSERT_THAT(IsNear(3.14f, Target.FloatVal, 0.001f));
		ASSERT_THAT(AreEqual(static_cast<uint8>(EArcPersistenceDeltaTestEnum::Active),
		                      static_cast<uint8>(Target.EnumVal)));
		ASSERT_THAT(AreEqual(Source.GuidVal, Target.GuidVal));
		ASSERT_THAT(IsTrue(Target.TagVal.MatchesTagExact(TAG_ArcDeltaTest_Active)));
		ASSERT_THAT(IsNear(10.0, Target.VecVal.X, 0.001));
		ASSERT_THAT(IsNear(20.0, Target.VecVal.Y, 0.001));
		ASSERT_THAT(IsNear(30.0, Target.VecVal.Z, 0.001));
		ASSERT_THAT(AreEqual(Source.AssetIdVal, Target.AssetIdVal));
		ASSERT_THAT(AreEqual(77, Target.Nested.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Nested")), Target.Nested.Name));
		ASSERT_THAT(IsNear(2.5f, Target.Nested.Speed, 0.001f));
		ASSERT_THAT(AreEqual(3, Target.IntArray.Num()));
		ASSERT_THAT(AreEqual(1, Target.IntArray[0]));
		ASSERT_THAT(AreEqual(2, Target.IntArray[1]));
		ASSERT_THAT(AreEqual(3, Target.IntArray[2]));
		ASSERT_THAT(AreEqual(2, Target.NameFloatMap.Num()));
		ASSERT_THAT(IsTrue(Target.NameFloatMap.Contains(FName("A"))));
		ASSERT_THAT(IsNear(1.0f, Target.NameFloatMap[FName("A")], 0.001f));
		ASSERT_THAT(IsTrue(Target.NameFloatMap.Contains(FName("B"))));
		ASSERT_THAT(IsNear(2.0f, Target.NameFloatMap[FName("B")], 0.001f));
		ASSERT_THAT(AreEqual(1, Target.NameStructMap.Num()));
		ASSERT_THAT(IsTrue(Target.NameStructMap.Contains(FName("X"))));
		ASSERT_THAT(AreEqual(5, Target.NameStructMap[FName("X")].Health));
		ASSERT_THAT(AreEqual(FString(TEXT("MapEntry")), Target.NameStructMap[FName("X")].Name));
	}

	TEST_METHOD(RoundTrip_MapToNestedStruct)
	{
		// Specifically cover the TMap<FName, FArcPersistenceTestStruct> path
		// (Map→GenericStruct recursion: the deepest nesting path in the plan).
		// Note: TMap<FName, TArray<int32>> is rejected by UHT — tested via struct wrapper.
		FArcPersistenceTestDeltaStruct Source;

		FArcPersistenceTestStruct EntryA;
		EntryA.Health = 10;
		EntryA.Name   = TEXT("Alpha");
		EntryA.Speed  = 1.0f;
		EntryA.Scores = {11, 22};
		Source.NameStructMap.Add(FName("Alpha"), EntryA);

		FArcPersistenceTestStruct EntryB;
		EntryB.Health = 20;
		EntryB.Name   = TEXT("Beta");
		EntryB.Speed  = 2.0f;
		Source.NameStructMap.Add(FName("Beta"), EntryB);

		FArcPersistenceTestDeltaStruct Target;
		ArcReflectionSerializerTestsPrivate::RoundTripDelta(Source, Target);

		ASSERT_THAT(AreEqual(2, Target.NameStructMap.Num()));
		ASSERT_THAT(IsTrue(Target.NameStructMap.Contains(FName("Alpha"))));
		ASSERT_THAT(AreEqual(10, Target.NameStructMap[FName("Alpha")].Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha")), Target.NameStructMap[FName("Alpha")].Name));
		ASSERT_THAT(IsNear(1.0f, Target.NameStructMap[FName("Alpha")].Speed, 0.001f));
		ASSERT_THAT(AreEqual(2, Target.NameStructMap[FName("Alpha")].Scores.Num()));
		ASSERT_THAT(AreEqual(11, Target.NameStructMap[FName("Alpha")].Scores[0]));
		ASSERT_THAT(AreEqual(22, Target.NameStructMap[FName("Alpha")].Scores[1]));
		ASSERT_THAT(IsTrue(Target.NameStructMap.Contains(FName("Beta"))));
		ASSERT_THAT(AreEqual(20, Target.NameStructMap[FName("Beta")].Health));
	}

	TEST_METHOD(SchemaVersion_StableBeforeAndAfterSave)
	{
		const UStruct* Type = FArcPersistenceTestDeltaStruct::StaticStruct();

		// Compute before any save (may trigger plan build itself, or compute standalone).
		const uint32 VersionBefore = FArcReflectionSerializer::ComputeSchemaVersion(Type);

		// Perform a save to ensure the plan is in the cache.
		FArcPersistenceTestDeltaStruct Source;
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(Type, &Source, SaveAr);
		(void)SaveAr.Finalize();

		// Version must be identical — plan build must not change the hash.
		const uint32 VersionAfter = FArcReflectionSerializer::ComputeSchemaVersion(Type);
		ASSERT_THAT(AreEqual(VersionBefore, VersionAfter));
	}

	TEST_METHOD(CachePopulated_AfterSave)
	{
		const UStruct* Type = FArcPersistenceTestDeltaStruct::StaticStruct();

		// Flush so we start from a known-empty state.
		FArcReflectionSerializer::FlushPlanCache();
		ASSERT_THAT(IsFalse(FArcReflectionSerializer::HasCachedPlan(Type, true)));

		// A Save must populate the SaveGame-only plan.
		FArcPersistenceTestDeltaStruct Source;
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(Type, &Source, SaveAr);
		(void)SaveAr.Finalize();

		ASSERT_THAT(IsTrue(FArcReflectionSerializer::HasCachedPlan(Type, true)));
	}
};

// =============================================================================
// TEST_CLASS 5: Opt-in delta serialization (#2) — Step 8
// =============================================================================

TEST_CLASS(ArcReflectionSerializer_Delta, "ArcPersistence.Serialization.Reflection.Delta")
{
	/**
	 * Helper: save Source against the struct default via SaveDeltaFromScriptStructDefault,
	 * return the raw JSON bytes for inspection.
	 */
	TArray<uint8> SaveDeltaBytes(const FArcPersistenceTestDeltaStruct& Source)
	{
		const UScriptStruct* Struct = FArcPersistenceTestDeltaStruct::StaticStruct();
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::SaveDeltaFromScriptStructDefault(Struct, &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();
		return Data;
	}

	TEST_METHOD(Delta_OmitsDefaultValuedKeys)
	{
		// Only IntVal differs from the struct default (0 → 99); all other fields are default.
		FArcPersistenceTestDeltaStruct Source;
		Source.IntVal = 99;

		const TArray<uint8> Json = SaveDeltaBytes(Source);
		ArcJson::FDocPtr Doc = ArcJson::Parse(Json);
		ASSERT_THAT(IsNotNull(Doc.Get()));
		const yyjson_val* Data = yyjson_obj_get(yyjson_doc_get_root(Doc.Get()), "data");

		// The changed field must be present.
		ASSERT_THAT(IsNotNull(yyjson_obj_get(Data, "IntVal")));

		// Default-valued fields must be absent (delta omits them).
		ASSERT_THAT(IsNull(yyjson_obj_get(Data, "FloatVal")));
		ASSERT_THAT(IsNull(yyjson_obj_get(Data, "GuidVal")));
		ASSERT_THAT(IsNull(yyjson_obj_get(Data, "IntArray")));
		ASSERT_THAT(IsNull(yyjson_obj_get(Data, "NameFloatMap")));
		ASSERT_THAT(IsNull(yyjson_obj_get(Data, "NameStructMap")));
	}

	TEST_METHOD(Delta_AllDefault_ProducesNoPropertyKeys)
	{
		// Fully default instance — delta save should write no property keys.
		FArcPersistenceTestDeltaStruct Source;

		const TArray<uint8> Json = SaveDeltaBytes(Source);
		ArcJson::FDocPtr Doc = ArcJson::Parse(Json);
		ASSERT_THAT(IsNotNull(Doc.Get()));
		const yyjson_val* Data = yyjson_obj_get(yyjson_doc_get_root(Doc.Get()), "data");

		// No property keys should be present in the data section.
		ASSERT_THAT(AreEqual(static_cast<size_t>(0), yyjson_obj_size(Data)));
	}

	TEST_METHOD(Delta_RoundTrip_IntoDefaultInstance)
	{
		// Populate some but not all fields; delta-save, then load into a default instance.
		// Missing keys must leave the destination at its initialized default.
		FArcPersistenceTestDeltaStruct Source;
		Source.IntVal   = 7;
		Source.FloatVal = 1.5f;
		Source.IntArray = {100, 200};
		Source.NameFloatMap.Add(FName("Z"), 9.0f);

		const UScriptStruct* Struct = FArcPersistenceTestDeltaStruct::StaticStruct();
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::SaveDeltaFromScriptStructDefault(Struct, &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		// Load into a fresh default instance.
		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));
		FArcPersistenceTestDeltaStruct Target;
		FArcReflectionSerializer::Load(Struct, &Target, LoadAr);

		// Changed fields must match source.
		ASSERT_THAT(AreEqual(7, Target.IntVal));
		ASSERT_THAT(IsNear(1.5f, Target.FloatVal, 0.001f));
		ASSERT_THAT(AreEqual(2, Target.IntArray.Num()));
		ASSERT_THAT(AreEqual(100, Target.IntArray[0]));
		ASSERT_THAT(AreEqual(200, Target.IntArray[1]));
		ASSERT_THAT(AreEqual(1, Target.NameFloatMap.Num()));
		ASSERT_THAT(IsTrue(Target.NameFloatMap.Contains(FName("Z"))));
		ASSERT_THAT(IsNear(9.0f, Target.NameFloatMap[FName("Z")], 0.001f));

		// Fields not saved must remain at default.
		ASSERT_THAT(AreEqual(static_cast<uint8>(EArcPersistenceDeltaTestEnum::Default),
		                      static_cast<uint8>(Target.EnumVal)));
		ASSERT_THAT(IsFalse(Target.GuidVal.IsValid()));
		ASSERT_THAT(IsFalse(Target.TagVal.IsValid()));
		ASSERT_THAT(IsNear(0.0, Target.VecVal.X, 0.001));
		ASSERT_THAT(IsFalse(Target.AssetIdVal.IsValid()));
	}

	TEST_METHOD(PlainSave_AllDefault_ProducesNonEmptyPropertySet)
	{
		// Proves that the default-skip gate is OPT-IN:
		// plain Save (not delta) must write property entries even for a fully default struct.
		FArcPersistenceTestDeltaStruct Source;

		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(
			FArcPersistenceTestDeltaStruct::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Bytes = SaveAr.Finalize();

		ArcJson::FDocPtr Doc = ArcJson::Parse(Bytes);
		ASSERT_THAT(IsNotNull(Doc.Get()));
		const yyjson_val* Data = yyjson_obj_get(yyjson_doc_get_root(Doc.Get()), "data");

		// At least one property key must be present — plain save writes everything.
		ASSERT_THAT(AreNotEqual(static_cast<size_t>(0), yyjson_obj_size(Data)));
	}
};

// =============================================================================
// TEST_CLASS 6: Editor plan-cache invalidation smoke (Step 9)
// Guarded WITH_EDITOR: InvalidatePlan is only available in editor/non-shipping
// builds (see ArcReflectionSerializer.h #if WITH_EDITOR || !UE_BUILD_SHIPPING).
// Simulates the Blueprint-recompile invalidation path: save → invalidate → re-save.
// =============================================================================

#if WITH_EDITOR

TEST_CLASS(ArcReflectionSerializer_PlanCacheInvalidation,
           "ArcPersistence.Serialization.Reflection.PlanCacheInvalidation")
{
	TEST_METHOD(InvalidatePlan_ClearsAndRebuildsCache)
	{
		const UStruct* Type = FArcPersistenceTestDeltaStruct::StaticStruct();

		// 1. Populate the cache by saving once.
		{
			FArcPersistenceTestDeltaStruct Source;
			FArcJsonSaveArchive SaveAr;
			SaveAr.SetVersion(1);
			FArcReflectionSerializer::Save(Type, &Source, SaveAr);
			(void)SaveAr.Finalize();
		}
		ASSERT_THAT(IsTrue(FArcReflectionSerializer::HasCachedPlan(Type, true)));

		// 2. Evict the plan (simulates Blueprint recompile / OnObjectsReplaced).
		FArcReflectionSerializer::InvalidatePlan(Type);
		ASSERT_THAT(IsFalse(FArcReflectionSerializer::HasCachedPlan(Type, true)));

		// 3. Save again — GetOrBuildPlan must rebuild on miss and cache the result.
		{
			FArcPersistenceTestDeltaStruct Source;
			FArcJsonSaveArchive SaveAr;
			SaveAr.SetVersion(1);
			FArcReflectionSerializer::Save(Type, &Source, SaveAr);
			(void)SaveAr.Finalize();
		}
		ASSERT_THAT(IsTrue(FArcReflectionSerializer::HasCachedPlan(Type, true)));
	}
};

#endif // WITH_EDITOR
