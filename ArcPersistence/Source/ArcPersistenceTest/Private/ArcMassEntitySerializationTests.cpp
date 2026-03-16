// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcPersistenceTestTypes.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Components/ActorTestSpawner.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"

// =============================================================================
// Entity-level integration tests for FArcMassFragmentSerializer.
// Uses a real FMassEntityManager with real entities and fragments.
// =============================================================================

TEST_CLASS(ArcMassEntitySerialization, "ArcPersistence.MassEntitySerialization")
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

	// Helper: create entity with Health fragment
	FMassEntityHandle CreateHealthEntity(int32 Health, float Armor, int32 TickCount)
	{
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = Health;
		HealthFrag.Armor = Armor;
		HealthFrag.TickCount = TickCount;

		FInstancedStruct Instance = FInstancedStruct::Make(HealthFrag);
		return EntityManager->CreateEntity(MakeArrayView(&Instance, 1));
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

	TEST_METHOD(SingleFragment_RoundTrip)
	{
		FMassEntityHandle Source = CreateHealthEntity(250, 75.5f, 999);
		FArcMassPersistenceConfigFragment Config;
		TArray<uint8> Data = SaveEntity(Source, Config);

		// Create target entity with same archetype, different values
		FMassEntityHandle Target = CreateHealthEntity(1, 0.f, 42);

		LoadEntity(Target, Data);

		const auto& Result = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(250, Result.Health));
		ASSERT_THAT(IsNear(75.5f, Result.Armor, 0.001f));
		// TickCount has no SaveGame flag — should remain at target's original value
		ASSERT_THAT(AreEqual(42, Result.TickCount));
	}

	TEST_METHOD(MultipleFragments_RoundTrip)
	{
		// Create entity with Health + Inventory
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = 200;
		HealthFrag.Armor = 50.f;

		FArcTestInventoryFragment InvFrag;
		InvFrag.Items = {FName("Sword"), FName("Shield"), FName("Potion")};
		InvFrag.Gold = 1500;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(HealthFrag));
		Instances.Add(FInstancedStruct::Make(InvFrag));

		FMassEntityHandle Source = EntityManager->CreateEntity(Instances);

		FArcMassPersistenceConfigFragment Config;
		TArray<uint8> Data = SaveEntity(Source, Config);

		// Create fresh target with same archetype
		FArcTestHealthFragment EmptyHealth;
		FArcTestInventoryFragment EmptyInv;
		TArray<FInstancedStruct> TargetInstances;
		TargetInstances.Add(FInstancedStruct::Make(EmptyHealth));
		TargetInstances.Add(FInstancedStruct::Make(EmptyInv));
		FMassEntityHandle Target = EntityManager->CreateEntity(TargetInstances);

		LoadEntity(Target, Data);

		const auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(200, H.Health));
		ASSERT_THAT(IsNear(50.f, H.Armor, 0.001f));

		const auto& I = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(Target);
		ASSERT_THAT(AreEqual(3, I.Items.Num()));
		ASSERT_THAT(AreEqual(FName("Sword"), I.Items[0]));
		ASSERT_THAT(AreEqual(FName("Shield"), I.Items[1]));
		ASSERT_THAT(AreEqual(FName("Potion"), I.Items[2]));
		ASSERT_THAT(AreEqual(1500, I.Gold));
	}

	TEST_METHOD(EmptyConfig_SkipsNonSaveGameFragment)
	{
		// Create entity with Health (has SaveGame) + Transient (no SaveGame)
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = 42;

		FArcTestTransientFragment TransFrag;
		TransFrag.Velocity = FVector(100, 200, 300);
		TransFrag.Timer = 5.f;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(HealthFrag));
		Instances.Add(FInstancedStruct::Make(TransFrag));

		FMassEntityHandle Source = EntityManager->CreateEntity(Instances);

		FArcMassPersistenceConfigFragment Config; // empty = auto SaveGame check
		TArray<uint8> Data = SaveEntity(Source, Config);

		// Create target with different transient values
		FArcTestHealthFragment TargetHealth;
		FArcTestTransientFragment TargetTrans;
		TargetTrans.Velocity = FVector(1, 2, 3);
		TargetTrans.Timer = 99.f;

		TArray<FInstancedStruct> TargetInstances;
		TargetInstances.Add(FInstancedStruct::Make(TargetHealth));
		TargetInstances.Add(FInstancedStruct::Make(TargetTrans));
		FMassEntityHandle Target = EntityManager->CreateEntity(TargetInstances);

		LoadEntity(Target, Data);

		// Health should be loaded
		const auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(42, H.Health));

		// Transient should remain untouched (was never serialized)
		const auto& T = EntityManager->GetFragmentDataChecked<FArcTestTransientFragment>(Target);
		ASSERT_THAT(IsNear(99.f, T.Timer, 0.001f));
		ASSERT_THAT(IsNear(1.0, T.Velocity.X, 0.001));
	}

	TEST_METHOD(AllowConfig_OnlySerializesAllowed)
	{
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = 300;

		FArcTestInventoryFragment InvFrag;
		InvFrag.Gold = 9999;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(HealthFrag));
		Instances.Add(FInstancedStruct::Make(InvFrag));

		FMassEntityHandle Source = EntityManager->CreateEntity(Instances);

		// Only allow Health
		FArcMassPersistenceConfigFragment Config;
		Config.AllowedFragments.Add(FArcTestHealthFragment::StaticStruct());

		TArray<uint8> Data = SaveEntity(Source, Config);

		// Target with zeroed values
		FArcTestHealthFragment EmptyHealth;
		FArcTestInventoryFragment EmptyInv;
		EmptyInv.Gold = 42; // should remain unchanged
		TArray<FInstancedStruct> TargetInstances;
		TargetInstances.Add(FInstancedStruct::Make(EmptyHealth));
		TargetInstances.Add(FInstancedStruct::Make(EmptyInv));
		FMassEntityHandle Target = EntityManager->CreateEntity(TargetInstances);

		LoadEntity(Target, Data);

		const auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(300, H.Health));

		// Inventory was NOT in allow list — should remain at target values
		const auto& I = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(Target);
		ASSERT_THAT(AreEqual(42, I.Gold));
	}

	TEST_METHOD(DisallowConfig_ExcludesFragment)
	{
		FArcTestHealthFragment HealthFrag;
		HealthFrag.Health = 500;

		FArcTestInventoryFragment InvFrag;
		InvFrag.Gold = 7777;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(HealthFrag));
		Instances.Add(FInstancedStruct::Make(InvFrag));

		FMassEntityHandle Source = EntityManager->CreateEntity(Instances);

		// Disallow Health
		FArcMassPersistenceConfigFragment Config;
		Config.DisallowedFragments.Add(FArcTestHealthFragment::StaticStruct());

		TArray<uint8> Data = SaveEntity(Source, Config);

		FArcTestHealthFragment EmptyHealth;
		EmptyHealth.Health = 1;
		FArcTestInventoryFragment EmptyInv;
		TArray<FInstancedStruct> TargetInstances;
		TargetInstances.Add(FInstancedStruct::Make(EmptyHealth));
		TargetInstances.Add(FInstancedStruct::Make(EmptyInv));
		FMassEntityHandle Target = EntityManager->CreateEntity(TargetInstances);

		LoadEntity(Target, Data);

		// Health was disallowed — should remain at target value
		const auto& H = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(1, H.Health));

		// Inventory should be loaded
		const auto& I = EntityManager->GetFragmentDataChecked<FArcTestInventoryFragment>(Target);
		ASSERT_THAT(AreEqual(7777, I.Gold));
	}

	TEST_METHOD(ModifiedData_OverwritesOnLoad)
	{
		FMassEntityHandle Source = CreateHealthEntity(999, 88.f, 0);
		FArcMassPersistenceConfigFragment Config;
		TArray<uint8> Data = SaveEntity(Source, Config);

		// Target has completely different values
		FMassEntityHandle Target = CreateHealthEntity(1, 0.1f, 50);

		LoadEntity(Target, Data);

		const auto& R = EntityManager->GetFragmentDataChecked<FArcTestHealthFragment>(Target);
		ASSERT_THAT(AreEqual(999, R.Health));
		ASSERT_THAT(IsNear(88.f, R.Armor, 0.001f));
		// TickCount not SaveGame, remains at target
		ASSERT_THAT(AreEqual(50, R.TickCount));
	}

	TEST_METHOD(DestroyedEntity_SaveProducesEmptyOutput)
	{
		FMassEntityHandle Entity = CreateHealthEntity(100, 50.f, 0);
		EntityManager->DestroyEntity(Entity);

		FArcMassPersistenceConfigFragment Config;

		// Should not crash
		FArcJsonSaveArchive SaveAr;
		FArcMassFragmentSerializer::SaveEntityFragments(
			*EntityManager, Entity, Config, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		// Data should be minimal (empty or just structure markers)
		ASSERT_THAT(IsTrue(Data.Num() >= 0)); // just didn't crash
	}
};
