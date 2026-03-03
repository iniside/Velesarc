// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "ArcPersistenceTestTypes.h"

// =============================================================================
// Tests for FArcMassFragmentSerializer — static methods that work on
// plain UScriptStruct (no Mass entity manager needed).
// =============================================================================

TEST_CLASS(ArcMassFragmentSerializer, "ArcPersistence.MassFragmentSerializer")
{
	TEST_METHOD(SaveAndLoadFragment_RoundTrip)
	{
		const UScriptStruct* Type = FArcPersistenceTestStruct::StaticStruct();

		FArcPersistenceTestStruct Source;
		Source.Health = 200;
		Source.Name = TEXT("TestEntity");
		Source.Speed = 5.0f;
		Source.Location = FVector(10.0, 20.0, 30.0);
		Source.Scores = {1, 2, 3};

		// Save: wrap in a struct scope like SaveEntityFragments does
		FArcJsonSaveArchive SaveAr;
		SaveAr.BeginStruct(Type->GetFName());
		SaveAr.WriteProperty(FName("_type"), FString(Type->GetPathName()));
		FArcMassFragmentSerializer::SaveFragment(Type, &Source, SaveAr);
		SaveAr.EndStruct();
		TArray<uint8> Data = SaveAr.Finalize();

		// Load
		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));
		ASSERT_THAT(IsTrue(LoadAr.BeginStruct(Type->GetFName())));

		FArcPersistenceTestStruct Target;
		FArcMassFragmentSerializer::LoadFragment(Type, &Target, LoadAr);
		LoadAr.EndStruct();

		ASSERT_THAT(AreEqual(200, Target.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("TestEntity")), Target.Name));
		ASSERT_THAT(IsNear(5.0f, Target.Speed, 0.001f));
		ASSERT_THAT(IsNear(10.0, Target.Location.X, 0.001));
		ASSERT_THAT(AreEqual(3, Target.Scores.Num()));
		ASSERT_THAT(AreEqual(1, Target.Scores[0]));
	}

	TEST_METHOD(ShouldSerializeFragment_EmptyConfig_ChecksSaveGame)
	{
		FArcMassPersistenceConfigFragment Config;

		// FArcPersistenceTestStruct has SaveGame properties — should pass
		ASSERT_THAT(IsTrue(
			FArcMassFragmentSerializer::ShouldSerializeFragment(
				FArcPersistenceTestStruct::StaticStruct(), Config)));
	}

	TEST_METHOD(ShouldSerializeFragment_AllowList_FiltersIn)
	{
		FArcMassPersistenceConfigFragment Config;
		Config.AllowedFragments.Add(FArcPersistenceTestStruct::StaticStruct());

		ASSERT_THAT(IsTrue(
			FArcMassFragmentSerializer::ShouldSerializeFragment(
				FArcPersistenceTestStruct::StaticStruct(), Config)));
	}

	TEST_METHOD(ShouldSerializeFragment_AllowList_FiltersOut)
	{
		FArcMassPersistenceConfigFragment Config;
		Config.AllowedFragments.Add(FArcPersistenceTestStruct::StaticStruct());

		// Not in allow list
		ASSERT_THAT(IsFalse(
			FArcMassFragmentSerializer::ShouldSerializeFragment(
				FArcPersistenceTestNestedStruct::StaticStruct(), Config)));
	}

	TEST_METHOD(ShouldSerializeFragment_DisallowList_Excludes)
	{
		FArcMassPersistenceConfigFragment Config;
		Config.DisallowedFragments.Add(FArcPersistenceTestStruct::StaticStruct());

		ASSERT_THAT(IsFalse(
			FArcMassFragmentSerializer::ShouldSerializeFragment(
				FArcPersistenceTestStruct::StaticStruct(), Config)));
	}

	TEST_METHOD(ShouldSerializeFragment_DisallowTakesPriority)
	{
		FArcMassPersistenceConfigFragment Config;
		Config.AllowedFragments.Add(FArcPersistenceTestStruct::StaticStruct());
		Config.DisallowedFragments.Add(FArcPersistenceTestStruct::StaticStruct());

		// Disallow wins over allow
		ASSERT_THAT(IsFalse(
			FArcMassFragmentSerializer::ShouldSerializeFragment(
				FArcPersistenceTestStruct::StaticStruct(), Config)));
	}

	TEST_METHOD(SaveFragment_NestedStruct_RoundTrip)
	{
		const UScriptStruct* Type = FArcPersistenceTestNestedStruct::StaticStruct();

		FArcPersistenceTestNestedStruct Source;
		Source.Inner.Health = 42;
		Source.Inner.Name = TEXT("Nested");
		Source.Multiplier = 3.14f;

		FArcJsonSaveArchive SaveAr;
		SaveAr.BeginStruct(Type->GetFName());
		FArcMassFragmentSerializer::SaveFragment(Type, &Source, SaveAr);
		SaveAr.EndStruct();
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data)));
		ASSERT_THAT(IsTrue(LoadAr.BeginStruct(Type->GetFName())));

		FArcPersistenceTestNestedStruct Target;
		FArcMassFragmentSerializer::LoadFragment(Type, &Target, LoadAr);
		LoadAr.EndStruct();

		ASSERT_THAT(AreEqual(42, Target.Inner.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Nested")), Target.Inner.Name));
		ASSERT_THAT(IsNear(3.14f, Target.Multiplier, 0.001f));
	}
};
