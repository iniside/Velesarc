// Copyright Lukasz Baran. All Rights Reserved.

/**
 * Test: OwnerObserver_FiresOnDestroy
 *
 * Verifies UArcPlacedEntityOwnerObserver dispatches RecordRemovalForRespawn on
 * the owning partition actor when an entity carrying FArcPlacedEntityOwnerFragment
 * is destroyed.
 *
 * Setup uses a real fixture asset
 * (/ArcMassPlacedEntityTest/DA_PlacedEntityRespawnFixture) — a
 * UArcMassEntityConfigAsset with UArcPlacedEntityRespawnTrait attached — and
 * spawns through UArcMassSpawnerSubsystem so the trait pipeline runs end-to-end:
 * the const-shared respawn config fragment is set up, FArcPlacedEntityOwnerFragment
 * is added by the trait, and the archetype is registered with the entity manager's
 * observer dispatch path the same way it would be in PIE.
 *
 * The spy (ASpyPartitionActor) intercepts RecordRemovalForRespawn without needing
 * UWorldPartition / Entries fixtures on the base partition actor.
 */

#include "CQTest.h"
#include "ArcSpyPartitionActor.h"
#include "Components/ActorTestSpawner.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityRespawnTrait.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "Mass/CommandBuffer.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/ProcessingTypes.h"
#include "UObject/SoftObjectPath.h"

namespace ArcPlacedEntityOwnerObserverTest
{
	constexpr const TCHAR* FixtureAssetPath =
		TEXT("/ArcMassPlacedEntityTest/DA_PlacedEntityRespawnFixture.DA_PlacedEntityRespawnFixture");
}

TEST_CLASS(OwnerObserver_FiresOnDestroy, "ArcMassPlacedEntity.OwnerObserver")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UArcMassSpawnerSubsystem* ArcSpawner = nullptr;
	UArcMassEntityConfigAsset* FixtureConfig = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();

		ArcSpawner = Spawner.GetWorld().GetSubsystem<UArcMassSpawnerSubsystem>();
		check(ArcSpawner);

		const FSoftObjectPath FixturePath(ArcPlacedEntityOwnerObserverTest::FixtureAssetPath);
		FixtureConfig = Cast<UArcMassEntityConfigAsset>(FixturePath.TryLoad());
		check(FixtureConfig);
	}

	TEST_METHOD(Observer_RoutesToOwnerPartitionActor_OnDestroy)
	{
		const FMassEntityHandle Entity =
			ArcSpawner->SpawnEntity(FixtureConfig, FTransform::Identity);
		ASSERT_THAT(IsTrue(Entity.IsValid()));

		ASpyPartitionActor* Spy = NewObject<ASpyPartitionActor>(GetTransientPackage());
		ASSERT_THAT(IsNotNull(Spy));

		FArcPlacedEntityOwnerFragment* OwnerFrag =
			EntityManager->GetFragmentDataPtr<FArcPlacedEntityOwnerFragment>(Entity);
		ASSERT_THAT(IsNotNull(OwnerFrag));
		OwnerFrag->OwnerPartitionActor = Spy;
		OwnerFrag->EntryIndex          = 7;
		OwnerFrag->TransformIndex      = 3;

		// Flush the deferred destroy. FlushCommands IS the SUT here: it dispatches
		// the entity destroy → ObserverManager::OnPreEntitiesDestroyed → our observer.
		// CLAUDE.md discourages FlushCommands in tests but the observer-firing pipeline
		// is exactly what's being verified, and there's no processor to drive Run with.
		EntityManager->Defer().DestroyEntity(Entity);
		EntityManager->FlushCommands();

		ASSERT_THAT(AreEqual(1, Spy->RecordedCalls.Num()));
		ASSERT_THAT(AreEqual(7, Spy->RecordedCalls[0].EntryIndex));
		ASSERT_THAT(AreEqual(3, Spy->RecordedCalls[0].TransformIndex));
	}

	TEST_METHOD(Observer_HandlesNullOwner_Gracefully)
	{
		const FMassEntityHandle Entity =
			ArcSpawner->SpawnEntity(FixtureConfig, FTransform::Identity);
		ASSERT_THAT(IsTrue(Entity.IsValid()));

		// Leave OwnerPartitionActor null (default-constructed). Destroy and flush.
		EntityManager->Defer().DestroyEntity(Entity);
		EntityManager->FlushCommands();

		// Reaching here without crash is the assertion. The observer's null-check
		// (Actor != nullptr) must have guarded the RecordRemovalForRespawn call.
		ASSERT_THAT(IsTrue(true));
	}
};
