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
#include "ArcPersistenceSubsystem.h"
#include "TestGameInstance.h"
#include "Storage/ArcPersistenceBackend.h"
#include "Storage/ArcJsonFileBackend.h"
#include "Storage/ArcPersistenceResult.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// =============================================================================
// Full pipeline tests: entity -> cell blob -> backend -> load -> verify.
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

		// Serialize -> backend
		TArray<uint8> Blob = SerializeEntities({E1, E2});
		const FString CellKey = TEXT("world/test/cells/0_0");
		ASSERT_THAT(IsTrue(Backend->SaveEntry(CellKey, MoveTemp(Blob)).Get().bSuccess));

		// Load from backend
		FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(CellKey).Get();
		ASSERT_THAT(IsTrue(LoadResult.bSuccess));

		// Create fresh target entities with same archetype
		FTestEntity T1 = CreateEntity(0, 0.f, 0);
		FTestEntity T2 = CreateEntity(0, 0.f, 0);

		TMap<FGuid, FMassEntityHandle> Targets;
		Targets.Add(E1.Guid, T1.Handle);
		Targets.Add(E2.Guid, T2.Handle);

		TArray<FGuid> Guids = DeserializeAndApply(LoadResult.Data, Targets);

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
		ASSERT_THAT(IsTrue(Backend->SaveEntry(CellKey, MoveTemp(Blob)).Get().bSuccess));

		// Destroy the source entity
		EntityManager->DestroyEntity(E1.Handle);

		// Load from backend — data should be intact
		FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(CellKey).Get();
		ASSERT_THAT(IsTrue(LoadResult.bSuccess));

		// Create new target entity
		FTestEntity Target = CreateEntity(0, 0.f, 0);
		TMap<FGuid, FMassEntityHandle> Targets;
		Targets.Add(E1.Guid, Target.Handle);

		TArray<FGuid> Guids = DeserializeAndApply(LoadResult.Data, Targets);
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

		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/test/cells/0_0"), MoveTemp(BlobA)).Get().bSuccess));
		ASSERT_THAT(IsTrue(Backend->SaveEntry(TEXT("world/test/cells/1_0"), MoveTemp(BlobB)).Get().bSuccess));

		// Load only cell B
		FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(TEXT("world/test/cells/1_0")).Get();
		ASSERT_THAT(IsTrue(LoadResult.bSuccess));

		FTestEntity TB1 = CreateEntity(0, 0.f, 0);
		FTestEntity TB2 = CreateEntity(0, 0.f, 0);

		TMap<FGuid, FMassEntityHandle> Targets;
		Targets.Add(CellB_E1.Guid, TB1.Handle);
		Targets.Add(CellB_E2.Guid, TB2.Handle);

		TArray<FGuid> Guids = DeserializeAndApply(LoadResult.Data, Targets);
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
		ASSERT_THAT(IsTrue(Backend->SaveEntry(CellKey, MoveTemp(Blob)).Get().bSuccess));

		FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(CellKey).Get();
		ASSERT_THAT(IsTrue(LoadResult.bSuccess));

		TMap<FGuid, FMassEntityHandle> Empty;
		TArray<FGuid> Guids = DeserializeAndApply(LoadResult.Data, Empty);
		ASSERT_THAT(AreEqual(0, Guids.Num()));
	}

	TEST_METHOD(OverwriteCell_LatestDataWins)
	{
		FTestEntity E1 = CreateEntity(100, 10.f, 500);
		const FString CellKey = TEXT("world/test/cells/overwrite");

		// Save v1
		TArray<uint8> Blob1 = SerializeEntities({E1});
		ASSERT_THAT(IsTrue(Backend->SaveEntry(CellKey, MoveTemp(Blob1)).Get().bSuccess));

		// Modify entity and save v2
		auto* H = EntityManager->GetFragmentDataPtr<FArcTestHealthFragment>(E1.Handle);
		H->Health = 9999;
		H->Armor = 88.f;
		auto* I = EntityManager->GetFragmentDataPtr<FArcTestInventoryFragment>(E1.Handle);
		I->Gold = 12345;

		TArray<uint8> Blob2 = SerializeEntities({E1});
		ASSERT_THAT(IsTrue(Backend->SaveEntry(CellKey, MoveTemp(Blob2)).Get().bSuccess));

		// Load — should get v2 data
		FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(CellKey).Get();
		ASSERT_THAT(IsTrue(LoadResult.bSuccess));

		FTestEntity Target = CreateEntity(0, 0.f, 0);
		TMap<FGuid, FMassEntityHandle> Targets;
		Targets.Add(E1.Guid, Target.Handle);

		DeserializeAndApply(LoadResult.Data, Targets);

		const auto& HResult = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target.Handle);
		ASSERT_THAT(AreEqual(9999, HResult.Health));
		ASSERT_THAT(IsNear(88.f, HResult.Armor, 0.001f));

		const auto& IResult = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(Target.Handle);
		ASSERT_THAT(AreEqual(12345, IResult.Gold));
	}
};

// =============================================================================
// Entity spawning tests via UArcMassEntityPersistenceSubsystem.
// Tests both sync (LoadCellFromData) and async (LoadCell/SaveCell/UnloadCell)
// paths through the full subsystem with backend integration.
// =============================================================================

TEST_CLASS(ArcMassEntitySpawning, "ArcPersistence.MassEntitySpawning")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UArcMassEntityPersistenceSubsystem* MassPersistSub = nullptr;
	IArcPersistenceBackend* Backend = nullptr;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES =
			Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();

		MassPersistSub =
			Spawner.GetWorld().GetSubsystem<UArcMassEntityPersistenceSubsystem>();
		check(MassPersistSub);
		MassPersistSub->Configure(10000.f, 50000.f, FGuid::NewGuid());

		UArcPersistenceSubsystem* PersistSub =
			Spawner.GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>();
		check(PersistSub);
		Backend = PersistSub->GetBackend();
		check(Backend);
	}

	struct FTestEntity
	{
		FMassEntityHandle Handle;
		FGuid Guid;
	};

	FTestEntity CreateSourceEntity(int32 Health, float Armor, int32 Gold,
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

	// Populates the backend at the cell's storage key so LoadCell can find it.
	void SaveBlobToBackend(const FIntVector& Cell, TArray<uint8> Blob)
	{
		const FString Key = MassPersistSub->MakeCellStorageKey(Cell);
		Backend->SaveEntry(Key, MoveTemp(Blob)).Get();
	}

	// ── Sync tests via LoadCellFromData ─────────────────────────────────

	TEST_METHOD(LoadCellFromData_SingleEntity_SpawnsWithCorrectData)
	{
		FTestEntity E1 = CreateSourceEntity(100, 10.f, 500, {FName("Sword")});
		TArray<uint8> Blob = SerializeEntities({E1});
		EntityManager->DestroyEntity(E1.Handle);

		const FIntVector Cell(0, 0, 0);
		MassPersistSub->LoadCellFromData(Cell, Blob);

		ASSERT_THAT(IsTrue(MassPersistSub->ActiveEntities.Contains(E1.Guid)));
		FMassEntityHandle Spawned = MassPersistSub->ActiveEntities[E1.Guid];
		ASSERT_THAT(IsTrue(EntityManager->IsEntityValid(Spawned)));

		const auto& H = EntityManager->GetFragmentDataChecked<
			FArcTestHealthFragment>(Spawned);
		ASSERT_THAT(AreEqual(100, H.Health));
		ASSERT_THAT(IsNear(10.f, H.Armor, 0.001f));

		const auto& Inv = EntityManager->GetFragmentDataChecked<
			FArcTestInventoryFragment>(Spawned);
		ASSERT_THAT(AreEqual(500, Inv.Gold));
		ASSERT_THAT(AreEqual(1, Inv.Items.Num()));
		ASSERT_THAT(AreEqual(FName("Sword"), Inv.Items[0]));
	}

	TEST_METHOD(LoadCellFromData_PersistenceFragmentTracking)
	{
		FTestEntity E1 = CreateSourceEntity(42, 5.f, 100);
		TArray<uint8> Blob = SerializeEntities({E1});
		EntityManager->DestroyEntity(E1.Handle);

		const FIntVector Cell(3, 7, 0);
		MassPersistSub->LoadCellFromData(Cell, Blob);

		FMassEntityHandle Spawned = MassPersistSub->ActiveEntities[E1.Guid];
		const auto& PFrag = EntityManager->GetFragmentDataChecked<
			FArcMassPersistenceFragment>(Spawned);
		ASSERT_THAT(AreEqual(E1.Guid, PFrag.PersistenceGuid));
		ASSERT_THAT(AreEqual(Cell, PFrag.StorageCell));
		ASSERT_THAT(AreEqual(Cell, PFrag.CurrentCell));
	}

	TEST_METHOD(LoadCellFromData_MultipleEntities_BatchSpawn)
	{
		FTestEntity E1 = CreateSourceEntity(100, 10.f, 500, {FName("Sword")});
		FTestEntity E2 = CreateSourceEntity(200, 20.f, 1000,
			{FName("Shield"), FName("Bow")});
		FTestEntity E3 = CreateSourceEntity(300, 30.f, 1500);

		TArray<uint8> Blob = SerializeEntities({E1, E2, E3});
		EntityManager->DestroyEntity(E1.Handle);
		EntityManager->DestroyEntity(E2.Handle);
		EntityManager->DestroyEntity(E3.Handle);

		MassPersistSub->LoadCellFromData(FIntVector(1, 2, 0), Blob);

		ASSERT_THAT(AreEqual(3, MassPersistSub->ActiveEntities.Num()));

		const auto& H1 = EntityManager->GetFragmentDataChecked<
			FArcTestHealthFragment>(MassPersistSub->ActiveEntities[E1.Guid]);
		ASSERT_THAT(AreEqual(100, H1.Health));

		const auto& H2 = EntityManager->GetFragmentDataChecked<
			FArcTestHealthFragment>(MassPersistSub->ActiveEntities[E2.Guid]);
		ASSERT_THAT(AreEqual(200, H2.Health));

		const auto& Inv2 = EntityManager->GetFragmentDataChecked<
			FArcTestInventoryFragment>(MassPersistSub->ActiveEntities[E2.Guid]);
		ASSERT_THAT(AreEqual(2, Inv2.Items.Num()));
		ASSERT_THAT(AreEqual(FName("Shield"), Inv2.Items[0]));
		ASSERT_THAT(AreEqual(FName("Bow"), Inv2.Items[1]));
	}

	TEST_METHOD(LoadCellFromData_DuplicateGuid_SkipsAlreadyActive)
	{
		FTestEntity E1 = CreateSourceEntity(100, 10.f, 500);
		TArray<uint8> Blob = SerializeEntities({E1});

		MassPersistSub->ActiveEntities.Add(E1.Guid, E1.Handle);

		MassPersistSub->LoadCellFromData(FIntVector(0, 0, 0), Blob);

		ASSERT_THAT(AreEqual(1, MassPersistSub->ActiveEntities.Num()));
	}

	TEST_METHOD(LoadCellFromData_NonSaveGameProperties_NotRestored)
	{
		FTestEntity E1 = CreateSourceEntity(100, 10.f, 500);
		EntityManager->GetFragmentDataPtr<FArcTestHealthFragment>(
			E1.Handle)->TickCount = 9999;

		TArray<uint8> Blob = SerializeEntities({E1});
		EntityManager->DestroyEntity(E1.Handle);

		MassPersistSub->LoadCellFromData(FIntVector(0, 0, 0), Blob);

		const auto& H = EntityManager->GetFragmentDataChecked<
			FArcTestHealthFragment>(MassPersistSub->ActiveEntities[E1.Guid]);
		ASSERT_THAT(AreEqual(100, H.Health));
		ASSERT_THAT(AreEqual(0, H.TickCount));
	}

	// ── Async tests via LoadCell (backend → background parse → spawn) ───

	TEST_METHOD(LoadCell_AsyncPath_SpawnsEntities)
	{
		FTestEntity E1 = CreateSourceEntity(100, 10.f, 500, {FName("Sword")});
		TArray<uint8> Blob = SerializeEntities({E1});
		EntityManager->DestroyEntity(E1.Handle);

		const FIntVector Cell(2, 3, 0);
		SaveBlobToBackend(Cell, MoveTemp(Blob));

		const FGuid E1Guid = E1.Guid;

		TestCommandBuilder
			.Do([this, Cell]()
			{
				MassPersistSub->LoadCell(Cell);
				ASSERT_THAT(IsTrue(
					MassPersistSub->PendingCellLoads.Contains(Cell)));
			})
			.Until([this, Cell]()
			{
				// Flush deferred commands so entities spawn in test env
				EntityManager->FlushCommands();
				return MassPersistSub->IsCellLoaded(Cell);
			})
			.Then([this, E1Guid, Cell]()
			{
				ASSERT_THAT(IsTrue(
					MassPersistSub->ActiveEntities.Contains(E1Guid)));

				FMassEntityHandle Spawned =
					MassPersistSub->ActiveEntities[E1Guid];
				ASSERT_THAT(IsTrue(EntityManager->IsEntityValid(Spawned)));

				const auto& H = EntityManager->GetFragmentDataChecked<
					FArcTestHealthFragment>(Spawned);
				ASSERT_THAT(AreEqual(100, H.Health));
				ASSERT_THAT(IsNear(10.f, H.Armor, 0.001f));

				const auto& Inv = EntityManager->GetFragmentDataChecked<
					FArcTestInventoryFragment>(Spawned);
				ASSERT_THAT(AreEqual(500, Inv.Gold));
				ASSERT_THAT(AreEqual(1, Inv.Items.Num()));

				ASSERT_THAT(IsFalse(
					MassPersistSub->PendingCellLoads.Contains(Cell)));
			});
	}

	TEST_METHOD(LoadCell_MultipleEntities_AsyncBatchSpawn)
	{
		FTestEntity E1 = CreateSourceEntity(100, 10.f, 500);
		FTestEntity E2 = CreateSourceEntity(200, 20.f, 1000,
			{FName("Shield"), FName("Bow")});

		TArray<uint8> Blob = SerializeEntities({E1, E2});
		EntityManager->DestroyEntity(E1.Handle);
		EntityManager->DestroyEntity(E2.Handle);

		const FIntVector Cell(0, 0, 0);
		SaveBlobToBackend(Cell, MoveTemp(Blob));

		const FGuid Guid1 = E1.Guid;
		const FGuid Guid2 = E2.Guid;

		TestCommandBuilder
			.Do([this, Cell]()
			{
				MassPersistSub->LoadCell(Cell);
			})
			.Until([this, Cell]()
			{
				EntityManager->FlushCommands();
				return MassPersistSub->IsCellLoaded(Cell);
			})
			.Then([this, Guid1, Guid2, Cell]()
			{
				ASSERT_THAT(IsTrue(
					MassPersistSub->ActiveEntities.Contains(Guid1)));
				ASSERT_THAT(IsTrue(
					MassPersistSub->ActiveEntities.Contains(Guid2)));

				const auto& H1 = EntityManager->GetFragmentDataChecked<
					FArcTestHealthFragment>(
					MassPersistSub->ActiveEntities[Guid1]);
				ASSERT_THAT(AreEqual(100, H1.Health));

				const auto& Inv2 = EntityManager->GetFragmentDataChecked<
					FArcTestInventoryFragment>(
					MassPersistSub->ActiveEntities[Guid2]);
				ASSERT_THAT(AreEqual(1000, Inv2.Gold));
				ASSERT_THAT(AreEqual(2, Inv2.Items.Num()));

				const TSet<FGuid>* CellGuids =
					MassPersistSub->CellEntityMap.Find(Cell);
				ASSERT_THAT(IsTrue(CellGuids != nullptr));
				ASSERT_THAT(AreEqual(2, CellGuids->Num()));
			});
	}

	TEST_METHOD(LoadCell_DoubleLoad_PreventsSecondRequest)
	{
		FTestEntity E1 = CreateSourceEntity(100, 10.f, 500);
		TArray<uint8> Blob = SerializeEntities({E1});
		EntityManager->DestroyEntity(E1.Handle);

		const FIntVector Cell(0, 0, 0);
		SaveBlobToBackend(Cell, MoveTemp(Blob));

		const FGuid Guid1 = E1.Guid;

		TestCommandBuilder
			.Do([this, Cell]()
			{
				MassPersistSub->LoadCell(Cell);
				// Second call should be a no-op (PendingCellLoads guard)
				MassPersistSub->LoadCell(Cell);
			})
			.Until([this, Cell]()
			{
				EntityManager->FlushCommands();
				return MassPersistSub->IsCellLoaded(Cell);
			})
			.Then([this, Guid1]()
			{
				// Should only have one entity, not a duplicate
				ASSERT_THAT(AreEqual(1,
					MassPersistSub->ActiveEntities.Num()));
				ASSERT_THAT(IsTrue(
					MassPersistSub->ActiveEntities.Contains(Guid1)));
			});
	}

	TEST_METHOD(SaveCell_ThenLoadBack_RoundTrips)
	{
		// Manually populate the subsystem with a live entity in a cell
		FTestEntity E1 = CreateSourceEntity(777, 77.f, 7777,
			{FName("Axe")});
		const FIntVector Cell(4, 5, 0);

		MassPersistSub->ActiveEntities.Add(E1.Guid, E1.Handle);
		MassPersistSub->CellEntityMap.FindOrAdd(Cell).Add(E1.Guid);
		MassPersistSub->LoadedCells.Add(Cell);

		// SaveCell serializes to backend
		MassPersistSub->SaveCell(Cell);
		Backend->Flush();

		// Destroy the entity and clear subsystem state
		EntityManager->DestroyEntity(E1.Handle);
		MassPersistSub->ActiveEntities.Empty();
		MassPersistSub->CellEntityMap.Empty();
		MassPersistSub->LoadedCells.Empty();

		// Load from backend via async path
		const FGuid Guid1 = E1.Guid;

		TestCommandBuilder
			.Do([this, Cell]()
			{
				MassPersistSub->LoadCell(Cell);
			})
			.Until([this, Cell]()
			{
				EntityManager->FlushCommands();
				return MassPersistSub->IsCellLoaded(Cell);
			})
			.Then([this, Guid1]()
			{
				ASSERT_THAT(IsTrue(
					MassPersistSub->ActiveEntities.Contains(Guid1)));

				FMassEntityHandle Spawned =
					MassPersistSub->ActiveEntities[Guid1];
				const auto& H = EntityManager->GetFragmentDataChecked<
					FArcTestHealthFragment>(Spawned);
				ASSERT_THAT(AreEqual(777, H.Health));
				ASSERT_THAT(IsNear(77.f, H.Armor, 0.001f));

				const auto& Inv = EntityManager->GetFragmentDataChecked<
					FArcTestInventoryFragment>(Spawned);
				ASSERT_THAT(AreEqual(7777, Inv.Gold));
				ASSERT_THAT(AreEqual(1, Inv.Items.Num()));
				ASSERT_THAT(AreEqual(FName("Axe"), Inv.Items[0]));
			});
	}

	TEST_METHOD(UnloadCell_DestroysEntitiesAndClearsState)
	{
		FTestEntity E1 = CreateSourceEntity(100, 10.f, 500);
		TArray<uint8> Blob = SerializeEntities({E1});
		EntityManager->DestroyEntity(E1.Handle);

		const FIntVector Cell(0, 0, 0);
		SaveBlobToBackend(Cell, MoveTemp(Blob));

		const FGuid Guid1 = E1.Guid;

		TestCommandBuilder
			.Do([this, Cell]()
			{
				MassPersistSub->LoadCell(Cell);
			})
			.Until([this, Cell]()
			{
				EntityManager->FlushCommands();
				return MassPersistSub->IsCellLoaded(Cell);
			})
			.Then([this, Guid1, Cell]()
			{
				// Cell is loaded, entity exists
				ASSERT_THAT(IsTrue(
					MassPersistSub->ActiveEntities.Contains(Guid1)));

				FMassEntityHandle SpawnedBefore =
					MassPersistSub->ActiveEntities[Guid1];

				// Unload the cell
				MassPersistSub->UnloadCell(Cell);

				ASSERT_THAT(IsFalse(MassPersistSub->IsCellLoaded(Cell)));
				ASSERT_THAT(IsFalse(
					MassPersistSub->ActiveEntities.Contains(Guid1)));
				ASSERT_THAT(IsFalse(
					EntityManager->IsEntityValid(SpawnedBefore)));
				ASSERT_THAT(IsTrue(
					MassPersistSub->CellEntityMap.Find(Cell) == nullptr));
			});
	}

	TEST_METHOD(LoadCell_EmptyCell_MarksLoaded)
	{
		const FIntVector Cell(9, 9, 0);
		TArray<uint8> EmptyBlob = SerializeEntities({});
		SaveBlobToBackend(Cell, MoveTemp(EmptyBlob));

		TestCommandBuilder
			.Do([this, Cell]()
			{
				MassPersistSub->LoadCell(Cell);
			})
			.Until([this, Cell]()
			{
				EntityManager->FlushCommands();
				return MassPersistSub->IsCellLoaded(Cell);
			})
			.Then([this, Cell]()
			{
				ASSERT_THAT(AreEqual(0,
					MassPersistSub->ActiveEntities.Num()));
				ASSERT_THAT(IsTrue(MassPersistSub->IsCellLoaded(Cell)));
			});
	}
};
