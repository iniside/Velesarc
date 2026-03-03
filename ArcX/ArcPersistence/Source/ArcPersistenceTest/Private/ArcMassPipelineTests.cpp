// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcPersistenceTestTypes.h"
#include "ArcMass/Persistence/ArcMassEntityPersistenceSubsystem.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Components/ActorTestSpawner.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "Storage/ArcJsonFileBackend.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// =============================================================================
// Full pipeline tests: entity → cell blob → backend → load → verify.
// =============================================================================

TEST_CLASS(ArcMassPipeline, "ArcPersistence.MassPipeline")
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

		TestDir = FPaths::ProjectSavedDir() / TEXT("ArcMassPipelineTest")
			/ FGuid::NewGuid().ToString();
		Backend = MakeUnique<FArcJsonFileBackend>(TestDir);
	}

	AFTER_EACH()
	{
		Backend.Reset();
		IFileManager::Get().DeleteDirectory(*TestDir, false, true);
	}

	struct FTestEntity
	{
		FMassEntityHandle Handle;
		FGuid Guid;
	};

	FTestEntity CreateEntity(int32 Health, float Armor, int32 Gold,
		const TArray<FName>& Items = {})
	{
		FArcMassPersistenceFragment PFrag;
		PFrag.PersistenceGuid = FGuid::NewGuid();

		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = Health;
		HealthFrag.Armor = Armor;

		FArcTestInventoryFragment InvFrag;
		InvFrag.Gold = Gold;
		InvFrag.Items = Items;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(PFrag));
		Instances.Add(FInstancedStruct::Make(HealthFrag));
		Instances.Add(FInstancedStruct::Make(InvFrag));

		FTestEntity Result;
		Result.Handle = EntityManager->CreateEntity(Instances);
		Result.Guid = PFrag.PersistenceGuid;
		return Result;
	}

	// Serialize a set of entities into a cell blob (mirrors subsystem logic)
	TArray<uint8> SerializeEntities(const TArray<FTestEntity>& Entities)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.BeginArray(FName("entities"), Entities.Num());

		for (int32 i = 0; i < Entities.Num(); ++i)
		{
			const FTestEntity& E = Entities[i];
			if (!EntityManager->IsEntityValid(E.Handle))
			{
				continue;
			}

			SaveAr.BeginArrayElement(i);
			SaveAr.WriteProperty(FName("_guid"), E.Guid);

			FArcMassPersistenceConfigFragment DefaultConfig;
			FArcMassFragmentSerializer::SaveEntityFragments(
				*EntityManager, E.Handle, DefaultConfig, SaveAr);

			SaveAr.EndArrayElement();
		}

		SaveAr.EndArray();
		return SaveAr.Finalize();
	}

	// Load entities from blob and apply to pre-created target entities
	// Returns GUIDs found in the blob
	TArray<FGuid> DeserializeAndApply(const TArray<uint8>& Blob,
		const TMap<FGuid, FMassEntityHandle>& TargetsByGuid)
	{
		TArray<FGuid> FoundGuids;

		FArcJsonLoadArchive LoadAr;
		if (!LoadAr.InitializeFromData(Blob))
		{
			return FoundGuids;
		}

		int32 EntityCount = 0;
		if (!LoadAr.BeginArray(FName("entities"), EntityCount))
		{
			return FoundGuids;
		}

		for (int32 i = 0; i < EntityCount; ++i)
		{
			if (!LoadAr.BeginArrayElement(i))
			{
				continue;
			}

			FGuid Guid;
			if (LoadAr.ReadProperty(FName("_guid"), Guid))
			{
				FoundGuids.Add(Guid);

				if (const FMassEntityHandle* Target = TargetsByGuid.Find(Guid))
				{
					if (EntityManager->IsEntityValid(*Target))
					{
						FArcMassFragmentSerializer::LoadEntityFragments(
							*EntityManager, *Target, LoadAr);
					}
				}
			}

			LoadAr.EndArrayElement();
		}

		LoadAr.EndArray();
		return FoundGuids;
	}

	TEST_METHOD(SaveAndLoadCell_ThroughBackend_RoundTrip)
	{
		FTestEntity E1 = CreateEntity(100, 10.f, 500, {FName("Sword")});
		FTestEntity E2 = CreateEntity(200, 20.f, 1000, {FName("Shield"), FName("Bow")});

		// Serialize → backend
		TArray<uint8> Blob = SerializeEntities({E1, E2});
		const FString CellKey = TEXT("world/test/cells/0_0");
		ASSERT_THAT(IsTrue(Backend->SaveEntry(CellKey, Blob)));

		// Load from backend
		TArray<uint8> LoadedBlob;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(CellKey, LoadedBlob)));

		// Create fresh target entities with same archetype
		FTestEntity T1 = CreateEntity(0, 0.f, 0);
		FTestEntity T2 = CreateEntity(0, 0.f, 0);

		TMap<FGuid, FMassEntityHandle> Targets;
		Targets.Add(E1.Guid, T1.Handle);
		Targets.Add(E2.Guid, T2.Handle);

		TArray<FGuid> Guids = DeserializeAndApply(LoadedBlob, Targets);

		// Both entities found
		ASSERT_THAT(AreEqual(2, Guids.Num()));

		// Verify E1 data
		const auto& H1 = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(T1.Handle);
		ASSERT_THAT(AreEqual(100, H1.Health));
		ASSERT_THAT(IsNear(10.f, H1.Armor, 0.001f));
		const auto& I1 = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(T1.Handle);
		ASSERT_THAT(AreEqual(500, I1.Gold));
		ASSERT_THAT(AreEqual(1, I1.Items.Num()));
		ASSERT_THAT(AreEqual(FName("Sword"), I1.Items[0]));

		// Verify E2 data
		const auto& H2 = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(T2.Handle);
		ASSERT_THAT(AreEqual(200, H2.Health));
		const auto& I2 = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(T2.Handle);
		ASSERT_THAT(AreEqual(1000, I2.Gold));
		ASSERT_THAT(AreEqual(2, I2.Items.Num()));
		ASSERT_THAT(AreEqual(FName("Shield"), I2.Items[0]));
		ASSERT_THAT(AreEqual(FName("Bow"), I2.Items[1]));
	}

	TEST_METHOD(SaveCell_DestroyEntities_LoadCell_DataSurvives)
	{
		FTestEntity E1 = CreateEntity(999, 99.f, 5000);

		// Save to backend
		TArray<uint8> Blob = SerializeEntities({E1});
		const FString CellKey = TEXT("world/test/cells/1_1");
		ASSERT_THAT(IsTrue(Backend->SaveEntry(CellKey, Blob)));

		// Destroy the source entity
		EntityManager->DestroyEntity(E1.Handle);

		// Load from backend — data should be intact
		TArray<uint8> LoadedBlob;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(CellKey, LoadedBlob)));

		// Create new target entity
		FTestEntity Target = CreateEntity(0, 0.f, 0);
		TMap<FGuid, FMassEntityHandle> Targets;
		Targets.Add(E1.Guid, Target.Handle);

		TArray<FGuid> Guids = DeserializeAndApply(LoadedBlob, Targets);
		ASSERT_THAT(AreEqual(1, Guids.Num()));
		ASSERT_THAT(AreEqual(E1.Guid, Guids[0]));

		// Verify recovered data
		const auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target.Handle);
		ASSERT_THAT(AreEqual(999, H.Health));
		ASSERT_THAT(IsNear(99.f, H.Armor, 0.001f));

		const auto& I = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(Target.Handle);
		ASSERT_THAT(AreEqual(5000, I.Gold));
	}

	TEST_METHOD(MultipleCells_IndependentPersistence)
	{
		FTestEntity CellA_E1 = CreateEntity(100, 10.f, 100);
		FTestEntity CellB_E1 = CreateEntity(200, 20.f, 200);
		FTestEntity CellB_E2 = CreateEntity(300, 30.f, 300);

		// Save cell A and cell B separately
		TArray<uint8> BlobA = SerializeEntities({CellA_E1});
		TArray<uint8> BlobB = SerializeEntities({CellB_E1, CellB_E2});

		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/test/cells/0_0"), BlobA)));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/test/cells/1_0"), BlobB)));

		// Load only cell B
		TArray<uint8> LoadedBlobB;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(TEXT("world/test/cells/1_0"), LoadedBlobB)));

		FTestEntity TB1 = CreateEntity(0, 0.f, 0);
		FTestEntity TB2 = CreateEntity(0, 0.f, 0);

		TMap<FGuid, FMassEntityHandle> Targets;
		Targets.Add(CellB_E1.Guid, TB1.Handle);
		Targets.Add(CellB_E2.Guid, TB2.Handle);

		TArray<FGuid> Guids = DeserializeAndApply(LoadedBlobB, Targets);
		ASSERT_THAT(AreEqual(2, Guids.Num()));

		// Cell B entity 1
		const auto& H1 = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(TB1.Handle);
		ASSERT_THAT(AreEqual(200, H1.Health));
		const auto& I1 = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(TB1.Handle);
		ASSERT_THAT(AreEqual(200, I1.Gold));

		// Cell B entity 2
		const auto& H2 = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(TB2.Handle);
		ASSERT_THAT(AreEqual(300, H2.Health));
		const auto& I2 = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(TB2.Handle);
		ASSERT_THAT(AreEqual(300, I2.Gold));

		// Cell A entity should NOT appear in cell B data
		ASSERT_THAT(IsFalse(Guids.Contains(CellA_E1.Guid)));
	}

	TEST_METHOD(EmptyCell_SaveAndLoad_NoErrors)
	{
		TArray<uint8> Blob = SerializeEntities({});
		const FString CellKey = TEXT("world/test/cells/empty");
		ASSERT_THAT(IsTrue(Backend->SaveEntry(CellKey, Blob)));

		TArray<uint8> Loaded;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(CellKey, Loaded)));

		TMap<FGuid, FMassEntityHandle> Empty;
		TArray<FGuid> Guids = DeserializeAndApply(Loaded, Empty);
		ASSERT_THAT(AreEqual(0, Guids.Num()));
	}

	TEST_METHOD(OverwriteCell_LatestDataWins)
	{
		FTestEntity E1 = CreateEntity(100, 10.f, 500);
		const FString CellKey = TEXT("world/test/cells/overwrite");

		// Save v1
		TArray<uint8> Blob1 = SerializeEntities({E1});
		ASSERT_THAT(IsTrue(Backend->SaveEntry(CellKey, Blob1)));

		// Modify entity and save v2
		auto* H = EntityManager->GetFragmentDataPtr<FArcTestHealthFragment>(E1.Handle);
		H->Health = 9999;
		H->Armor = 88.f;
		auto* I = EntityManager->GetFragmentDataPtr<FArcTestInventoryFragment>(E1.Handle);
		I->Gold = 12345;

		TArray<uint8> Blob2 = SerializeEntities({E1});
		ASSERT_THAT(IsTrue(Backend->SaveEntry(CellKey, Blob2)));

		// Load — should get v2 data
		TArray<uint8> Loaded;
		ASSERT_THAT(IsTrue(Backend->LoadEntry(CellKey, Loaded)));

		FTestEntity Target = CreateEntity(0, 0.f, 0);
		TMap<FGuid, FMassEntityHandle> Targets;
		Targets.Add(E1.Guid, Target.Handle);

		DeserializeAndApply(Loaded, Targets);

		const auto& HResult = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target.Handle);
		ASSERT_THAT(AreEqual(9999, HResult.Health));
		ASSERT_THAT(IsNear(88.f, HResult.Armor, 0.001f));

		const auto& IResult = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(Target.Handle);
		ASSERT_THAT(AreEqual(12345, IResult.Gold));
	}
};
