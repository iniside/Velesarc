// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcPersistenceTestTypes.h"
#include "Storage/ArcJsonFileBackend.h"
#include "Serialization/ArcReflectionSerializer.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Components/ActorTestSpawner.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "UObject/PrimaryAssetId.h"

// =============================================================================
// TEST_CLASS 1: FArcJsonFileBackend CRUD operations
// =============================================================================

TEST_CLASS(ArcFileBackend_CRUD, "ArcPersistence.FileBackend.CRUD")
{
	FString TestDir;
	TUniquePtr<FArcJsonFileBackend> Backend;

	BEFORE_EACH()
	{
		TestDir = FPaths::ProjectSavedDir() / TEXT("ArcPersistenceFileTests")
			/ FGuid::NewGuid().ToString();
		Backend = MakeUnique<FArcJsonFileBackend>(TestDir);
	}

	AFTER_EACH()
	{
		Backend.Reset();
		IFileManager::Get().DeleteDirectory(*TestDir, false, true);
	}

	TEST_METHOD(SaveAndLoad_RoundTrip)
	{
		const FString Key = TEXT("test/simple");
		const TArray<uint8> SourceData = {1, 2, 3, 4, 5, 42, 255};

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, SourceData)));

		TArray<uint8> LoadedData;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, LoadedData)));
		ASSERT_THAT(AreEqual(SourceData.Num(), LoadedData.Num()));

		for (int32 i = 0; i < SourceData.Num(); ++i)
		{
			ASSERT_THAT(AreEqual(SourceData[i], LoadedData[i]));
		}
	}

	TEST_METHOD(SaveCreatesFile_OnDisk)
	{
		const FString Key = TEXT("disk/check");
		const TArray<uint8> Data = {10, 20};

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, Data)));

		// Verify the .json file actually exists on disk
		const FString ExpectedPath = TestDir / TEXT("disk/check.json");
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*ExpectedPath)));
	}

	TEST_METHOD(NestedKey_CreatesSubdirectories)
	{
		const FString Key = TEXT("world/saves/region_01/cell_0_0");
		const TArray<uint8> Data = {1};

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, Data)));

		const FString ExpectedPath = TestDir / TEXT("world/saves/region_01/cell_0_0.json");
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*ExpectedPath)));
	}

	TEST_METHOD(Overwrite_LatestDataWins)
	{
		const FString Key = TEXT("overwrite");
		const TArray<uint8> DataV1 = {1, 1, 1};
		const TArray<uint8> DataV2 = {2, 2, 2, 2, 2};

		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, DataV1)));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, DataV2)));

		TArray<uint8> Loaded;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(Key, Loaded)));
		ASSERT_THAT(AreEqual(5, Loaded.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>(2), Loaded[0]));
	}

	TEST_METHOD(LoadMissingKey_ReturnsFalse)
	{
		TArray<uint8> Loaded;
		ASSERT_THAT(IsFalse(Backend->LoadEntry(TEXT("nonexistent/key"), Loaded)));
	}

	TEST_METHOD(EntryExists_TrueForPresent)
	{
		const FString Key = TEXT("exists/yes");
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, {1})));
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key)));
	}

	TEST_METHOD(EntryExists_FalseForMissing)
	{
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("no/such/key"))));
	}

	TEST_METHOD(DeleteEntry_RemovesFile)
	{
		const FString Key = TEXT("to_delete");
		ASSERT_THAT(IsTrue(Backend->SaveEntry(Key, {1, 2, 3})));
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key)));

		ASSERT_THAT(IsTrue(Backend->DeleteEntry(Key)));
		ASSERT_THAT(IsFalse(Backend->EntryExists(Key)));

		TArray<uint8> Loaded;
		ASSERT_THAT(IsFalse(Backend->LoadEntry(Key, Loaded)));
	}

	TEST_METHOD(ListEntries_FindsAllUnderPrefix)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/cells/0_0"), {1})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/cells/1_0"), {2})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/cells/0_1"), {3})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("players/player_1"), {4})));

		TArray<FString> CellKeys = Backend->ListEntries(TEXT("world/cells"));
		ASSERT_THAT(AreEqual(3, CellKeys.Num()));

		TArray<FString> PlayerKeys = Backend->ListEntries(TEXT("players"));
		ASSERT_THAT(AreEqual(1, PlayerKeys.Num()));
	}

	TEST_METHOD(ListEntries_EmptyPrefix_FindsAll)
	{
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("a"), {1})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("b/c"), {2})));

		TArray<FString> AllKeys = Backend->ListEntries(TEXT(""));
		ASSERT_THAT(AreEqual(2, AllKeys.Num()));
	}

	TEST_METHOD(ListEntries_NoResults_ReturnsEmpty)
	{
		TArray<FString> Keys = Backend->ListEntries(TEXT("empty_prefix"));
		ASSERT_THAT(AreEqual(0, Keys.Num()));
	}
};

// =============================================================================
// TEST_CLASS 2: FArcJsonFileBackend transactions
// =============================================================================

TEST_CLASS(ArcFileBackend_Transactions, "ArcPersistence.FileBackend.Transactions")
{
	FString TestDir;
	TUniquePtr<FArcJsonFileBackend> Backend;

	BEFORE_EACH()
	{
		TestDir = FPaths::ProjectSavedDir() / TEXT("ArcPersistenceFileTests")
			/ FGuid::NewGuid().ToString();
		Backend = MakeUnique<FArcJsonFileBackend>(TestDir);
	}

	AFTER_EACH()
	{
		Backend.Reset();
		IFileManager::Get().DeleteDirectory(*TestDir, false, true);
	}

	TEST_METHOD(Commit_MakesFilesAppear)
	{
		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("tx/a"), {1, 2})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("tx/b"), {3, 4})));
		Backend->CommitTransaction();

		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("tx/a"))));
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("tx/b"))));

		TArray<uint8> DataA;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(TEXT("tx/a"), DataA)));
		ASSERT_THAT(AreEqual(2, DataA.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>(1), DataA[0]));

		TArray<uint8> DataB;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(TEXT("tx/b"), DataB)));
		ASSERT_THAT(AreEqual(2, DataB.Num()));
	}

	TEST_METHOD(Rollback_FilesDoNotAppear)
	{
		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("tx/roll_a"), {10})));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("tx/roll_b"), {20})));
		Backend->RollbackTransaction();

		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("tx/roll_a"))));
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("tx/roll_b"))));
	}

	TEST_METHOD(Rollback_CleansUpTempFiles)
	{
		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("tx/temp_check"), {99})));
		Backend->RollbackTransaction();

		// Verify no .tmp file left on disk
		const FString TmpPath = TestDir / TEXT("tx/temp_check.tmp");
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(*TmpPath)));
	}

	TEST_METHOD(MultipleTransactions_Independent)
	{
		// First transaction: commit
		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("batch1/a"), {1})));
		Backend->CommitTransaction();

		// Second transaction: rollback
		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("batch2/b"), {2})));
		Backend->RollbackTransaction();

		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("batch1/a"))));
		ASSERT_THAT(IsFalse(Backend->EntryExists(TEXT("batch2/b"))));
	}
};

// =============================================================================
// TEST_CLASS 3: Full struct → serialize → disk → load → deserialize round-trips
// =============================================================================

TEST_CLASS(ArcFileBackend_StructRoundTrip, "ArcPersistence.FileBackend.StructRoundTrip")
{
	FString TestDir;
	TUniquePtr<FArcJsonFileBackend> Backend;

	BEFORE_EACH()
	{
		TestDir = FPaths::ProjectSavedDir() / TEXT("ArcPersistenceFileTests")
			/ FGuid::NewGuid().ToString();
		Backend = MakeUnique<FArcJsonFileBackend>(TestDir);
	}

	AFTER_EACH()
	{
		Backend.Reset();
		IFileManager::Get().DeleteDirectory(*TestDir, false, true);
	}

	// Save a struct to disk via backend
	template<typename T>
	bool SaveStruct(const FString& Key, const T& Source)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(1);
		FArcReflectionSerializer::Save(T::StaticStruct(), &Source, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();
		return Backend->SaveEntry(Key, Data);
	}

	// Load a struct from disk via backend
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

	TEST_METHOD(SimpleStruct_RoundTripThroughDisk)
	{
		FArcPersistenceTestStruct Source;
		Source.Health = 250;
		Source.Name = TEXT("DiskWarrior");
		Source.Speed = 7.5f;
		Source.Id = FGuid::NewGuid();
		Source.Location = FVector(100.0, 200.0, 300.0);
		Source.Scores = {10, 20, 30, 40};
		Source.NotSaved = 999; // should NOT survive

		const FString Key = TEXT("structs/simple");
		ASSERT_THAT(IsTrue(SaveStruct(Key, Source)));
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key)));

		FArcPersistenceTestStruct Target;
		ASSERT_THAT(IsTrue(LoadStruct(Key, Target)));

		ASSERT_THAT(AreEqual(250, Target.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("DiskWarrior")), Target.Name));
		ASSERT_THAT(IsNear(7.5f, Target.Speed, 0.001f));
		ASSERT_THAT(AreEqual(Source.Id, Target.Id));
		ASSERT_THAT(IsNear(100.0, Target.Location.X, 0.001));
		ASSERT_THAT(IsNear(200.0, Target.Location.Y, 0.001));
		ASSERT_THAT(IsNear(300.0, Target.Location.Z, 0.001));
		ASSERT_THAT(AreEqual(4, Target.Scores.Num()));
		ASSERT_THAT(AreEqual(10, Target.Scores[0]));
		ASSERT_THAT(AreEqual(40, Target.Scores[3]));
		// Non-SaveGame field stays at default
		ASSERT_THAT(AreEqual(42, Target.NotSaved));
	}

	TEST_METHOD(NestedStruct_RoundTripThroughDisk)
	{
		FArcPersistenceTestNestedStruct Source;
		Source.Inner.Health = 42;
		Source.Inner.Name = TEXT("NestedDisk");
		Source.Inner.Speed = 3.14f;
		Source.Inner.Location = FVector(1.0, 2.0, 3.0);
		Source.Inner.Scores = {5, 10};
		Source.Multiplier = 2.5f;

		const FString Key = TEXT("structs/nested");
		ASSERT_THAT(IsTrue(SaveStruct(Key, Source)));

		FArcPersistenceTestNestedStruct Target;
		ASSERT_THAT(IsTrue(LoadStruct(Key, Target)));

		ASSERT_THAT(AreEqual(42, Target.Inner.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("NestedDisk")), Target.Inner.Name));
		ASSERT_THAT(IsNear(3.14f, Target.Inner.Speed, 0.001f));
		ASSERT_THAT(AreEqual(2, Target.Inner.Scores.Num()));
		ASSERT_THAT(IsNear(2.5f, Target.Multiplier, 0.001f));
	}

	TEST_METHOD(ComplexStruct_RoundTripThroughDisk)
	{
		FArcPersistenceTestComplexStruct Source;
		Source.SoftRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Script/CoreUObject.Object")));
		Source.SoftClass = TSoftClassPtr<UObject>(FSoftObjectPath(AActor::StaticClass()));
		Source.SubclassRef = APawn::StaticClass();
		Source.AssetId = FPrimaryAssetId(FPrimaryAssetType("Weapon"), FName("Sword_01"));

		const FString Key = TEXT("structs/complex");
		ASSERT_THAT(IsTrue(SaveStruct(Key, Source)));

		FArcPersistenceTestComplexStruct Target;
		ASSERT_THAT(IsTrue(LoadStruct(Key, Target)));

		ASSERT_THAT(AreEqual(Source.SoftRef.GetUniqueID(), Target.SoftRef.GetUniqueID()));
		ASSERT_THAT(AreEqual(Source.SoftClass.GetUniqueID(), Target.SoftClass.GetUniqueID()));
		ASSERT_THAT(AreEqual(APawn::StaticClass(), Target.SubclassRef.Get()));
		ASSERT_THAT(AreEqual(Source.AssetId, Target.AssetId));
	}

	TEST_METHOD(MapStruct_RoundTripThroughDisk)
	{
		FArcPersistenceTestMapStruct Source;
		Source.StringIntMap.Add(TEXT("Alpha"), 1);
		Source.StringIntMap.Add(TEXT("Beta"), 2);
		Source.NameStringMap.Add(FName("Key1"), TEXT("Value1"));

		const FString Key = TEXT("structs/map");
		ASSERT_THAT(IsTrue(SaveStruct(Key, Source)));

		FArcPersistenceTestMapStruct Target;
		ASSERT_THAT(IsTrue(LoadStruct(Key, Target)));

		ASSERT_THAT(AreEqual(2, Target.StringIntMap.Num()));
		ASSERT_THAT(AreEqual(1, Target.StringIntMap[TEXT("Alpha")]));
		ASSERT_THAT(AreEqual(2, Target.StringIntMap[TEXT("Beta")]));
		ASSERT_THAT(AreEqual(1, Target.NameStringMap.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Value1")), Target.NameStringMap[FName("Key1")]));
	}

	TEST_METHOD(SetStruct_RoundTripThroughDisk)
	{
		FArcPersistenceTestSetStruct Source;
		Source.NameSet.Add(FName("Alpha"));
		Source.NameSet.Add(FName("Beta"));
		Source.IntSet.Add(10);
		Source.IntSet.Add(20);
		Source.IntSet.Add(30);
		Source.StringSet.Add(TEXT("Hello"));

		const FString Key = TEXT("structs/set");
		ASSERT_THAT(IsTrue(SaveStruct(Key, Source)));

		FArcPersistenceTestSetStruct Target;
		ASSERT_THAT(IsTrue(LoadStruct(Key, Target)));

		ASSERT_THAT(AreEqual(2, Target.NameSet.Num()));
		ASSERT_THAT(IsTrue(Target.NameSet.Contains(FName("Alpha"))));
		ASSERT_THAT(IsTrue(Target.NameSet.Contains(FName("Beta"))));
		ASSERT_THAT(AreEqual(3, Target.IntSet.Num()));
		ASSERT_THAT(IsTrue(Target.IntSet.Contains(10)));
		ASSERT_THAT(IsTrue(Target.IntSet.Contains(20)));
		ASSERT_THAT(IsTrue(Target.IntSet.Contains(30)));
		ASSERT_THAT(AreEqual(1, Target.StringSet.Num()));
		ASSERT_THAT(IsTrue(Target.StringSet.Contains(TEXT("Hello"))));
	}

	TEST_METHOD(MultipleKeys_IndependentData)
	{
		FArcPersistenceTestStruct Source1;
		Source1.Health = 100;
		Source1.Name = TEXT("Entity1");

		FArcPersistenceTestStruct Source2;
		Source2.Health = 200;
		Source2.Name = TEXT("Entity2");

		ASSERT_THAT(IsTrue(SaveStruct(TEXT("multi/entity_1"), Source1)));
		ASSERT_THAT(IsTrue(SaveStruct(TEXT("multi/entity_2"), Source2)));

		FArcPersistenceTestStruct Target1;
		ASSERT_THAT(IsTrue(LoadStruct(TEXT("multi/entity_1"), Target1)));
		ASSERT_THAT(AreEqual(100, Target1.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Entity1")), Target1.Name));

		FArcPersistenceTestStruct Target2;
		ASSERT_THAT(IsTrue(LoadStruct(TEXT("multi/entity_2"), Target2)));
		ASSERT_THAT(AreEqual(200, Target2.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Entity2")), Target2.Name));
	}

	TEST_METHOD(SaveOverwrite_LoadReturnsLatest)
	{
		FArcPersistenceTestStruct Source;
		Source.Health = 100;
		Source.Name = TEXT("Version1");

		const FString Key = TEXT("overwrite/struct");
		ASSERT_THAT(IsTrue(SaveStruct(Key, Source)));

		// Overwrite with new data
		Source.Health = 999;
		Source.Name = TEXT("Version2");
		ASSERT_THAT(IsTrue(SaveStruct(Key, Source)));

		FArcPersistenceTestStruct Target;
		ASSERT_THAT(IsTrue(LoadStruct(Key, Target)));
		ASSERT_THAT(AreEqual(999, Target.Health));
		ASSERT_THAT(AreEqual(FString(TEXT("Version2")), Target.Name));
	}

	TEST_METHOD(DeletedEntry_LoadReturnsFalse)
	{
		FArcPersistenceTestStruct Source;
		Source.Health = 42;

		const FString Key = TEXT("delete/struct");
		ASSERT_THAT(IsTrue(SaveStruct(Key, Source)));
		ASSERT_THAT(IsTrue(Backend->DeleteEntry(Key)));

		FArcPersistenceTestStruct Target;
		ASSERT_THAT(IsFalse(LoadStruct(Key, Target)));
	}
};

// =============================================================================
// TEST_CLASS 4: Mass entity → serialize → disk → load → entity round-trips
// =============================================================================

TEST_CLASS(ArcFileBackend_EntityRoundTrip, "ArcPersistence.FileBackend.EntityRoundTrip")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FString TestDir;
	TUniquePtr<FArcJsonFileBackend> Backend;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();

		TestDir = FPaths::ProjectSavedDir() / TEXT("ArcPersistenceFileTests")
			/ FGuid::NewGuid().ToString();
		Backend = MakeUnique<FArcJsonFileBackend>(TestDir);
	}

	AFTER_EACH()
	{
		Backend.Reset();
		IFileManager::Get().DeleteDirectory(*TestDir, false, true);
	}

	// Save entity fragments to disk
	bool SaveEntityToDisk(const FString& Key, FMassEntityHandle Entity,
		const FArcMassPersistenceConfigFragment& Config = FArcMassPersistenceConfigFragment())
	{
		FArcJsonSaveArchive SaveAr;
		FArcMassFragmentSerializer::SaveEntityFragments(
			*EntityManager, Entity, Config, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();
		return Backend->SaveEntry(Key, Data);
	}

	// Load entity fragments from disk
	bool LoadEntityFromDisk(const FString& Key, FMassEntityHandle Entity)
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

		FArcMassFragmentSerializer::LoadEntityFragments(
			*EntityManager, Entity, LoadAr);
		return true;
	}

	TEST_METHOD(SingleFragment_ToDisk_RoundTrip)
	{
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = 500;
		HealthFrag.Armor = 75.5f;
		HealthFrag.TickCount = 999; // not SaveGame

		FInstancedStruct Instance = FInstancedStruct::Make(HealthFrag);
		FMassEntityHandle Source = EntityManager->CreateEntity(MakeArrayView(&Instance, 1));

		const FString Key = TEXT("entities/health_only");
		ASSERT_THAT(IsTrue(SaveEntityToDisk(Key, Source)));
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key)));

		// Create fresh target entity with same archetype
		FArcTestHealthFragment EmptyHealth;
		FInstancedStruct TargetInstance = FInstancedStruct::Make(EmptyHealth);
		FMassEntityHandle Target = EntityManager->CreateEntity(MakeArrayView(&TargetInstance, 1));

		ASSERT_THAT(IsTrue(LoadEntityFromDisk(Key, Target)));

		const auto& Result = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(500, Result.Health));
		ASSERT_THAT(IsNear(75.5f, Result.Armor, 0.001f));
		// Non-SaveGame stays at default
		ASSERT_THAT(AreEqual(0, Result.TickCount));
	}

	TEST_METHOD(MultipleFragments_ToDisk_RoundTrip)
	{
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = 300;
		HealthFrag.Armor = 40.f;

		FArcTestInventoryFragment InvFrag;
		InvFrag.Items = {FName("Sword"), FName("Shield"), FName("Potion")};
		InvFrag.Gold = 2500;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(HealthFrag));
		Instances.Add(FInstancedStruct::Make(InvFrag));
		FMassEntityHandle Source = EntityManager->CreateEntity(Instances);

		const FString Key = TEXT("entities/multi_frag");
		ASSERT_THAT(IsTrue(SaveEntityToDisk(Key, Source)));

		// Fresh target
		FArcTestHealthFragment EmptyHealth;
		FArcTestInventoryFragment EmptyInv;
		TArray<FInstancedStruct> TargetInstances;
		TargetInstances.Add(FInstancedStruct::Make(EmptyHealth));
		TargetInstances.Add(FInstancedStruct::Make(EmptyInv));
		FMassEntityHandle Target = EntityManager->CreateEntity(TargetInstances);

		ASSERT_THAT(IsTrue(LoadEntityFromDisk(Key, Target)));

		const auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(300, H.Health));
		ASSERT_THAT(IsNear(40.f, H.Armor, 0.001f));

		const auto& I = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(Target);
		ASSERT_THAT(AreEqual(2500, I.Gold));
		ASSERT_THAT(AreEqual(3, I.Items.Num()));
		ASSERT_THAT(AreEqual(FName("Sword"), I.Items[0]));
		ASSERT_THAT(AreEqual(FName("Shield"), I.Items[1]));
		ASSERT_THAT(AreEqual(FName("Potion"), I.Items[2]));
	}

	TEST_METHOD(ComplexFragment_ToDisk_RoundTrip)
	{
		FArcTestComplexFragment ComplexFrag;
		ComplexFrag.AssetRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Script/CoreUObject.Object")));
		ComplexFrag.ActorClass = APawn::StaticClass();
		ComplexFrag.PrimaryAsset = FPrimaryAssetId(FPrimaryAssetType("Weapon"), FName("Axe_03"));
		ComplexFrag.Stats.Add(FName("Strength"), 20);
		ComplexFrag.Stats.Add(FName("Dexterity"), 15);
		ComplexFrag.Tags.Add(FName("Legendary"));
		ComplexFrag.Tags.Add(FName("TwoHanded"));

		FInstancedStruct Instance = FInstancedStruct::Make(ComplexFrag);
		FMassEntityHandle Source = EntityManager->CreateEntity(MakeArrayView(&Instance, 1));

		const FString Key = TEXT("entities/complex_frag");
		ASSERT_THAT(IsTrue(SaveEntityToDisk(Key, Source)));

		FArcTestComplexFragment EmptyFrag;
		FInstancedStruct TargetInstance = FInstancedStruct::Make(EmptyFrag);
		FMassEntityHandle Target = EntityManager->CreateEntity(MakeArrayView(&TargetInstance, 1));

		ASSERT_THAT(IsTrue(LoadEntityFromDisk(Key, Target)));

		const auto& C = EntityManager->GetFragmentDataChecked<FArcTestComplexFragment>(Target);
		ASSERT_THAT(AreEqual(ComplexFrag.AssetRef.GetUniqueID(), C.AssetRef.GetUniqueID()));
		ASSERT_THAT(AreEqual(APawn::StaticClass(), C.ActorClass.Get()));
		ASSERT_THAT(AreEqual(ComplexFrag.PrimaryAsset, C.PrimaryAsset));
		ASSERT_THAT(AreEqual(2, C.Stats.Num()));
		ASSERT_THAT(AreEqual(20, C.Stats[FName("Strength")]));
		ASSERT_THAT(AreEqual(15, C.Stats[FName("Dexterity")]));
		ASSERT_THAT(AreEqual(2, C.Tags.Num()));
		ASSERT_THAT(IsTrue(C.Tags.Contains(FName("Legendary"))));
		ASSERT_THAT(IsTrue(C.Tags.Contains(FName("TwoHanded"))));
	}

	TEST_METHOD(MultipleEntities_SeparateFiles_RoundTrip)
	{
		// Entity A: warrior
		FArcTestHealthFragment HealthA;
		HealthA.Health = 500;
		HealthA.Armor = 80.f;
		FArcTestInventoryFragment InvA;
		InvA.Gold = 1000;
		InvA.Items = {FName("GreatSword")};

		TArray<FInstancedStruct> InstsA;
		InstsA.Add(FInstancedStruct::Make(HealthA));
		InstsA.Add(FInstancedStruct::Make(InvA));
		FMassEntityHandle SourceA = EntityManager->CreateEntity(InstsA);

		// Entity B: mage
		FArcTestHealthFragment HealthB;
		HealthB.Health = 200;
		HealthB.Armor = 10.f;
		FArcTestInventoryFragment InvB;
		InvB.Gold = 5000;
		InvB.Items = {FName("Staff"), FName("Tome")};

		TArray<FInstancedStruct> InstsB;
		InstsB.Add(FInstancedStruct::Make(HealthB));
		InstsB.Add(FInstancedStruct::Make(InvB));
		FMassEntityHandle SourceB = EntityManager->CreateEntity(InstsB);

		// Save both to separate keys
		ASSERT_THAT(IsTrue(SaveEntityToDisk(TEXT("entities/warrior"), SourceA)));
		ASSERT_THAT(IsTrue(SaveEntityToDisk(TEXT("entities/mage"), SourceB)));

		// Verify two distinct files on disk
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("entities/warrior"))));
		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("entities/mage"))));

		// Load warrior
		FArcTestHealthFragment EmptyH;
		FArcTestInventoryFragment EmptyI;
		TArray<FInstancedStruct> TargetInstsA;
		TargetInstsA.Add(FInstancedStruct::Make(EmptyH));
		TargetInstsA.Add(FInstancedStruct::Make(EmptyI));
		FMassEntityHandle TargetA = EntityManager->CreateEntity(TargetInstsA);

		ASSERT_THAT(IsTrue(LoadEntityFromDisk(TEXT("entities/warrior"), TargetA)));

		const auto& HA = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(TargetA);
		ASSERT_THAT(AreEqual(500, HA.Health));
		ASSERT_THAT(IsNear(80.f, HA.Armor, 0.001f));
		const auto& IA = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(TargetA);
		ASSERT_THAT(AreEqual(1000, IA.Gold));
		ASSERT_THAT(AreEqual(1, IA.Items.Num()));
		ASSERT_THAT(AreEqual(FName("GreatSword"), IA.Items[0]));

		// Load mage
		TArray<FInstancedStruct> TargetInstsB;
		TargetInstsB.Add(FInstancedStruct::Make(EmptyH));
		TargetInstsB.Add(FInstancedStruct::Make(EmptyI));
		FMassEntityHandle TargetB = EntityManager->CreateEntity(TargetInstsB);

		ASSERT_THAT(IsTrue(LoadEntityFromDisk(TEXT("entities/mage"), TargetB)));

		const auto& HB = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(TargetB);
		ASSERT_THAT(AreEqual(200, HB.Health));
		const auto& IB = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(TargetB);
		ASSERT_THAT(AreEqual(5000, IB.Gold));
		ASSERT_THAT(AreEqual(2, IB.Items.Num()));
		ASSERT_THAT(AreEqual(FName("Staff"), IB.Items[0]));
		ASSERT_THAT(AreEqual(FName("Tome"), IB.Items[1]));
	}

	TEST_METHOD(DestroySource_DiskDataSurvives)
	{
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = 777;
		HealthFrag.Armor = 55.f;

		FArcTestInventoryFragment InvFrag;
		InvFrag.Gold = 9999;
		InvFrag.Items = {FName("Crown")};

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(HealthFrag));
		Instances.Add(FInstancedStruct::Make(InvFrag));
		FMassEntityHandle Source = EntityManager->CreateEntity(Instances);

		const FString Key = TEXT("entities/doomed");
		ASSERT_THAT(IsTrue(SaveEntityToDisk(Key, Source)));

		// Destroy source — disk data should still be there
		EntityManager->DestroyEntity(Source);
		ASSERT_THAT(IsTrue(Backend->EntryExists(Key)));

		// Load into fresh entity
		FArcTestHealthFragment EmptyH;
		FArcTestInventoryFragment EmptyI;
		TArray<FInstancedStruct> TargetInsts;
		TargetInsts.Add(FInstancedStruct::Make(EmptyH));
		TargetInsts.Add(FInstancedStruct::Make(EmptyI));
		FMassEntityHandle Target = EntityManager->CreateEntity(TargetInsts);

		ASSERT_THAT(IsTrue(LoadEntityFromDisk(Key, Target)));

		const auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(777, H.Health));
		ASSERT_THAT(IsNear(55.f, H.Armor, 0.001f));

		const auto& I = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(Target);
		ASSERT_THAT(AreEqual(9999, I.Gold));
		ASSERT_THAT(AreEqual(1, I.Items.Num()));
		ASSERT_THAT(AreEqual(FName("Crown"), I.Items[0]));
	}

	TEST_METHOD(OverwriteEntity_LatestDataOnDisk)
	{
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = 100;
		HealthFrag.Armor = 10.f;

		FInstancedStruct Instance = FInstancedStruct::Make(HealthFrag);
		FMassEntityHandle Entity = EntityManager->CreateEntity(MakeArrayView(&Instance, 1));

		const FString Key = TEXT("entities/overwrite");
		ASSERT_THAT(IsTrue(SaveEntityToDisk(Key, Entity)));

		// Mutate and overwrite
		auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Entity);
		const_cast<FArcTestHealthFragment&>(H).Health = 9999;
		const_cast<FArcTestHealthFragment&>(H).Armor = 99.f;

		ASSERT_THAT(IsTrue(SaveEntityToDisk(Key, Entity)));

		// Load and verify latest
		FArcTestHealthFragment EmptyH;
		FInstancedStruct TargetInstance = FInstancedStruct::Make(EmptyH);
		FMassEntityHandle Target = EntityManager->CreateEntity(MakeArrayView(&TargetInstance, 1));

		ASSERT_THAT(IsTrue(LoadEntityFromDisk(Key, Target)));

		const auto& Result = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(9999, Result.Health));
		ASSERT_THAT(IsNear(99.f, Result.Armor, 0.001f));
	}

	TEST_METHOD(TransactionCommit_EntityDataOnDisk)
	{
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = 350;
		HealthFrag.Armor = 25.f;

		FArcTestInventoryFragment InvFrag;
		InvFrag.Gold = 4000;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(HealthFrag));
		Instances.Add(FInstancedStruct::Make(InvFrag));
		FMassEntityHandle Source = EntityManager->CreateEntity(Instances);

		// Serialize inside a transaction
		FArcJsonSaveArchive SaveAr;
		FArcMassFragmentSerializer::SaveEntityFragments(
			*EntityManager, Source, FArcMassPersistenceConfigFragment(), SaveAr);
		TArray<uint8> Blob = SaveAr.Finalize();

		Backend->BeginTransaction();
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("tx/entity"), Blob)));
		Backend->CommitTransaction();

		ASSERT_THAT(IsTrue(Backend->EntryExists(TEXT("tx/entity"))));

		// Load from committed transaction
		FArcTestHealthFragment EmptyH;
		FArcTestInventoryFragment EmptyI;
		TArray<FInstancedStruct> TargetInsts;
		TargetInsts.Add(FInstancedStruct::Make(EmptyH));
		TargetInsts.Add(FInstancedStruct::Make(EmptyI));
		FMassEntityHandle Target = EntityManager->CreateEntity(TargetInsts);

		ASSERT_THAT(IsTrue(LoadEntityFromDisk(TEXT("tx/entity"), Target)));

		const auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(350, H.Health));
		const auto& I = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(Target);
		ASSERT_THAT(AreEqual(4000, I.Gold));
	}

	TEST_METHOD(ListEntities_FindsSavedEntities)
	{
		FArcTestHealthFragment H1;
		H1.Health = 100;
		FInstancedStruct I1 = FInstancedStruct::Make(H1);
		FMassEntityHandle E1 = EntityManager->CreateEntity(MakeArrayView(&I1, 1));

		FArcTestHealthFragment H2;
		H2.Health = 200;
		FInstancedStruct I2 = FInstancedStruct::Make(H2);
		FMassEntityHandle E2 = EntityManager->CreateEntity(MakeArrayView(&I2, 1));

		ASSERT_THAT(IsTrue(SaveEntityToDisk(TEXT("world/entities/npc_001"), E1)));
		ASSERT_THAT(IsTrue(SaveEntityToDisk(TEXT("world/entities/npc_002"), E2)));
		ASSERT_THAT(IsTrue(SaveEntityToDisk(TEXT("players/player_01"), E1)));

		TArray<FString> EntityKeys = Backend->ListEntries(TEXT("world/entities"));
		ASSERT_THAT(AreEqual(2, EntityKeys.Num()));

		TArray<FString> PlayerKeys = Backend->ListEntries(TEXT("players"));
		ASSERT_THAT(AreEqual(1, PlayerKeys.Num()));
	}
};
