// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "GameFramework/DefaultPawn.h"
#include "GameplayTagContainer.h"
#include "Components/GameFrameworkComponentManager.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcMassGameFrameworkTestTypes.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"

namespace ArcMassFrameworkTests
{
	static FGameplayTag DataAvailableTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("InitState.DataAvailable")), /*ErrorIfNotFound*/ false);
		return Tag;
	}

	static FGameplayTag DataInitializedTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("InitState.DataInitialized")), /*ErrorIfNotFound*/ false);
		return Tag;
	}
}

/**
 * Verifies that during the DataAvailable -> DataInitialized transition the pawn extension
 * component invokes every FArcMassAddPawnComponent::GiveComponent BEFORE every
 * FArcMassPawnInitFragment::Initialize, exactly once each.
 */
TEST_CLASS(ArcMassPawnData_FragmentApply, "ArcMassGameFramework.PawnData.FragmentApply")
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

	TEST_METHOD(InitFragment_FiresOnce_AfterAddComponent)
	{
		ADefaultPawn& Pawn = Spawner.SpawnActor<ADefaultPawn>();

		UArcMassPawnData* PawnData = NewObject<UArcMassPawnData>(&Pawn);
		PawnData->Components.Add(FInstancedStruct::Make<FArcMassTestAddPawnComponent>());
		PawnData->InitFragments.Add(FInstancedStruct::Make<FArcMassTestPawnInitFragment>());

		UArcMassPawnExtensionComponent* Extension = NewObject<UArcMassPawnExtensionComponent>(&Pawn);
		Extension->RegisterComponent();
		Extension->SetPawnData(PawnData);

		UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(&Pawn);
		Extension->HandleChangeInitState(Manager
			, ArcMassFrameworkTests::DataAvailableTag()
			, ArcMassFrameworkTests::DataInitializedTag());

		ASSERT_THAT(AreEqual(1, FArcMassTestAddPawnComponent::GiveCount));
		ASSERT_THAT(AreEqual(1, FArcMassTestPawnInitFragment::InitCount));

		// Ordering: Components applied first, then InitFragments.
		ASSERT_THAT(IsTrue(FArcMassTestAddPawnComponent::LastGiveOrderIndex >= 0));
		ASSERT_THAT(IsTrue(FArcMassTestPawnInitFragment::LastInitOrderIndex > FArcMassTestAddPawnComponent::LastGiveOrderIndex));
	}

	TEST_METHOD(MultipleEntries_AllFire)
	{
		ADefaultPawn& Pawn = Spawner.SpawnActor<ADefaultPawn>();

		UArcMassPawnData* PawnData = NewObject<UArcMassPawnData>(&Pawn);
		PawnData->Components.Add(FInstancedStruct::Make<FArcMassTestAddPawnComponent>());
		PawnData->Components.Add(FInstancedStruct::Make<FArcMassTestAddPawnComponent>());
		PawnData->InitFragments.Add(FInstancedStruct::Make<FArcMassTestPawnInitFragment>());
		PawnData->InitFragments.Add(FInstancedStruct::Make<FArcMassTestPawnInitFragment>());
		PawnData->InitFragments.Add(FInstancedStruct::Make<FArcMassTestPawnInitFragment>());

		UArcMassPawnExtensionComponent* Extension = NewObject<UArcMassPawnExtensionComponent>(&Pawn);
		Extension->RegisterComponent();
		Extension->SetPawnData(PawnData);

		UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(&Pawn);
		Extension->HandleChangeInitState(Manager
			, ArcMassFrameworkTests::DataAvailableTag()
			, ArcMassFrameworkTests::DataInitializedTag());

		ASSERT_THAT(AreEqual(2, FArcMassTestAddPawnComponent::GiveCount));
		ASSERT_THAT(AreEqual(3, FArcMassTestPawnInitFragment::InitCount));
	}
};
