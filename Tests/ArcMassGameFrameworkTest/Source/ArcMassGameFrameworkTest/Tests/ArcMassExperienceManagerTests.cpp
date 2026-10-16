// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/GameStateBase.h"

#include "Experience/ArcMassExperienceDefinition.h"
#include "Experience/ArcMassExperienceManagerComponent.h"

namespace ArcMassFrameworkTests
{
	struct FExperienceDelegateRecorder
	{
		int32 Count = 0;
		const UArcMassExperienceDefinition* LastSeen = nullptr;

		void OnLoaded(const UArcMassExperienceDefinition* Exp)
		{
			Count++;
			LastSeen = Exp;
		}
	};
}

/**
 * Scope note: A full ServerSetCurrentExperience -> Loaded path requires an asset-registered
 * UArcMassExperienceDefinition primary asset, GameFeature plugin activation, and a fully
 * initialized GameState. None of that fits into FActorTestSpawner. This suite exercises the
 * delegate registration semantics and the public load-state API of the manager component
 * (the parts that are testable without PIE / asset registry / GFM). The full pipeline is
 * exercised by Gauntlet integration tests.
 */
TEST_CLASS(ArcMassExperienceManager_Tests, "ArcMassGameFramework.ExperienceManager")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	/**
	 * Pins CallOrRegister_OnExperienceLoaded queueing semantics: while the manager is
	 * not Loaded, registered delegates do not fire. When the manager transitions to
	 * Loaded (driven via the FArcMassExperienceManagerTestSeam, since the full
	 * load pipeline is not exercisable here), every registered listener fires
	 * exactly once. Order is intentionally not asserted to avoid coupling to
	 * multicast registration order.
	 */
	TEST_METHOD(CallOrRegister_QueuesWhileUnloaded_FiresOnceWhenLoaded)
	{
		AGameStateBase& GameState = Spawner.SpawnActor<AGameStateBase>();
		UArcMassExperienceManagerComponent* Manager = NewObject<UArcMassExperienceManagerComponent>(&GameState);
		Manager->RegisterComponent();

		int32 CountA = 0;
		int32 CountB = 0;
		Manager->CallOrRegister_OnExperienceLoaded(
			FOnArcMassExperienceLoaded::FDelegate::CreateLambda(
				[&CountA](const UArcMassExperienceDefinition*) { ++CountA; }));
		Manager->CallOrRegister_OnExperienceLoaded(
			FOnArcMassExperienceLoaded::FDelegate::CreateLambda(
				[&CountB](const UArcMassExperienceDefinition*) { ++CountB; }));

		// Neither should fire while LoadState != Loaded.
		ASSERT_THAT(AreEqual(0, CountA));
		ASSERT_THAT(AreEqual(0, CountB));

		FArcMassExperienceManagerTestSeam::ForceLoadedAndBroadcast(*Manager, /*Experience=*/nullptr);

		ASSERT_THAT(AreEqual(1, CountA));
		ASSERT_THAT(AreEqual(1, CountB));
	}
};
