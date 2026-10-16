// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "GameFramework/DefaultPawn.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/EntityHandle.h"

#include "Fragments/ArcAbilityInputFragment.h"

#include "ArcMassGameFrameworkTestTypes.h"
#include "Pawn/ArcMassFrameworkAgentComponent.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "Pawn/ArcMassPlayerInputComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcMassFwkTest_Input_Fire, "Test.InputTag.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcMassFwkTest_Input_Reload, "Test.InputTag.Reload");

namespace ArcMassFwkInputTestHelpers
{
	static FMassEntityHandle CreateInputEntity(FMassEntityManager& EntityManager)
	{
		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(FArcAbilityInputFragment()));
		return EntityManager.CreateEntity(Instances);
	}
}

TEST_CLASS(ArcMassPawnExtensionComponent_InputRouting, "ArcMassGameFramework.PawnExtensionComponent.InputRouting")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
	}

	TEST_METHOD(InputPressed_WritesToEntityFragment)
	{
		ADefaultPawn& Pawn = Spawner.SpawnActor<ADefaultPawn>();

		const FMassEntityHandle Entity = ArcMassFwkInputTestHelpers::CreateInputEntity(*EntityManager);

		UArcMassFrameworkAgentComponentTestStub* Agent = NewObject<UArcMassFrameworkAgentComponentTestStub>(&Pawn);
		Agent->RegisterComponent();
		Agent->Test_SetEntityHandle(Entity);

		UArcMassPlayerInputComponent* Hero = NewObject<UArcMassPlayerInputComponent>(&Pawn);
		Hero->RegisterComponent();

		Hero->Input_AbilityInputTagPressed(TAG_ArcMassFwkTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = *View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>();
		ASSERT_THAT(AreEqual(1, Input.PressedThisFrame.Num()));
		ASSERT_THAT(IsTrue(Input.PressedThisFrame.Contains(TAG_ArcMassFwkTest_Input_Fire)));
		ASSERT_THAT(IsTrue(Input.HeldInputs.HasTag(TAG_ArcMassFwkTest_Input_Fire)));
		ASSERT_THAT(AreEqual(0, Input.ReleasedThisFrame.Num()));
	}

	TEST_METHOD(InputReleased_WritesToEntityFragment)
	{
		ADefaultPawn& Pawn = Spawner.SpawnActor<ADefaultPawn>();

		const FMassEntityHandle Entity = ArcMassFwkInputTestHelpers::CreateInputEntity(*EntityManager);

		UArcMassFrameworkAgentComponentTestStub* Agent = NewObject<UArcMassFrameworkAgentComponentTestStub>(&Pawn);
		Agent->RegisterComponent();
		Agent->Test_SetEntityHandle(Entity);

		UArcMassPlayerInputComponent* Hero = NewObject<UArcMassPlayerInputComponent>(&Pawn);
		Hero->RegisterComponent();

		Hero->Input_AbilityInputTagPressed(TAG_ArcMassFwkTest_Input_Reload);
		Hero->Input_AbilityInputTagReleased(TAG_ArcMassFwkTest_Input_Reload);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = *View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>();
		ASSERT_THAT(IsTrue(Input.PressedThisFrame.Contains(TAG_ArcMassFwkTest_Input_Reload)));
		ASSERT_THAT(IsTrue(Input.ReleasedThisFrame.Contains(TAG_ArcMassFwkTest_Input_Reload)));
		ASSERT_THAT(IsFalse(Input.HeldInputs.HasTag(TAG_ArcMassFwkTest_Input_Reload)));
	}

};
