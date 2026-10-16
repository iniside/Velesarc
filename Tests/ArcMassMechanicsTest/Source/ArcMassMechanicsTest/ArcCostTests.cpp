// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassMechanicsAbilityHarness.h"
#include "ArcMassMechanicsTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcMassMechanics/Costs/ArcCostTypes.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Components/ActorTestSpawner.h"
#include "Mass/EntityManager.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CostTest_Input, "Test.MassMechanics.Cost.Input");

namespace ArcCostTestsPrivate
{
	FArcMechTest_FixedCost MakeManaCost(float Amount)
	{
		FArcMechTest_FixedCost Cost;
		Cost.Resource = FArcMechTest_CostSet::GetManaAttribute();
		Cost.Meta = FArcMechTest_CostSet::GetManaMetaAttribute();
		Cost.Amount = Amount;
		return Cost;
	}

	FArcMechTest_FixedCost MakeStaminaCost(float Amount)
	{
		FArcMechTest_FixedCost Cost;
		Cost.Resource = FArcMechTest_CostSet::GetStaminaAttribute();
		Cost.Meta = FArcMechTest_CostSet::GetStaminaMetaAttribute();
		Cost.Amount = Amount;
		return Cost;
	}

	FArcCost_Set MakeSet(const FArcMechTest_FixedCost& First, const FArcMechTest_FixedCost& Second)
	{
		FArcCost_Set Set;
		Set.Costs.Add(FInstancedStruct::Make(First));
		Set.Costs.Add(FInstancedStruct::Make(Second));
		return Set;
	}

	UArcAbilityDefinition* LoadManaAbility()
	{
		return ArcMechTest::LoadTestAsset<UArcAbilityDefinition>(TEXT("Costs/AD_Cost_Mana30"));
	}

	UArcAbilityDefinition* LoadManaStaminaAbility()
	{
		return ArcMechTest::LoadTestAsset<UArcAbilityDefinition>(TEXT("Costs/AD_Cost_ManaStamina"));
	}
}

TEST_CLASS(ArcCost_Resource, "ArcMassMechanics.Costs.Resource")
{
	FActorTestSpawner Spawner;
	ArcMassMechanicsAbilityTest::FAbilityHarness Harness;
	UArcAbilityDefinition* Definition = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Definition = NewObject<UArcAbilityDefinition>();
	}

	FArcMassAbilityCostContext MakeContext(FMassEntityHandle Entity, UArcAbilityDefinition* Ability)
	{
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Ability, TAG_CostTest_Input);
		return FArcMassAbilityCostContext{*Harness.EntityManager, Entity, Handle, Ability};
	}

	FMassEntityHandle CreateEntity(float Mana, bool bHandlerConfig = true)
	{
		ArcMassMechanicsAbilityTest::FCostEntityOptions Options;
		Options.Mana = Mana;
		Options.bHandlerConfig = bHandlerConfig;
		return Harness.CreateEntity(Options);
	}

	TEST_METHOD(CheckCost_Enough_PassesWithResultFields)
	{
		const FMassEntityHandle Entity = CreateEntity(50.f);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcMechTest_FixedCost* Cost = Ability->Cost.GetPtr<FArcMechTest_FixedCost>();
		ASSERT_THAT(IsNotNull(Cost));

		FArcCostCheckResult Result;
		const bool bPassed = Cost->CheckCost(MakeContext(Entity, Ability), &Result);

		ASSERT_THAT(IsTrue(bPassed));
		ASSERT_THAT(IsTrue(Result.bCanAfford));
		ASSERT_THAT(IsNear(30.f, Result.RequiredAmount, 0.001f));
		ASSERT_THAT(IsNear(50.f, Result.CurrentAmount, 0.001f));
		ASSERT_THAT(IsNear(0.f, Result.MissingAmount, 0.001f));
	}

	TEST_METHOD(CheckCost_ExactAmount_Passes)
	{
		const FMassEntityHandle Entity = CreateEntity(30.f);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcMechTest_FixedCost* Cost = Ability->Cost.GetPtr<FArcMechTest_FixedCost>();
		ASSERT_THAT(IsNotNull(Cost));

		ASSERT_THAT(IsTrue(Cost->CheckCost(MakeContext(Entity, Ability))));
	}

	TEST_METHOD(CheckCost_Insufficient_FailsWithResultFields)
	{
		const FMassEntityHandle Entity = CreateEntity(20.f);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcMechTest_FixedCost* Cost = Ability->Cost.GetPtr<FArcMechTest_FixedCost>();
		ASSERT_THAT(IsNotNull(Cost));

		FArcCostCheckResult Result;
		const bool bPassed = Cost->CheckCost(MakeContext(Entity, Ability), &Result);

		ASSERT_THAT(IsFalse(bPassed));
		ASSERT_THAT(IsFalse(Result.bCanAfford));
		ASSERT_THAT(IsNear(30.f, Result.RequiredAmount, 0.001f));
		ASSERT_THAT(IsNear(20.f, Result.CurrentAmount, 0.001f));
		ASSERT_THAT(IsNear(10.f, Result.MissingAmount, 0.001f));
		ASSERT_THAT(IsTrue(Result.Reason == EArcCostFailReason::Insufficient));
	}

	TEST_METHOD(CheckCost_ComparesCurrentValueNotBaseValue)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f);
		Harness.GetSet(Entity)->Mana.CurrentValue = 20.f;
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcMechTest_FixedCost* Cost = Ability->Cost.GetPtr<FArcMechTest_FixedCost>();
		ASSERT_THAT(IsNotNull(Cost));

		FArcCostCheckResult Result;
		ASSERT_THAT(IsFalse(Cost->CheckCost(MakeContext(Entity, Ability), &Result)));
		ASSERT_THAT(IsNear(20.f, Result.CurrentAmount, 0.001f));
	}

	TEST_METHOD(CheckCost_ResourceSetMissingOnEntity_Unsatisfied)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcMechTest_FixedCost* Authored = Ability->Cost.GetPtr<FArcMechTest_FixedCost>();
		ASSERT_THAT(IsNotNull(Authored));
		FArcMechTest_FixedCost Cost = *Authored;
		Cost.Resource = FArcMechTest_ResourceSet::GetResourceAAttribute();

		FArcCostCheckResult Result;
		ASSERT_THAT(IsFalse(Cost.CheckCost(MakeContext(Entity, Ability), &Result)));
		ASSERT_THAT(IsFalse(Result.bCanAfford));
		ASSERT_THAT(IsTrue(Result.Reason == EArcCostFailReason::Unsatisfied));
	}

	TEST_METHOD(CheckCost_UnsetResource_Unsatisfied)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f);
		FArcMechTest_FixedCost Cost = ArcCostTestsPrivate::MakeManaCost(10.f);
		Cost.Resource = FArcAttributeRef();

		FArcCostCheckResult Result;
		ASSERT_THAT(IsFalse(Cost.CheckCost(MakeContext(Entity, Definition), &Result)));
		ASSERT_THAT(IsTrue(Result.Reason == EArcCostFailReason::Unsatisfied));
	}

	TEST_METHOD(CheckCost_MetaSetMissingOnEntity_Unsatisfied)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcMechTest_FixedCost* Authored = Ability->Cost.GetPtr<FArcMechTest_FixedCost>();
		ASSERT_THAT(IsNotNull(Authored));
		FArcMechTest_FixedCost Cost = *Authored;
		Cost.Meta = FArcMechTest_ResourceSet::GetResourceADeltaAttribute();

		FArcCostCheckResult Result;
		ASSERT_THAT(IsFalse(Cost.CheckCost(MakeContext(Entity, Ability), &Result)));
		ASSERT_THAT(IsFalse(Result.bCanAfford));
		ASSERT_THAT(IsTrue(Result.Reason == EArcCostFailReason::Unsatisfied));
	}

	TEST_METHOD(ApplyCost_EnqueuesMeta_HandlerSubtractsResourceOnDrain)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcMechTest_FixedCost* Cost = Ability->Cost.GetPtr<FArcMechTest_FixedCost>();
		ASSERT_THAT(IsNotNull(Cost));

		Cost->ApplyCost(MakeContext(Entity, Ability));
		ASSERT_THAT(IsNear(100.f, Harness.GetSet(Entity)->Mana.CurrentValue, 0.001f));

		Harness.DrainPendingOps(Entity);

		const FArcMechTest_CostSet* Set = Harness.GetSet(Entity);
		ASSERT_THAT(IsNear(70.f, Set->Mana.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(70.f, Set->Mana.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->ManaMeta.CurrentValue, 0.001f));
	}

	TEST_METHOD(ApplyCost_WithoutHandler_WritesMetaNotResource)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f, false);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcMechTest_FixedCost* Cost = Ability->Cost.GetPtr<FArcMechTest_FixedCost>();
		ASSERT_THAT(IsNotNull(Cost));

		Cost->ApplyCost(MakeContext(Entity, Ability));
		Harness.DrainPendingOps(Entity);

		const FArcMechTest_CostSet* Set = Harness.GetSet(Entity);
		ASSERT_THAT(IsNear(100.f, Set->Mana.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(30.f, Set->ManaMeta.CurrentValue, 0.001f));
	}

	TEST_METHOD(ApplyCost_ZeroOrNegativeAmount_EnqueuesNothing)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f, false);
		const FArcMassAbilityCostContext Context = MakeContext(Entity, Definition);

		ArcCostTestsPrivate::MakeManaCost(0.f).ApplyCost(Context);
		ArcCostTestsPrivate::MakeManaCost(-10.f).ApplyCost(Context);
		Harness.DrainPendingOps(Entity);

		const FArcMechTest_CostSet* Set = Harness.GetSet(Entity);
		ASSERT_THAT(IsNear(0.f, Set->ManaMeta.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(100.f, Set->Mana.CurrentValue, 0.001f));
	}

	TEST_METHOD(ApplyCost_UnsetMeta_EnqueuesNothing)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f, false);
		FArcMechTest_FixedCost Cost = ArcCostTestsPrivate::MakeManaCost(25.f);
		Cost.Meta = FArcAttributeRef();

		Cost.ApplyCost(MakeContext(Entity, Definition));
		Harness.DrainPendingOps(Entity);

		const FArcMechTest_CostSet* Set = Harness.GetSet(Entity);
		ASSERT_THAT(IsNear(0.f, Set->ManaMeta.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(100.f, Set->Mana.CurrentValue, 0.001f));
	}

	TEST_METHOD(Activation_CommitAppliesResourceCost)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f);
		Definition->StateTree = ArcMassMechanicsAbilityTest::MakeCommitTree(Harness.World);
		UArcAbilityDefinition* ManaAbility = ArcCostTestsPrivate::LoadManaAbility();
		ASSERT_THAT(IsNotNull(ManaAbility));
		Definition->Cost = ManaAbility->Cost;
		ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Definition, TAG_CostTest_Input);

		Harness.Activate(Entity, TAG_CostTest_Input);
		Harness.DrainPendingOps(Entity);

		ASSERT_THAT(IsNear(70.f, Harness.GetSet(Entity)->Mana.CurrentValue, 0.001f));
	}
};

TEST_CLASS(ArcCost_Set, "ArcMassMechanics.Costs.Set")
{
	FActorTestSpawner Spawner;
	ArcMassMechanicsAbilityTest::FAbilityHarness Harness;
	UArcAbilityDefinition* Definition = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Definition = NewObject<UArcAbilityDefinition>();
	}

	FMassEntityHandle CreateEntity(float Mana, float Stamina)
	{
		ArcMassMechanicsAbilityTest::FCostEntityOptions Options;
		Options.Mana = Mana;
		Options.Stamina = Stamina;
		return Harness.CreateEntity(Options);
	}

	FArcMassAbilityCostContext MakeContext(FMassEntityHandle Entity, UArcAbilityDefinition* Ability)
	{
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Ability, TAG_CostTest_Input);
		return FArcMassAbilityCostContext{*Harness.EntityManager, Entity, Handle, Ability};
	}

	TEST_METHOD(CheckCost_AllAffordable_PassesWithDefaultResult)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f, 100.f);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaStaminaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcCost_Set* Set = Ability->Cost.GetPtr<FArcCost_Set>();
		ASSERT_THAT(IsNotNull(Set));

		FArcCostCheckResult Result;
		Result.bCanAfford = false;
		Result.RequiredAmount = 99.f;
		ASSERT_THAT(IsTrue(Set->CheckCost(MakeContext(Entity, Ability), &Result)));
		ASSERT_THAT(IsTrue(Result.bCanAfford));
		ASSERT_THAT(IsNear(0.f, Result.RequiredAmount, 0.001f));
	}

	TEST_METHOD(CheckCost_OnlySecondEntryUnaffordable_FailsWithSecondResult)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f, 10.f);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaStaminaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcCost_Set* Set = Ability->Cost.GetPtr<FArcCost_Set>();
		ASSERT_THAT(IsNotNull(Set));

		FArcCostCheckResult Result;
		ASSERT_THAT(IsFalse(Set->CheckCost(MakeContext(Entity, Ability), &Result)));
		ASSERT_THAT(IsFalse(Result.bCanAfford));
		ASSERT_THAT(IsNear(40.f, Result.RequiredAmount, 0.001f));
		ASSERT_THAT(IsNear(10.f, Result.CurrentAmount, 0.001f));
		ASSERT_THAT(IsNear(30.f, Result.MissingAmount, 0.001f));
		ASSERT_THAT(IsTrue(Result.Reason == EArcCostFailReason::Insufficient));
	}

	TEST_METHOD(CheckCost_BothUnaffordable_ReportsFirstFailure)
	{
		const FMassEntityHandle Entity = CreateEntity(5.f, 10.f);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaStaminaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcCost_Set* Set = Ability->Cost.GetPtr<FArcCost_Set>();
		ASSERT_THAT(IsNotNull(Set));

		FArcCostCheckResult Result;
		ASSERT_THAT(IsFalse(Set->CheckCost(MakeContext(Entity, Ability), &Result)));
		ASSERT_THAT(IsNear(30.f, Result.RequiredAmount, 0.001f));
		ASSERT_THAT(IsNear(5.f, Result.CurrentAmount, 0.001f));
		ASSERT_THAT(IsNear(25.f, Result.MissingAmount, 0.001f));
	}

	TEST_METHOD(CheckCost_UnsatisfiedEntry_ReportsUnsatisfied)
	{
		using namespace ArcCostTestsPrivate;
		const FMassEntityHandle Entity = CreateEntity(100.f, 100.f);
		FArcMechTest_FixedCost Unresolved = MakeStaminaCost(1.f);
		Unresolved.Resource = FArcMechTest_ResourceSet::GetResourceAAttribute();
		const FArcCost_Set Set = MakeSet(MakeManaCost(30.f), Unresolved);

		FArcCostCheckResult Result;
		ASSERT_THAT(IsFalse(Set.CheckCost(MakeContext(Entity, Definition), &Result)));
		ASSERT_THAT(IsTrue(Result.Reason == EArcCostFailReason::Unsatisfied));
	}

	TEST_METHOD(ApplyCost_AppliesEveryEntry)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f, 100.f);
		UArcAbilityDefinition* Ability = ArcCostTestsPrivate::LoadManaStaminaAbility();
		ASSERT_THAT(IsNotNull(Ability));
		const FArcCost_Set* Set = Ability->Cost.GetPtr<FArcCost_Set>();
		ASSERT_THAT(IsNotNull(Set));

		Set->ApplyCost(MakeContext(Entity, Ability));
		Harness.DrainPendingOps(Entity);

		const FArcMechTest_CostSet* Attributes = Harness.GetSet(Entity);
		ASSERT_THAT(IsNear(70.f, Attributes->Mana.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(60.f, Attributes->Stamina.CurrentValue, 0.001f));
	}

	TEST_METHOD(Activation_SecondEntryUnaffordable_AppliesNothing)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f, 10.f);
		Definition->StateTree = ArcMassMechanicsAbilityTest::MakeCommitTree(Harness.World);
		UArcAbilityDefinition* ManaStaminaAbility = ArcCostTestsPrivate::LoadManaStaminaAbility();
		ASSERT_THAT(IsNotNull(ManaStaminaAbility));
		Definition->Cost = ManaStaminaAbility->Cost;
		ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Definition, TAG_CostTest_Input);

		Harness.Activate(Entity, TAG_CostTest_Input);
		Harness.DrainPendingOps(Entity);

		const FArcMechTest_CostSet* Attributes = Harness.GetSet(Entity);
		ASSERT_THAT(IsNear(100.f, Attributes->Mana.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(10.f, Attributes->Stamina.CurrentValue, 0.001f));
	}

	TEST_METHOD(Activation_AllAffordable_CommitAppliesEveryEntry)
	{
		const FMassEntityHandle Entity = CreateEntity(100.f, 100.f);
		Definition->StateTree = ArcMassMechanicsAbilityTest::MakeCommitTree(Harness.World);
		UArcAbilityDefinition* ManaStaminaAbility = ArcCostTestsPrivate::LoadManaStaminaAbility();
		ASSERT_THAT(IsNotNull(ManaStaminaAbility));
		Definition->Cost = ManaStaminaAbility->Cost;
		ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Definition, TAG_CostTest_Input);

		Harness.Activate(Entity, TAG_CostTest_Input);
		Harness.DrainPendingOps(Entity);

		const FArcMechTest_CostSet* Attributes = Harness.GetSet(Entity);
		ASSERT_THAT(IsNear(70.f, Attributes->Mana.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(60.f, Attributes->Stamina.CurrentValue, 0.001f));
	}
};
