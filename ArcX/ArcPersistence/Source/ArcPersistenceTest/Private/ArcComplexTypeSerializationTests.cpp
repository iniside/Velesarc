// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Serialization/ArcReflectionSerializer.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "ArcPersistenceTestTypes.h"
#include "UObject/PrimaryAssetId.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Components/ActorTestSpawner.h"
#include "NativeGameplayTags.h"

// ---- Test gameplay tags (must be registered as native tags) ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTest_Status_Burning, "ArcTest.Status.Burning");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTest_Status_Frozen, "ArcTest.Status.Frozen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTest_Status_Poisoned, "ArcTest.Status.Poisoned");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTest_Class_Warrior, "ArcTest.Class.Warrior");

// =============================================================================
// TEST_CLASS 1: Complex reference type round-trip serialization
// =============================================================================

TEST_CLASS(ArcComplexTypes_References, "ArcPersistence.Serialization.ComplexTypes.References")
{
	template<typename T>
	void RoundTrip(const T& Source, T& Target)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(T::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		check(LoadAr.InitializeFromData(Data));
		FArcReflectionSerializer::Load(T::StaticStruct(), &Target, LoadAr);
	}

	TEST_METHOD(SoftObjectPtr_RoundTrip)
	{
		FArcPersistenceTestComplexStruct Source;
		Source.SoftRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Script/CoreUObject.Object")));

		FArcPersistenceTestComplexStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(Source.SoftRef.GetUniqueID(), Target.SoftRef.GetUniqueID()));
	}

	TEST_METHOD(SoftObjectPtr_Null_RoundTrip)
	{
		FArcPersistenceTestComplexStruct Source;
		// Source.SoftRef is default (null)

		FArcPersistenceTestComplexStruct Target;
		Target.SoftRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Script/CoreUObject.Object")));

		RoundTrip(Source, Target);

		ASSERT_THAT(IsTrue(Target.SoftRef.IsNull()));
	}

	TEST_METHOD(SoftClassPtr_RoundTrip)
	{
		FArcPersistenceTestComplexStruct Source;
		Source.SoftClass = TSoftClassPtr<UObject>(FSoftObjectPath(AActor::StaticClass()));

		FArcPersistenceTestComplexStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(Source.SoftClass.GetUniqueID(), Target.SoftClass.GetUniqueID()));
	}

	TEST_METHOD(SubclassOf_RoundTrip)
	{
		FArcPersistenceTestComplexStruct Source;
		Source.SubclassRef = APawn::StaticClass();

		FArcPersistenceTestComplexStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(APawn::StaticClass(), Target.SubclassRef.Get()));
	}

	TEST_METHOD(SubclassOf_Null_RoundTrip)
	{
		FArcPersistenceTestComplexStruct Source;
		// Source.SubclassRef is default (null)

		FArcPersistenceTestComplexStruct Target;
		Target.SubclassRef = APawn::StaticClass();

		RoundTrip(Source, Target);

		ASSERT_THAT(IsNull(Target.SubclassRef.Get()));
	}

	TEST_METHOD(ObjectPtr_RoundTrip)
	{
		FArcPersistenceTestComplexStruct Source;
		Source.HardRef = UObject::StaticClass();

		FArcPersistenceTestComplexStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(static_cast<UObject*>(UObject::StaticClass()), Target.HardRef.Get()));
	}

	TEST_METHOD(ObjectPtr_Null_RoundTrip)
	{
		FArcPersistenceTestComplexStruct Source;
		// Source.HardRef is default (null)

		FArcPersistenceTestComplexStruct Target;
		Target.HardRef = UObject::StaticClass();

		RoundTrip(Source, Target);

		ASSERT_THAT(IsNull(Target.HardRef.Get()));
	}

	TEST_METHOD(PrimaryAssetId_RoundTrip)
	{
		FArcPersistenceTestComplexStruct Source;
		Source.AssetId = FPrimaryAssetId(FPrimaryAssetType("Map"), FName("TestLevel"));

		FArcPersistenceTestComplexStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(Source.AssetId, Target.AssetId));
	}

	TEST_METHOD(PrimaryAssetId_Invalid_RoundTrip)
	{
		FArcPersistenceTestComplexStruct Source;
		// Source.AssetId is default (invalid)

		FArcPersistenceTestComplexStruct Target;
		Target.AssetId = FPrimaryAssetId(FPrimaryAssetType("Map"), FName("ExistingLevel"));

		RoundTrip(Source, Target);

		ASSERT_THAT(IsFalse(Target.AssetId.IsValid()));
	}

	TEST_METHOD(AllReferenceTypes_Combined)
	{
		FArcPersistenceTestComplexStruct Source;
		Source.SoftRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Script/CoreUObject.Object")));
		Source.SoftClass = TSoftClassPtr<UObject>(FSoftObjectPath(AActor::StaticClass()));
		Source.SubclassRef = APawn::StaticClass();
		Source.HardRef = UObject::StaticClass();
		Source.AssetId = FPrimaryAssetId(FPrimaryAssetType("Map"), FName("TestLevel"));

		FArcPersistenceTestComplexStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(Source.SoftRef.GetUniqueID(), Target.SoftRef.GetUniqueID()));
		ASSERT_THAT(AreEqual(Source.SoftClass.GetUniqueID(), Target.SoftClass.GetUniqueID()));
		ASSERT_THAT(AreEqual(APawn::StaticClass(), Target.SubclassRef.Get()));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(UObject::StaticClass()), Target.HardRef.Get()));
		ASSERT_THAT(AreEqual(Source.AssetId, Target.AssetId));
	}
};

// =============================================================================
// TEST_CLASS 2: TMap round-trip serialization
// =============================================================================

TEST_CLASS(ArcComplexTypes_Map, "ArcPersistence.Serialization.ComplexTypes.Map")
{
	template<typename T>
	void RoundTrip(const T& Source, T& Target)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(T::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		check(LoadAr.InitializeFromData(Data));
		FArcReflectionSerializer::Load(T::StaticStruct(), &Target, LoadAr);
	}

	TEST_METHOD(Map_StringInt_RoundTrip)
	{
		FArcPersistenceTestMapStruct Source;
		Source.StringIntMap.Add(TEXT("Alpha"), 1);
		Source.StringIntMap.Add(TEXT("Beta"), 2);
		Source.StringIntMap.Add(TEXT("Gamma"), 3);

		FArcPersistenceTestMapStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(3, Target.StringIntMap.Num()));
		ASSERT_THAT(AreEqual(1, Target.StringIntMap[TEXT("Alpha")]));
		ASSERT_THAT(AreEqual(2, Target.StringIntMap[TEXT("Beta")]));
		ASSERT_THAT(AreEqual(3, Target.StringIntMap[TEXT("Gamma")]));
	}

	TEST_METHOD(Map_NameString_RoundTrip)
	{
		FArcPersistenceTestMapStruct Source;
		Source.NameStringMap.Add(FName("Key1"), TEXT("Value1"));
		Source.NameStringMap.Add(FName("Key2"), TEXT("Value2"));

		FArcPersistenceTestMapStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(2, Target.NameStringMap.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Value1")), Target.NameStringMap[FName("Key1")]));
		ASSERT_THAT(AreEqual(FString(TEXT("Value2")), Target.NameStringMap[FName("Key2")]));
	}

	TEST_METHOD(Map_IntStruct_RoundTrip)
	{
		FArcPersistenceTestMapStruct Source;

		FArcPersistenceTestStruct Entry0;
		Entry0.Health = 100;
		Entry0.Name = TEXT("Warrior");
		Entry0.Speed = 3.5f;
		Source.IntStructMap.Add(0, Entry0);

		FArcPersistenceTestStruct Entry1;
		Entry1.Health = 80;
		Entry1.Name = TEXT("Mage");
		Entry1.Speed = 2.0f;
		Source.IntStructMap.Add(1, Entry1);

		FArcPersistenceTestMapStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(2, Target.IntStructMap.Num()));
		ASSERT_THAT(IsTrue(Target.IntStructMap.Contains(0)));
		ASSERT_THAT(IsTrue(Target.IntStructMap.Contains(1)));

		ASSERT_THAT(AreEqual(100, Target.IntStructMap[0].Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Warrior")), Target.IntStructMap[0].Name));
		ASSERT_THAT(IsNear(3.5f, Target.IntStructMap[0].Speed, 0.001f));

		ASSERT_THAT(AreEqual(80, Target.IntStructMap[1].Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Mage")), Target.IntStructMap[1].Name));
		ASSERT_THAT(IsNear(2.0f, Target.IntStructMap[1].Speed, 0.001f));
	}

	TEST_METHOD(Map_Empty_RoundTrip)
	{
		FArcPersistenceTestMapStruct Source;
		// Source maps are all empty

		FArcPersistenceTestMapStruct Target;
		Target.StringIntMap.Add(TEXT("Old"), 999);
		Target.NameStringMap.Add(FName("OldKey"), TEXT("OldValue"));

		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(0, Target.StringIntMap.Num()));
		ASSERT_THAT(AreEqual(0, Target.NameStringMap.Num()));
	}

	TEST_METHOD(Map_Overwrites_ExistingData)
	{
		FArcPersistenceTestMapStruct Source;
		Source.StringIntMap.Add(TEXT("New"), 42);

		FArcPersistenceTestMapStruct Target;
		Target.StringIntMap.Add(TEXT("OldA"), 1);
		Target.StringIntMap.Add(TEXT("OldB"), 2);

		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(1, Target.StringIntMap.Num()));
		ASSERT_THAT(IsTrue(Target.StringIntMap.Contains(TEXT("New"))));
		ASSERT_THAT(AreEqual(42, Target.StringIntMap[TEXT("New")]));
		ASSERT_THAT(IsFalse(Target.StringIntMap.Contains(TEXT("OldA"))));
		ASSERT_THAT(IsFalse(Target.StringIntMap.Contains(TEXT("OldB"))));
	}
};

// =============================================================================
// TEST_CLASS 3: TSet round-trip serialization
// =============================================================================

TEST_CLASS(ArcComplexTypes_Set, "ArcPersistence.Serialization.ComplexTypes.Set")
{
	template<typename T>
	void RoundTrip(const T& Source, T& Target)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(T::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		check(LoadAr.InitializeFromData(Data));
		FArcReflectionSerializer::Load(T::StaticStruct(), &Target, LoadAr);
	}

	TEST_METHOD(Set_Name_RoundTrip)
	{
		FArcPersistenceTestSetStruct Source;
		Source.NameSet.Add(FName("Alpha"));
		Source.NameSet.Add(FName("Beta"));
		Source.NameSet.Add(FName("Gamma"));

		FArcPersistenceTestSetStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(3, Target.NameSet.Num()));
		ASSERT_THAT(IsTrue(Target.NameSet.Contains(FName("Alpha"))));
		ASSERT_THAT(IsTrue(Target.NameSet.Contains(FName("Beta"))));
		ASSERT_THAT(IsTrue(Target.NameSet.Contains(FName("Gamma"))));
	}

	TEST_METHOD(Set_Int_RoundTrip)
	{
		FArcPersistenceTestSetStruct Source;
		Source.IntSet.Add(10);
		Source.IntSet.Add(20);
		Source.IntSet.Add(30);

		FArcPersistenceTestSetStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(3, Target.IntSet.Num()));
		ASSERT_THAT(IsTrue(Target.IntSet.Contains(10)));
		ASSERT_THAT(IsTrue(Target.IntSet.Contains(20)));
		ASSERT_THAT(IsTrue(Target.IntSet.Contains(30)));
	}

	TEST_METHOD(Set_String_RoundTrip)
	{
		FArcPersistenceTestSetStruct Source;
		Source.StringSet.Add(TEXT("Hello"));
		Source.StringSet.Add(TEXT("World"));

		FArcPersistenceTestSetStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(2, Target.StringSet.Num()));
		ASSERT_THAT(IsTrue(Target.StringSet.Contains(TEXT("Hello"))));
		ASSERT_THAT(IsTrue(Target.StringSet.Contains(TEXT("World"))));
	}

	TEST_METHOD(Set_Empty_RoundTrip)
	{
		FArcPersistenceTestSetStruct Source;
		// Source sets are all empty

		FArcPersistenceTestSetStruct Target;
		Target.NameSet.Add(FName("Old1"));
		Target.NameSet.Add(FName("Old2"));
		Target.IntSet.Add(999);

		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(0, Target.NameSet.Num()));
		ASSERT_THAT(AreEqual(0, Target.IntSet.Num()));
	}

	TEST_METHOD(Set_Overwrites_ExistingData)
	{
		FArcPersistenceTestSetStruct Source;
		Source.NameSet.Add(FName("OnlyThis"));

		FArcPersistenceTestSetStruct Target;
		Target.NameSet.Add(FName("OldA"));
		Target.NameSet.Add(FName("OldB"));
		Target.NameSet.Add(FName("OldC"));

		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(1, Target.NameSet.Num()));
		ASSERT_THAT(IsTrue(Target.NameSet.Contains(FName("OnlyThis"))));
		ASSERT_THAT(IsFalse(Target.NameSet.Contains(FName("OldA"))));
		ASSERT_THAT(IsFalse(Target.NameSet.Contains(FName("OldB"))));
		ASSERT_THAT(IsFalse(Target.NameSet.Contains(FName("OldC"))));
	}
};

// =============================================================================
// TEST_CLASS 4: Gameplay tag round-trip serialization
// =============================================================================

TEST_CLASS(ArcComplexTypes_GameplayTag, "ArcPersistence.Serialization.ComplexTypes.GameplayTag")
{
	template<typename T>
	void RoundTrip(const T& Source, T& Target)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(T::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		check(LoadAr.InitializeFromData(Data));
		FArcReflectionSerializer::Load(T::StaticStruct(), &Target, LoadAr);
	}

	TEST_METHOD(SingleTag_RoundTrip)
	{
		FArcPersistenceTestTagStruct Source;
		Source.SingleTag = TAG_ArcTest_Status_Burning;

		FArcPersistenceTestTagStruct Target;
		RoundTrip(Source, Target);

		bool bEqual = Target.SingleTag.MatchesTagExact(TAG_ArcTest_Status_Burning);
		ASSERT_THAT(IsTrue(bEqual));
	}

	TEST_METHOD(SingleTag_Invalid_RoundTrip)
	{
		FArcPersistenceTestTagStruct Source;
		// Source.SingleTag is default (invalid)

		FArcPersistenceTestTagStruct Target;
		Target.SingleTag = TAG_ArcTest_Status_Burning;

		RoundTrip(Source, Target);

		ASSERT_THAT(IsFalse(Target.SingleTag.IsValid()));
	}

	TEST_METHOD(TagContainer_RoundTrip)
	{
		FArcPersistenceTestTagStruct Source;
		Source.TagContainer.AddTag(TAG_ArcTest_Status_Burning);
		Source.TagContainer.AddTag(TAG_ArcTest_Status_Frozen);
		Source.TagContainer.AddTag(TAG_ArcTest_Status_Poisoned);

		FArcPersistenceTestTagStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(3, Target.TagContainer.Num()));
		ASSERT_THAT(IsTrue(Target.TagContainer.HasTag(TAG_ArcTest_Status_Burning)));
		ASSERT_THAT(IsTrue(Target.TagContainer.HasTag(TAG_ArcTest_Status_Frozen)));
		ASSERT_THAT(IsTrue(Target.TagContainer.HasTag(TAG_ArcTest_Status_Poisoned)));
	}

	TEST_METHOD(TagContainer_Empty_RoundTrip)
	{
		FArcPersistenceTestTagStruct Source;
		// Source.TagContainer is default (empty)

		FArcPersistenceTestTagStruct Target;
		Target.TagContainer.AddTag(TAG_ArcTest_Status_Burning);
		Target.TagContainer.AddTag(TAG_ArcTest_Status_Frozen);

		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(0, Target.TagContainer.Num()));
	}

	TEST_METHOD(TagContainer_Overwrites_ExistingData)
	{
		FArcPersistenceTestTagStruct Source;
		Source.TagContainer.AddTag(TAG_ArcTest_Class_Warrior);

		FArcPersistenceTestTagStruct Target;
		Target.TagContainer.AddTag(TAG_ArcTest_Status_Burning);
		Target.TagContainer.AddTag(TAG_ArcTest_Status_Frozen);

		RoundTrip(Source, Target);

		ASSERT_THAT(AreEqual(1, Target.TagContainer.Num()));
		ASSERT_THAT(IsTrue(Target.TagContainer.HasTag(TAG_ArcTest_Class_Warrior)));
		ASSERT_THAT(IsFalse(Target.TagContainer.HasTag(TAG_ArcTest_Status_Burning)));
	}

	TEST_METHOD(CombinedTagsAndLabel)
	{
		FArcPersistenceTestTagStruct Source;
		Source.SingleTag = TAG_ArcTest_Class_Warrior;
		Source.TagContainer.AddTag(TAG_ArcTest_Status_Burning);
		Source.TagContainer.AddTag(TAG_ArcTest_Status_Poisoned);
		Source.Label = TEXT("TestCombined");

		FArcPersistenceTestTagStruct Target;
		RoundTrip(Source, Target);

		ASSERT_THAT(IsTrue(Target.SingleTag.MatchesTagExact(TAG_ArcTest_Class_Warrior)));
		ASSERT_THAT(AreEqual(2, Target.TagContainer.Num()));
		ASSERT_THAT(IsTrue(Target.TagContainer.HasTag(TAG_ArcTest_Status_Burning)));
		ASSERT_THAT(IsTrue(Target.TagContainer.HasTag(TAG_ArcTest_Status_Poisoned)));
		ASSERT_THAT(AreEqual(FString(TEXT("TestCombined")), Target.Label));
	}
};

// =============================================================================
// TEST_CLASS 5: Entity-level complex fragment serialization
// =============================================================================

TEST_CLASS(ArcComplexTypes_Entity, "ArcPersistence.Serialization.ComplexTypes.Entity")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
	}

	// Helper: save entity fragments to byte array
	TArray<uint8> SaveEntity(FMassEntityHandle Entity,
		const FArcMassPersistenceConfigFragment& Config)
	{
		FArcJsonSaveArchive SaveAr;
		FArcMassFragmentSerializer::SaveEntityFragments(
			*EntityManager, Entity, Config, SaveAr);
		return SaveAr.Finalize();
	}

	// Helper: load entity fragments from byte array
	void LoadEntity(FMassEntityHandle Entity, const TArray<uint8>& Data)
	{
		FArcJsonLoadArchive LoadAr;
		if (LoadAr.InitializeFromData(Data))
		{
			FArcMassFragmentSerializer::LoadEntityFragments(
				*EntityManager, Entity, LoadAr);
		}
	}

	TEST_METHOD(ComplexFragment_RoundTrip)
	{
		// Create source entity with a complex fragment
		FArcTestComplexFragment ComplexFrag;
		ComplexFrag.AssetRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Script/CoreUObject.Object")));
		ComplexFrag.ActorClass = APawn::StaticClass();
		ComplexFrag.PrimaryAsset = FPrimaryAssetId(FPrimaryAssetType("Weapon"), FName("Sword_01"));
		ComplexFrag.Stats.Add(FName("Strength"), 10);
		ComplexFrag.Stats.Add(FName("Agility"), 15);
		ComplexFrag.Tags.Add(FName("Elite"));
		ComplexFrag.Tags.Add(FName("Boss"));

		FInstancedStruct Instance = FInstancedStruct::Make(ComplexFrag);
		FMassEntityHandle Source = EntityManager->CreateEntity(MakeArrayView(&Instance, 1));

		FArcMassPersistenceConfigFragment Config;
		TArray<uint8> Data = SaveEntity(Source, Config);

		// Create target entity with same archetype, default values
		FArcTestComplexFragment EmptyFrag;
		FInstancedStruct TargetInstance = FInstancedStruct::Make(EmptyFrag);
		FMassEntityHandle Target = EntityManager->CreateEntity(MakeArrayView(&TargetInstance, 1));

		LoadEntity(Target, Data);

		const auto& Result = EntityManager->GetFragmentDataChecked<FArcTestComplexFragment>(Target);

		ASSERT_THAT(AreEqual(ComplexFrag.AssetRef.GetUniqueID(), Result.AssetRef.GetUniqueID()));
		ASSERT_THAT(AreEqual(APawn::StaticClass(), Result.ActorClass.Get()));
		ASSERT_THAT(AreEqual(ComplexFrag.PrimaryAsset, Result.PrimaryAsset));

		ASSERT_THAT(AreEqual(2, Result.Stats.Num()));
		ASSERT_THAT(IsTrue(Result.Stats.Contains(FName("Strength"))));
		ASSERT_THAT(AreEqual(10, Result.Stats[FName("Strength")]));
		ASSERT_THAT(IsTrue(Result.Stats.Contains(FName("Agility"))));
		ASSERT_THAT(AreEqual(15, Result.Stats[FName("Agility")]));

		ASSERT_THAT(AreEqual(2, Result.Tags.Num()));
		ASSERT_THAT(IsTrue(Result.Tags.Contains(FName("Elite"))));
		ASSERT_THAT(IsTrue(Result.Tags.Contains(FName("Boss"))));
	}

	TEST_METHOD(MixedFragments_SimpleAndComplex)
	{
		// Create entity with both Health and Complex fragments
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = 350;
		HealthFrag.Armor = 65.5f;
		HealthFrag.TickCount = 999;

		FArcTestComplexFragment ComplexFrag;
		ComplexFrag.AssetRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Script/CoreUObject.Object")));
		ComplexFrag.ActorClass = APawn::StaticClass();
		ComplexFrag.PrimaryAsset = FPrimaryAssetId(FPrimaryAssetType("Item"), FName("Shield_02"));
		ComplexFrag.Stats.Add(FName("Defense"), 25);
		ComplexFrag.Tags.Add(FName("Rare"));

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(HealthFrag));
		Instances.Add(FInstancedStruct::Make(ComplexFrag));

		FMassEntityHandle Source = EntityManager->CreateEntity(Instances);

		FArcMassPersistenceConfigFragment Config;
		TArray<uint8> Data = SaveEntity(Source, Config);

		// Create target entity with same archetype, default values
		FArcTestHealthFragment EmptyHealth;
		FArcTestComplexFragment EmptyComplex;
		TArray<FInstancedStruct> TargetInstances;
		TargetInstances.Add(FInstancedStruct::Make(EmptyHealth));
		TargetInstances.Add(FInstancedStruct::Make(EmptyComplex));
		FMassEntityHandle Target = EntityManager->CreateEntity(TargetInstances);

		LoadEntity(Target, Data);

		// Verify Health fragment
		const auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(350, H.Health));
		ASSERT_THAT(IsNear(65.5f, H.Armor, 0.001f));
		// TickCount has no SaveGame flag — should remain at default
		ASSERT_THAT(AreEqual(0, H.TickCount));

		// Verify Complex fragment
		const auto& C = EntityManager->GetFragmentDataChecked<FArcTestComplexFragment>(Target);
		ASSERT_THAT(AreEqual(ComplexFrag.AssetRef.GetUniqueID(), C.AssetRef.GetUniqueID()));
		ASSERT_THAT(AreEqual(APawn::StaticClass(), C.ActorClass.Get()));
		ASSERT_THAT(AreEqual(ComplexFrag.PrimaryAsset, C.PrimaryAsset));

		ASSERT_THAT(AreEqual(1, C.Stats.Num()));
		ASSERT_THAT(IsTrue(C.Stats.Contains(FName("Defense"))));
		ASSERT_THAT(AreEqual(25, C.Stats[FName("Defense")]));

		ASSERT_THAT(AreEqual(1, C.Tags.Num()));
		ASSERT_THAT(IsTrue(C.Tags.Contains(FName("Rare"))));
	}
};
