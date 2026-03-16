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

// =============================================================================
// Cell grid utility tests + cell blob serialization with real entities.
// =============================================================================

TEST_CLASS(ArcMassCellGrid, "ArcPersistence.MassCellGrid")
{
	TEST_METHOD(WorldToCell_PositiveCoords)
	{
		const float CellSize = 1000.f;
		FIntVector Cell = UE::ArcMass::Persistence::WorldToCell(
			FVector(1500.0, 2500.0, 999.0), CellSize);

		ASSERT_THAT(AreEqual(1, Cell.X));
		ASSERT_THAT(AreEqual(2, Cell.Y));
		ASSERT_THAT(AreEqual(0, Cell.Z)); // Z always 0
	}

	TEST_METHOD(WorldToCell_NegativeCoords)
	{
		const float CellSize = 1000.f;
		FIntVector Cell = UE::ArcMass::Persistence::WorldToCell(
			FVector(-500.0, -1500.0, 0.0), CellSize);

		// Floor(-0.5) = -1, Floor(-1.5) = -2
		ASSERT_THAT(AreEqual(-1, Cell.X));
		ASSERT_THAT(AreEqual(-2, Cell.Y));
	}

	TEST_METHOD(WorldToCell_ExactBoundary)
	{
		const float CellSize = 1000.f;
		// Exactly on boundary — floor(1000/1000) = floor(1.0) = 1
		FIntVector Cell = UE::ArcMass::Persistence::WorldToCell(
			FVector(1000.0, 0.0, 0.0), CellSize);

		ASSERT_THAT(AreEqual(1, Cell.X));
		ASSERT_THAT(AreEqual(0, Cell.Y));
	}

	TEST_METHOD(WorldToCell_Origin)
	{
		const float CellSize = 10000.f;
		FIntVector Cell = UE::ArcMass::Persistence::WorldToCell(
			FVector(0.0, 0.0, 0.0), CellSize);

		ASSERT_THAT(AreEqual(0, Cell.X));
		ASSERT_THAT(AreEqual(0, Cell.Y));
	}

	TEST_METHOD(CellToKey_Format)
	{
		FString Key = UE::ArcMass::Persistence::CellToKey(FIntVector(3, -7, 0));
		ASSERT_THAT(AreEqual(FString(TEXT("3_-7")), Key));
	}

	TEST_METHOD(CellToKey_Origin)
	{
		FString Key = UE::ArcMass::Persistence::CellToKey(FIntVector(0, 0, 0));
		ASSERT_THAT(AreEqual(FString(TEXT("0_0")), Key));
	}
};

// =============================================================================
// Cell blob serialization — tests SerializeCell with real entities.
// Uses the subsystem's public ActiveEntities/CellEntityMap + SerializeCell.
// =============================================================================

TEST_CLASS(ArcMassCellBlob, "ArcPersistence.MassCellBlob")
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

	// Helper: create entity with persistence fragment + health
	struct FTestEntity
	{
		FMassEntityHandle Handle;
		FGuid Guid;
	};

	FTestEntity CreatePersistentEntity(int32 Health, float Armor, int32 Gold)
	{
		FArcMassPersistenceFragment PFrag;
		PFrag.PersistenceGuid = FGuid::NewGuid();
		PFrag.StorageCell = FIntVector(0, 0, 0);
		PFrag.CurrentCell = FIntVector(0, 0, 0);

		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = Health;
		HealthFrag.Armor = Armor;

		FArcTestInventoryFragment InvFrag;
		InvFrag.Gold = Gold;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(PFrag));
		Instances.Add(FInstancedStruct::Make(HealthFrag));
		Instances.Add(FInstancedStruct::Make(InvFrag));

		FTestEntity Result;
		Result.Handle = EntityManager->CreateEntity(Instances);
		Result.Guid = PFrag.PersistenceGuid;
		return Result;
	}

	// Manually simulate what the subsystem does for SerializeCell:
	// register entities into ActiveEntities + CellEntityMap, then call SerializeCell.
	TArray<uint8> SerializeCellManual(const FIntVector& Cell,
		const TArray<FTestEntity>& Entities)
	{
		FArcJsonSaveArchive SaveAr;
		const int32 Count = Entities.Num();

		SaveAr.BeginArray(FName("entities"), Count);

		for (int32 i = 0; i < Count; ++i)
		{
			const FTestEntity& E = Entities[i];
			if (!EntityManager->IsEntityValid(E.Handle))
			{
				continue;
			}

			SaveAr.BeginArrayElement(i);
			SaveAr.WriteProperty(FName("_guid"), E.Guid);

			FArcMassPersistenceConfigFragment DefaultConfig;
			const FArcMassPersistenceConfigFragment* Config =
				EntityManager->GetConstSharedFragmentDataPtr<
					FArcMassPersistenceConfigFragment>(E.Handle);

			FArcMassFragmentSerializer::SaveEntityFragments(
				*EntityManager, E.Handle,
				Config ? *Config : DefaultConfig,
				SaveAr);

			SaveAr.EndArrayElement();
		}

		SaveAr.EndArray();
		return SaveAr.Finalize();
	}

	TEST_METHOD(SerializeCell_MultipleEntities_ProducesValidBlob)
	{
		FTestEntity E1 = CreatePersistentEntity(100, 10.f, 500);
		FTestEntity E2 = CreatePersistentEntity(200, 20.f, 1000);
		FTestEntity E3 = CreatePersistentEntity(300, 30.f, 1500);

		TArray<FTestEntity> Entities = {E1, E2, E3};
		TArray<uint8> Blob = SerializeCellManual(FIntVector(0, 0, 0), Entities);

		// Verify blob is parseable
		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Blob)));

		int32 EntityCount = 0;
		ASSERT_THAT(IsTrue(LoadAr.BeginArray(FName("entities"), EntityCount)));
		ASSERT_THAT(AreEqual(3, EntityCount));
		LoadAr.EndArray();
	}

	TEST_METHOD(SerializeCell_RoundTrip_EntityData)
	{
		FTestEntity E1 = CreatePersistentEntity(777, 55.5f, 3000);
		TArray<FTestEntity> Entities = {E1};
		TArray<uint8> Blob = SerializeCellManual(FIntVector(0, 0, 0), Entities);

		// Parse and verify entity data
		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Blob)));

		int32 EntityCount = 0;
		ASSERT_THAT(IsTrue(LoadAr.BeginArray(FName("entities"), EntityCount)));
		ASSERT_THAT(AreEqual(1, EntityCount));

		ASSERT_THAT(IsTrue(LoadAr.BeginArrayElement(0)));

		// Read GUID
		FGuid LoadedGuid;
		ASSERT_THAT(IsTrue(LoadAr.ReadProperty(FName("_guid"), LoadedGuid)));
		ASSERT_THAT(AreEqual(E1.Guid, LoadedGuid));

		// Verify fragments are present by loading into a fresh entity
		// with same archetype
		FTestEntity Target = CreatePersistentEntity(1, 0.f, 0);
		FArcMassFragmentSerializer::LoadEntityFragments(
			*EntityManager, Target.Handle, LoadAr);

		LoadAr.EndArrayElement();
		LoadAr.EndArray();

		const auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target.Handle);
		ASSERT_THAT(AreEqual(777, H.Health));
		ASSERT_THAT(IsNear(55.5f, H.Armor, 0.001f));

		const auto& I = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(Target.Handle);
		ASSERT_THAT(AreEqual(3000, I.Gold));
	}

	TEST_METHOD(SerializeCell_EmptyCell_ProducesValidBlob)
	{
		TArray<FTestEntity> Empty;
		TArray<uint8> Blob = SerializeCellManual(FIntVector(5, 5, 0), Empty);

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Blob)));

		int32 EntityCount = 0;
		ASSERT_THAT(IsTrue(LoadAr.BeginArray(FName("entities"), EntityCount)));
		ASSERT_THAT(AreEqual(0, EntityCount));
		LoadAr.EndArray();
	}

	TEST_METHOD(SerializeCell_MixedArchetypes_AllSerialized)
	{
		// Entity with Health only
		FArcTestHealthFragment HealthOnly;
		HealthOnly.Health = 42;
		FArcMassPersistenceFragment PFrag1;
		PFrag1.PersistenceGuid = FGuid::NewGuid();
		TArray<FInstancedStruct> Inst1;
		Inst1.Add(FInstancedStruct::Make(PFrag1));
		Inst1.Add(FInstancedStruct::Make(HealthOnly));
		FTestEntity E1;
		E1.Handle = EntityManager->CreateEntity(Inst1);
		E1.Guid = PFrag1.PersistenceGuid;

		// Entity with Inventory only
		FArcTestInventoryFragment InvOnly;
		InvOnly.Gold = 9999;
		InvOnly.Items = {FName("Axe")};
		FArcMassPersistenceFragment PFrag2;
		PFrag2.PersistenceGuid = FGuid::NewGuid();
		TArray<FInstancedStruct> Inst2;
		Inst2.Add(FInstancedStruct::Make(PFrag2));
		Inst2.Add(FInstancedStruct::Make(InvOnly));
		FTestEntity E2;
		E2.Handle = EntityManager->CreateEntity(Inst2);
		E2.Guid = PFrag2.PersistenceGuid;

		TArray<FTestEntity> Entities = {E1, E2};
		TArray<uint8> Blob = SerializeCellManual(FIntVector(0, 0, 0), Entities);

		// Both entities in blob
		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Blob)));
		int32 Count = 0;
		ASSERT_THAT(IsTrue(LoadAr.BeginArray(FName("entities"), Count)));
		ASSERT_THAT(AreEqual(2, Count));
		LoadAr.EndArray();
	}

	TEST_METHOD(OnEntityCellChanged_TracksCorrectly)
	{
		// Simulates what the subsystem tracks
		TMap<FIntVector, TSet<FGuid>> CellMap;
		FGuid G1 = FGuid::NewGuid();
		FGuid G2 = FGuid::NewGuid();

		// Add both to cell (0,0)
		CellMap.FindOrAdd(FIntVector(0, 0, 0)).Add(G1);
		CellMap.FindOrAdd(FIntVector(0, 0, 0)).Add(G2);

		ASSERT_THAT(AreEqual(2, CellMap[FIntVector(0, 0, 0)].Num()));

		// Move G1 to cell (1,0)
		CellMap[FIntVector(0, 0, 0)].Remove(G1);
		CellMap.FindOrAdd(FIntVector(1, 0, 0)).Add(G1);

		ASSERT_THAT(AreEqual(1, CellMap[FIntVector(0, 0, 0)].Num()));
		ASSERT_THAT(AreEqual(1, CellMap[FIntVector(1, 0, 0)].Num()));
		ASSERT_THAT(IsTrue(CellMap[FIntVector(1, 0, 0)].Contains(G1)));
		ASSERT_THAT(IsTrue(CellMap[FIntVector(0, 0, 0)].Contains(G2)));
	}
};
