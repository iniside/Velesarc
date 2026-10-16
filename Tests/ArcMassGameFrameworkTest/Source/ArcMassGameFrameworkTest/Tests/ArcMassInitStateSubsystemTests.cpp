// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"
#include "TestGameInstance.h"

#include "Core/ArcMassGameFrameworkTags.h"

/**
 * Verifies UArcMassFrameworkInitStateSubsystem registers exactly the three init-state
 * tags it claims to (Spawned, DataAvailable, DataInitialized) with the
 * UGameFrameworkComponentManager, and that InitState_GameplayReady is intentionally
 * NOT registered.
 *
 * Probe technique:
 *   UGameFrameworkComponentManager::IsInitStateAfterOrEqual(A, B) walks the manager's
 *   internal InitStateOrder array. For an unregistered tag the call returns false in
 *   both directions (A->B and B->A). For a registered tag, walking against another
 *   registered tag returns the relative order. We use this as a clean, side-effect-free
 *   read of the manager's registry.
 */
TEST_CLASS(ArcMassInitStateSubsystem_Tests, "ArcMassGameFramework.InitStateSubsystem")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(RegistersThreeStates_NotFour)
	{
		UGameInstance* GameInstance = static_cast<UGameInstance*>(Spawner.GetGameInstance());
		ASSERT_THAT(IsNotNull(GameInstance));

		UGameFrameworkComponentManager* ComponentManager =
			GameInstance->GetSubsystem<UGameFrameworkComponentManager>();
		ASSERT_THAT(IsNotNull(ComponentManager));

		const FArcMassGameFrameworkTags& Tags = FArcMassGameFrameworkTags::Get();

		// --- Positive: the three states the subsystem registers must be ordered ---
		// IsInitStateAfterOrEqual(A, B) returns true only if both A and B are in
		// InitStateOrder and A appears at-or-after B (or A == B).
		ASSERT_THAT(IsTrue(ComponentManager->IsInitStateAfterOrEqual(
			Tags.InitState_Spawned, Tags.InitState_Spawned)));
		ASSERT_THAT(IsTrue(ComponentManager->IsInitStateAfterOrEqual(
			Tags.InitState_DataAvailable, Tags.InitState_Spawned)));
		ASSERT_THAT(IsTrue(ComponentManager->IsInitStateAfterOrEqual(
			Tags.InitState_DataInitialized, Tags.InitState_DataAvailable)));
		ASSERT_THAT(IsTrue(ComponentManager->IsInitStateAfterOrEqual(
			Tags.InitState_DataInitialized, Tags.InitState_Spawned)));

		// --- Negative ordering between the three (descending direction) ---
		ASSERT_THAT(IsFalse(ComponentManager->IsInitStateAfterOrEqual(
			Tags.InitState_Spawned, Tags.InitState_DataAvailable)));
		ASSERT_THAT(IsFalse(ComponentManager->IsInitStateAfterOrEqual(
			Tags.InitState_DataAvailable, Tags.InitState_DataInitialized)));

		// --- Negative: GameplayReady must NOT be registered. ---
		// If GameplayReady were registered after DataInitialized,
		// IsInitStateAfterOrEqual(GameplayReady, DataInitialized) would return true.
		// If it were registered before DataInitialized, the inverse would return true.
		// Both must be false to prove the tag is absent from InitStateOrder.
		ASSERT_THAT(IsFalse(ComponentManager->IsInitStateAfterOrEqual(
			Tags.InitState_GameplayReady, Tags.InitState_DataInitialized)));
		ASSERT_THAT(IsFalse(ComponentManager->IsInitStateAfterOrEqual(
			Tags.InitState_DataInitialized, Tags.InitState_GameplayReady)));
		ASSERT_THAT(IsFalse(ComponentManager->IsInitStateAfterOrEqual(
			Tags.InitState_GameplayReady, Tags.InitState_Spawned)));
		ASSERT_THAT(IsFalse(ComponentManager->IsInitStateAfterOrEqual(
			Tags.InitState_Spawned, Tags.InitState_GameplayReady)));
	}
};
