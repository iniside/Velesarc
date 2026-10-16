// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "AIController.h"
#include "GameFramework/DefaultPawn.h"
#include "GameplayTagContainer.h"
#include "Components/GameFrameworkComponentManager.h"

#include "Core/ArcMassGameFrameworkTags.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "ArcMassGameFrameworkTestTypes.h"
#include "StructUtils/InstancedStruct.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassInitChainTest, Display, All);

namespace ArcMassFrameworkTests
{
	static FGameplayTag InitStateSpawnedTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("InitState.Spawned")), /*ErrorIfNotFound*/ false);
		return Tag;
	}

	static FGameplayTag InitStateDataAvailableTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("InitState.DataAvailable")), /*ErrorIfNotFound*/ false);
		return Tag;
	}

	static FGameplayTag InitStateDataInitializedTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("InitState.DataInitialized")), /*ErrorIfNotFound*/ false);
		return Tag;
	}
}

/**
 * Scope: drives the PawnExtension init chain with focused unit-style entry points
 * (BeginPlay -> CheckDefaultInitialization, plus direct HandleChangeInitState invocation
 * for the DataAvailable -> DataInitialized step). Avoids depending on an authoritative
 * Mass entity-creation pipeline (UMassAgentSubsystem-driven) which is not available
 * inside FActorTestSpawner without a fully booted PIE world.
 */
TEST_CLASS(ArcMassPawnExtension_InitChain, "ArcMassGameFramework.PawnExtension.InitChain")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		FArcMassTestPawnInitFragment::Reset();
		FArcMassTestAddPawnComponent::Reset();
	}

	TEST_METHOD(HandleChangeInitState_RunsGiveAndInitialize_AndFiresReadyDelegate)
	{
		ADefaultPawn& Pawn = Spawner.SpawnActor<ADefaultPawn>();

		// Build a minimal PawnData with one AddPawnComponent and one InitFragment.
		UArcMassPawnData* PawnData = NewObject<UArcMassPawnData>(&Pawn);
		PawnData->Components.Add(FInstancedStruct::Make<FArcMassTestAddPawnComponent>());
		PawnData->InitFragments.Add(FInstancedStruct::Make<FArcMassTestPawnInitFragment>());

		UArcMassPawnExtensionComponent* Extension = NewObject<UArcMassPawnExtensionComponent>(&Pawn);
		Extension->RegisterComponent();

		// Authority path: Pawn is spawned by the test world, default standalone -> HasAuthority == true.
		Extension->SetPawnData(PawnData);

		int32 ReadyCount = 0;
		Extension->OnPawnReadyToInitialize().AddLambda(
			[&ReadyCount](UArcMassPawnExtensionComponent*)
			{
				ReadyCount++;
			});

		// Drive the DataAvailable -> DataInitialized step directly so we exercise
		// HandleChangeInitState without needing the full state-manager dance to gate it.
		UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(&Pawn);
		Extension->HandleChangeInitState(Manager
			, ArcMassFrameworkTests::InitStateDataAvailableTag()
			, ArcMassFrameworkTests::InitStateDataInitializedTag());

		// Both should have fired exactly once, GiveComponent before Initialize.
		ASSERT_THAT(AreEqual(1, FArcMassTestAddPawnComponent::GiveCount));
		ASSERT_THAT(AreEqual(1, FArcMassTestPawnInitFragment::InitCount));
		ASSERT_THAT(IsTrue(FArcMassTestAddPawnComponent::LastGiveOrderIndex >= 0));
		ASSERT_THAT(IsTrue(FArcMassTestPawnInitFragment::LastInitOrderIndex > FArcMassTestAddPawnComponent::LastGiveOrderIndex));

		// Ready delegate should have fired exactly once.
		ASSERT_THAT(AreEqual(1, ReadyCount));
	}

	TEST_METHOD(OnPawnReadyToInitialize_RegisterAndCall_FiresImmediatelyWhenAlreadyReady)
	{
		ADefaultPawn& Pawn = Spawner.SpawnActor<ADefaultPawn>();

		UArcMassPawnData* PawnData = NewObject<UArcMassPawnData>(&Pawn);

		UArcMassPawnExtensionComponent* Extension = NewObject<UArcMassPawnExtensionComponent>(&Pawn);
		Extension->RegisterComponent();
		Extension->SetPawnData(PawnData);

		UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(&Pawn);
		Extension->HandleChangeInitState(Manager
			, ArcMassFrameworkTests::InitStateDataAvailableTag()
			, ArcMassFrameworkTests::InitStateDataInitializedTag());

		int32 LateCount = 0;
		Extension->OnPawnReadyToInitialize_RegisterAndCall(
			UArcMassPawnExtensionComponent::FArcMassOnPawnReadyToInitialize::FDelegate::CreateLambda(
				[&LateCount](UArcMassPawnExtensionComponent*)
				{
					LateCount++;
				}));

		ASSERT_THAT(AreEqual(1, LateCount));
	}

	/**
	 * Gates the four early-out branches of CanChangeInitState (Spawned -> DataAvailable):
	 *   1) PawnData missing             -> false
	 *   2) Controller missing on auth   -> false
	 *   3) AgentComp/EntityHandle bad   -> false
	 *   4) All three present + bot path -> true
	 *
	 * The "bot path" (AAIController possession) sets IsBotControlled() so the predicate
	 * skips the locally-controlled InputComponent/LocalPlayer requirement at
	 * ArcMassPawnExtensionComponent.cpp:302-310.
	 */
	TEST_METHOD(CanChangeInitState_FailsWithoutPawnData)
	{
		ADefaultPawn& Pawn = Spawner.SpawnActor<ADefaultPawn>();

		UArcMassPawnExtensionComponent* Extension = NewObject<UArcMassPawnExtensionComponent>(&Pawn);
		Extension->RegisterComponent();

		UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(&Pawn);

		const bool bCan = Extension->CanChangeInitState(Manager
			, FArcMassGameFrameworkTags::Get().InitState_Spawned
			, FArcMassGameFrameworkTags::Get().InitState_DataAvailable);

		ASSERT_THAT(IsFalse(bCan));
	}

	TEST_METHOD(CanChangeInitState_FailsWithoutController)
	{
		ADefaultPawn& Pawn = Spawner.SpawnActor<ADefaultPawn>();

		UArcMassPawnData* PawnData = NewObject<UArcMassPawnData>(&Pawn);

		UArcMassPawnExtensionComponent* Extension = NewObject<UArcMassPawnExtensionComponent>(&Pawn);
		Extension->RegisterComponent();
		Extension->SetPawnData(PawnData);

		// No controller possession; ADefaultPawn from Spawner.SpawnActor has no controller.
		UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(&Pawn);

		const bool bCan = Extension->CanChangeInitState(Manager
			, FArcMassGameFrameworkTags::Get().InitState_Spawned
			, FArcMassGameFrameworkTags::Get().InitState_DataAvailable);

		ASSERT_THAT(IsFalse(bCan));
	}

	TEST_METHOD(CanChangeInitState_FailsWithoutEntityHandle)
	{
		ADefaultPawn& Pawn = Spawner.SpawnActor<ADefaultPawn>();

		UArcMassPawnData* PawnData = NewObject<UArcMassPawnData>(&Pawn);

		UArcMassPawnExtensionComponent* Extension = NewObject<UArcMassPawnExtensionComponent>(&Pawn);
		Extension->RegisterComponent();
		Extension->SetPawnData(PawnData);

		AAIController& AI = Spawner.SpawnActor<AAIController>();
		AI.Possess(&Pawn);

		// Register an agent component but DO NOT stamp an entity handle.
		UArcMassFrameworkAgentComponentTestStub* AgentComp =
			NewObject<UArcMassFrameworkAgentComponentTestStub>(&Pawn);
		AgentComp->RegisterComponent();

		UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(&Pawn);

		const bool bCan = Extension->CanChangeInitState(Manager
			, FArcMassGameFrameworkTags::Get().InitState_Spawned
			, FArcMassGameFrameworkTags::Get().InitState_DataAvailable);

		ASSERT_THAT(IsFalse(bCan));
	}

};
