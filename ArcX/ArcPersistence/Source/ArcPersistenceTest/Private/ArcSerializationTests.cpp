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
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "GameplayTagContainer.h"

// =============================================================================
// TEST_CLASS 1: Leaf-type round-trip tests
// =============================================================================

TEST_CLASS(ArcJsonArchive_LeafTypes, "ArcPersistence.Serialization.JsonArchive.LeafTypes")
{
	TEST_METHOD(RoundTrips_Bool)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Flag"), true);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));
		ASSERT_THAT(AreEqual(1u, LoadAr.GetVersion()));

		bool Value = false;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Flag"), Value)));
		ASSERT_THAT(IsTrue(Value));
	}

	TEST_METHOD(RoundTrips_Int32)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Count"), 42);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Count"), Value)));
		ASSERT_THAT(AreEqual(42, Value));
	}

	TEST_METHOD(RoundTrips_Int64)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("BigNumber"), static_cast<int64>(123456789012LL));
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		int64 Value = 0;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("BigNumber"), Value)));
		ASSERT_THAT(AreEqual(static_cast<int64>(123456789012LL), Value));
	}

	TEST_METHOD(RoundTrips_Uint8)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Byte"), static_cast<uint8>(255));
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		uint8 Value = 0;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Byte"), Value)));
		ASSERT_THAT(AreEqual(static_cast<uint8>(255), Value));
	}

	TEST_METHOD(RoundTrips_Float)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Ratio"), 3.14f);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		float Value = 0.0f;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Ratio"), Value)));
		ASSERT_THAT(IsNear(3.14f, Value, 0.001f));
	}

	TEST_METHOD(RoundTrips_Double)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Precise"), 3.14159265358979);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		double Value = 0.0;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Precise"), Value)));
		ASSERT_THAT(IsNear(3.14159265358979, Value, 0.0000000001));
	}

	TEST_METHOD(RoundTrips_FString)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("ItemName"), FString(TEXT("TestItem")));
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		FString Value;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("ItemName"), Value)));
		ASSERT_THAT(AreEqual(FString(TEXT("TestItem")), Value));
	}

	TEST_METHOD(RoundTrips_FName)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		const FName WrittenName(TEXT("TestName"));
		SaveAr.WriteProperty(FName("Tag"), WrittenName);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		FName Value;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Tag"), Value)));
		ASSERT_THAT(AreEqual(FName(TEXT("TestName")), Value));
	}

	TEST_METHOD(RoundTrips_FGuid)
	{
		const FGuid Original = FGuid::NewGuid();

		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Id"), Original);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		FGuid Value;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Id"), Value)));
		ASSERT_THAT(AreEqual(Original, Value));
	}

	TEST_METHOD(RoundTrips_FGameplayTag)
	{
		// Use RequestGameplayTag to get a tag. If the tag does not exist in the
		// project's tag tables the call may warn, but the round-trip mechanism
		// itself is what we are testing here.
		const FGameplayTag Original = FGameplayTag::RequestGameplayTag(FName(TEXT("Test.RoundTrip")), false);

		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Tag"), Original);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		FGameplayTag Value;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Tag"), Value)));
		ASSERT_THAT(AreEqual(Original, Value));
	}

	TEST_METHOD(RoundTrips_FVector)
	{
		const FVector Original(1.0, 2.0, 3.0);

		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Position"), Original);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		FVector Value = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Position"), Value)));
		ASSERT_THAT(IsNear(1.0, Value.X, 0.001));
		ASSERT_THAT(IsNear(2.0, Value.Y, 0.001));
		ASSERT_THAT(IsNear(3.0, Value.Z, 0.001));
	}

	TEST_METHOD(RoundTrips_FRotator)
	{
		const FRotator Original(45.0, 90.0, 0.0);

		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Rotation"), Original);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		FRotator Value = FRotator::ZeroRotator;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Rotation"), Value)));
		ASSERT_THAT(IsNear(45.0, Value.Pitch, 0.001));
		ASSERT_THAT(IsNear(90.0, Value.Yaw, 0.001));
		ASSERT_THAT(IsNear(0.0, Value.Roll, 0.001));
	}

	TEST_METHOD(RoundTrips_FTransform)
	{
		const FRotator Rot(45.0, 90.0, 0.0);
		const FVector Trans(100.0, 200.0, 300.0);
		const FVector Scale(1.0, 1.0, 1.0);
		const FTransform Original(Rot, Trans, Scale);

		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Transform"), Original);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		FTransform Value;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Transform"), Value)));

		// Verify translation
		const FVector LoadedTrans = Value.GetTranslation();
		ASSERT_THAT(IsNear(100.0, LoadedTrans.X, 0.001));
		ASSERT_THAT(IsNear(200.0, LoadedTrans.Y, 0.001));
		ASSERT_THAT(IsNear(300.0, LoadedTrans.Z, 0.001));

		// Verify rotation via quaternion comparison
		const FQuat OrigQuat = Original.GetRotation();
		const FQuat LoadedQuat = Value.GetRotation();
		ASSERT_THAT(IsNear(OrigQuat.X, LoadedQuat.X, 0.001));
		ASSERT_THAT(IsNear(OrigQuat.Y, LoadedQuat.Y, 0.001));
		ASSERT_THAT(IsNear(OrigQuat.Z, LoadedQuat.Z, 0.001));
		ASSERT_THAT(IsNear(OrigQuat.W, LoadedQuat.W, 0.001));

		// Verify scale
		const FVector LoadedScale = Value.GetScale3D();
		ASSERT_THAT(IsNear(1.0, LoadedScale.X, 0.001));
		ASSERT_THAT(IsNear(1.0, LoadedScale.Y, 0.001));
		ASSERT_THAT(IsNear(1.0, LoadedScale.Z, 0.001));
	}

	TEST_METHOD(MissingKey_ReturnsFalse)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Existing"), 42);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		int32 Value = 99;
		ASSERT_THAT(IsFalse(LoadAr.ReadProperty(FName("NonExistent"), Value)));
		// Output value must be unchanged when key is missing.
		ASSERT_THAT(AreEqual(99, Value));
	}

	TEST_METHOD(VersionRoundTrips)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(42);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));
		ASSERT_THAT(AreEqual(42u, LoadAr.GetVersion()));
	}
};

// =============================================================================
// TEST_CLASS 2: Nesting round-trip tests
// =============================================================================

TEST_CLASS(ArcJsonArchive_Nesting, "ArcPersistence.Serialization.JsonArchive.Nesting")
{
	TEST_METHOD(RoundTrips_NestedStruct)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.BeginStruct(FName("Stats"));
		SaveAr.WriteProperty(FName("Health"), 100.0f);
		SaveAr.WriteProperty(FName("Stamina"), 75.5f);
		SaveAr.EndStruct();
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		ASSERT_THAT(IsTrue(LoadAr.BeginStruct(FName("Stats"))));

		float Health = 0.0f;
		float Stamina = 0.0f;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Health"), Health)));
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Stamina"), Stamina)));
		ASSERT_THAT(IsNear(100.0f, Health, 0.001f));
		ASSERT_THAT(IsNear(75.5f, Stamina, 0.001f));

		LoadAr.EndStruct();
	}

	TEST_METHOD(RoundTrips_Array)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.BeginArray(FName("Scores"), 3);
		for (int32 i = 0; i < 3; ++i)
		{
			SaveAr.BeginArrayElement(i);
			SaveAr.WriteProperty(FName("Value"), (i + 1) * 10);
			SaveAr.EndArrayElement();
		}
		SaveAr.EndArray();
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		int32 Count = 0;
		ASSERT_THAT(IsTrue(LoadAr.BeginArray(FName("Scores"), Count)));
		ASSERT_THAT(AreEqual(3, Count));

		for (int32 i = 0; i < Count; ++i)
		{
			ASSERT_THAT(IsTrue(LoadAr.BeginArrayElement(i)));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Value"), Value)));
			ASSERT_THAT(AreEqual((i + 1) * 10, Value));

			LoadAr.EndArrayElement();
		}
		LoadAr.EndArray();
	}

	TEST_METHOD(RoundTrips_NestedArrayOfStructs)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.BeginArray(FName("Items"), 2);

		// Element 0
		SaveAr.BeginArrayElement(0);
		SaveAr.WriteProperty(FName("Name"), FString(TEXT("Sword")));
		SaveAr.WriteProperty(FName("Damage"), 50);
		SaveAr.EndArrayElement();

		// Element 1
		SaveAr.BeginArrayElement(1);
		SaveAr.WriteProperty(FName("Name"), FString(TEXT("Shield")));
		SaveAr.WriteProperty(FName("Damage"), 10);
		SaveAr.EndArrayElement();

		SaveAr.EndArray();
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		int32 Count = 0;
		ASSERT_THAT(IsTrue(LoadAr.BeginArray(FName("Items"), Count)));
		ASSERT_THAT(AreEqual(2, Count));

		// Element 0
		ASSERT_THAT(IsTrue(LoadAr.BeginArrayElement(0)));
		FString Name0;
		int32 Damage0 = 0;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Name"), Name0)));
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Damage"), Damage0)));
		ASSERT_THAT(AreEqual(FString(TEXT("Sword")), Name0));
		ASSERT_THAT(AreEqual(50, Damage0));
		LoadAr.EndArrayElement();

		// Element 1
		ASSERT_THAT(IsTrue(LoadAr.BeginArrayElement(1)));
		FString Name1;
		int32 Damage1 = 0;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Name"), Name1)));
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Damage"), Damage1)));
		ASSERT_THAT(AreEqual(FString(TEXT("Shield")), Name1));
		ASSERT_THAT(AreEqual(10, Damage1));
		LoadAr.EndArrayElement();

		LoadAr.EndArray();
	}

	TEST_METHOD(MissingStruct_ReturnsFalse)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("SomeValue"), 1);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		ASSERT_THAT(IsFalse(LoadAr.BeginStruct(FName("NonExistentStruct"))));
	}

	TEST_METHOD(MissingArray_ReturnsFalse)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("SomeValue"), 1);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		int32 Count = -1;
		ASSERT_THAT(IsFalse(LoadAr.BeginArray(FName("NonExistentArray"), Count)));
		// Count should be untouched on failure.
		ASSERT_THAT(AreEqual(-1, Count));
	}

	TEST_METHOD(EmptyArray_RoundTrips)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.BeginArray(FName("EmptyList"), 0);
		SaveAr.EndArray();
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		int32 Count = -1;
		ASSERT_THAT(IsTrue(LoadAr.BeginArray(FName("EmptyList"), Count)));
		ASSERT_THAT(AreEqual(0, Count));
		LoadAr.EndArray();
	}
};

// =============================================================================
// TEST_CLASS 3: Edge case tests
// =============================================================================

TEST_CLASS(ArcJsonArchive_EdgeCases, "ArcPersistence.Serialization.JsonArchive.EdgeCases")
{
	TEST_METHOD(EmptyArchive_RoundTrips)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(7);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));
		ASSERT_THAT(AreEqual(7u, LoadAr.GetVersion()));

		// No data was written, so any read should return false.
		int32 Value = 0;
		ASSERT_THAT(IsFalse(LoadAr.ReadProperty(FName("Anything"), Value)));
	}

	TEST_METHOD(InvalidJson_InitFails)
	{
		// Fill with garbage bytes that are not valid JSON.
		TArray<uint8> GarbageData;
		const char* Garbage = "this is not json {{[[[";
		GarbageData.Append(reinterpret_cast<const uint8*>(Garbage), FCStringAnsi::Strlen(Garbage));

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsFalse(LoadAr.InitializeFromData(GarbageData)));
	}

	TEST_METHOD(MissingVersionField_InitFails)
	{
		// Valid JSON but missing the required "version" key.
		const char* JsonStr = R"({"data":{}})";
		TArray<uint8> Data;
		Data.Append(reinterpret_cast<const uint8*>(JsonStr), FCStringAnsi::Strlen(JsonStr));

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsFalse(LoadAr.InitializeFromData(Data)));
	}

	TEST_METHOD(MultiplePropertiesSameLevel)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		SaveAr.WriteProperty(FName("Alpha"), 1);
		SaveAr.WriteProperty(FName("Beta"), FString(TEXT("hello")));
		SaveAr.WriteProperty(FName("Gamma"), 3.14f);
		SaveAr.WriteProperty(FName("Delta"), true);
		SaveAr.WriteProperty(FName("Epsilon"), static_cast<int64>(999LL));
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));

		int32 Alpha = 0;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Alpha"), Alpha)));
		ASSERT_THAT(AreEqual(1, Alpha));

		FString Beta;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Beta"), Beta)));
		ASSERT_THAT(AreEqual(FString(TEXT("hello")), Beta));

		float Gamma = 0.0f;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Gamma"), Gamma)));
		ASSERT_THAT(IsNear(3.14f, Gamma, 0.001f));

		bool Delta = false;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Delta"), Delta)));
		ASSERT_THAT(IsTrue(Delta));

		int64 Epsilon = 0;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("Epsilon"), Epsilon)));
		ASSERT_THAT(AreEqual(static_cast<int64>(999LL), Epsilon));
	}
};
