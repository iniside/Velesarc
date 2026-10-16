// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcRPGMechanicsTestHarness.h"
#include "ArcRPGMechanicsTestTypes.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Abilities/ArcRPGCooldown_CascadeReset.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"
#include "Abilities/Tasks/ArcAbilityTask_Commit.h"
#include "Abilities/Tasks/ArcRPGAbilityTask_CascadeIncrement.h"
#include "Attributes/ArcAttributeSet_RPGCoreStats.h"
#include "Attributes/ArcAttributeSet_RPGEnergy.h"
#include "Attributes/ArcAttributeSet_RPGEntropy.h"
#include "Attributes/ArcAttributeSet_RPGSkills.h"
#include "Attributes/ArcAttributeSet_RPGStamina.h"
#include "Components/ActorTestSpawner.h"
#include "Costs/ArcRPGItemCost.h"
#include "Engine/World.h"
#include "Fragments/ArcAbilityCustomData.h"
#include "Items/ArcItemData.h"
#include "Mass/EntityManager.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcRPGItemCostTestsPrivate
{
	template <typename TCost>
	const TCost* FindCost(const UArcAbilityDefinition& Ability)
	{
		return Ability.Cost.GetPtr<TCost>();
	}

	UStateTree* MakeCommitThenAddStackTree(UObject* Outer)
	{
		UStateTree* StateTree = NewObject<UStateTree>(Outer);
		UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
		StateTree->EditorData = EditorData;
		EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

		UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
		Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

		UStateTreeState& Run = Root.AddChildState(FName(TEXT("CommitAndCascade")));
		Run.AddTask<FArcAbilityTask_Commit>();
		Run.AddTask<FArcRPGAbilityTask_CascadeIncrement>();
		Run.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bCompiled = Compiler.Compile(*StateTree);
		ensureAlwaysMsgf(bCompiled, TEXT("MakeCommitThenAddStackTree failed to compile"));
		return StateTree;
	}
}

TEST_CLASS(ArcRPGCost_Item, "ArcRPGMechanics.Costs.Item")
{
	FActorTestSpawner Spawner;
	ArcRPGMechanicsTestHarness::FHarness Harness;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	FArcMassAbilityCostContext GrantWithItem(UArcAbilityDefinition* Ability, FMassEntityHandle Entity, FArcItemData& Item)
	{
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Ability, ArcRPGMechanicsTestTags::Input,
			FMassEntityHandle(), ArcRPGMechanicsTestHarness::MakeItemCustomData(Item));
		return FArcMassAbilityCostContext{*Harness.EntityManager, Entity, Handle, Ability};
	}

	FArcMassAbilityCostContext GrantWithoutItem(UArcAbilityDefinition* Ability, FMassEntityHandle Entity)
	{
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Ability, ArcRPGMechanicsTestTags::Input);
		return FArcMassAbilityCostContext{*Harness.EntityManager, Entity, Handle, Ability};
	}

	bool MakeItem(const TCHAR* Name, FArcItemData& OutItem)
	{
		const FArcItemDefinitionData* Definition = ArcRPGMechanicsTestHarness::LoadItem(Name);
		if (!Definition)
		{
			return false;
		}
		ArcRPGMechanicsTestHarness::MakeItem(Definition, OutItem);
		return true;
	}

	TEST_METHOD(MissingItem_CheckFailsUnsatisfied)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Energy"));
		ASSERT_THAT(IsNotNull(Ability));
		ArcRPGMechanicsTestHarness::FCasterOptions Caster;
		Caster.Energy = 1000.f;
		const FMassEntityHandle Entity = Harness.CreateCaster(Caster);

		FArcCostCheckResult Result;
		const FArcRPGCost_Ability* AbilityCost = ArcRPGItemCostTestsPrivate::FindCost<FArcRPGCost_Ability>(*Ability);
		ASSERT_THAT(IsNotNull(AbilityCost));
		ASSERT_THAT(IsFalse(AbilityCost->CheckCost(GrantWithoutItem(Ability, Entity), &Result)));
		ASSERT_THAT(IsFalse(Result.bCanAfford));
		ASSERT_THAT(IsTrue(Result.Reason == EArcCostFailReason::Unsatisfied));
	}

	TEST_METHOD(MissingItem_ApplyChangesNothing)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Energy"));
		ASSERT_THAT(IsNotNull(Ability));
		const FMassEntityHandle Entity = Harness.CreateCaster();

		const FArcRPGCost_Ability* AbilityCost = ArcRPGItemCostTestsPrivate::FindCost<FArcRPGCost_Ability>(*Ability);
		ASSERT_THAT(IsNotNull(AbilityCost));
		AbilityCost->ApplyCost(GrantWithoutItem(Ability, Entity));
		Harness.DrainPendingOps(Entity);

		ASSERT_THAT(IsNear(100.f, Harness.GetEnergy(Entity)->Energy.CurrentValue, 0.001f));
	}

	TEST_METHOD(Energy_NoStacks_RequiresTheBaseEnergyCost)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Energy"));
		ASSERT_THAT(IsNotNull(Ability));
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeItem(TEXT("ID_Spell_Cascade3"), Item)));

		ArcRPGMechanicsTestHarness::FCasterOptions Caster;
		Caster.Energy = 10.f;
		const FMassEntityHandle Entity = Harness.CreateCaster(Caster);

		FArcCostCheckResult Result;
		const FArcRPGCost_Ability* AbilityCost = ArcRPGItemCostTestsPrivate::FindCost<FArcRPGCost_Ability>(*Ability);
		ASSERT_THAT(IsNotNull(AbilityCost));
		ASSERT_THAT(IsFalse(AbilityCost->CheckCost(GrantWithItem(Ability, Entity, Item), &Result)));
		ASSERT_THAT(IsNear(20.f, Result.RequiredAmount, 0.001f));
		ASSERT_THAT(IsNear(10.f, Result.CurrentAmount, 0.001f));
		ASSERT_THAT(IsNear(10.f, Result.MissingAmount, 0.001f));
		ASSERT_THAT(IsTrue(Result.Reason == EArcCostFailReason::Insufficient));
	}

	TEST_METHOD(Energy_ThreeStacks_MultiplierCompoundsPerStack)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Energy"));
		ASSERT_THAT(IsNotNull(Ability));
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeItem(TEXT("ID_Spell_Cascade2"), Item)));
		ArcRPGMechanicsTestHarness::AddCascadeStacks(Item, 3);

		ArcRPGMechanicsTestHarness::FCasterOptions Caster;
		Caster.Energy = 100.f;
		const FMassEntityHandle Entity = Harness.CreateCaster(Caster);

		FArcCostCheckResult Result;
		const FArcRPGCost_Ability* AbilityCost = ArcRPGItemCostTestsPrivate::FindCost<FArcRPGCost_Ability>(*Ability);
		ASSERT_THAT(IsNotNull(AbilityCost));
		ASSERT_THAT(IsFalse(AbilityCost->CheckCost(GrantWithItem(Ability, Entity, Item), &Result)));
		ASSERT_THAT(IsNear(160.f, Result.RequiredAmount, 0.001f));
		ASSERT_THAT(IsNear(60.f, Result.MissingAmount, 0.001f));
	}

	TEST_METHOD(Energy_MultiplierBelowOne_TreatedAsOne)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Energy"));
		ASSERT_THAT(IsNotNull(Ability));
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeItem(TEXT("ID_Spell_Cascade0_5"), Item)));
		ArcRPGMechanicsTestHarness::AddCascadeStacks(Item, 2);

		const FMassEntityHandle Entity = Harness.CreateCaster();

		const FArcRPGCost_Ability* AbilityCost = ArcRPGItemCostTestsPrivate::FindCost<FArcRPGCost_Ability>(*Ability);
		ASSERT_THAT(IsNotNull(AbilityCost));
		const FArcMassAbilityCostContext Context = GrantWithItem(Ability, Entity, Item);
		ASSERT_THAT(IsTrue(AbilityCost->CheckCost(Context)));

		AbilityCost->ApplyCost(Context);
		Harness.DrainPendingOps(Entity);
		ASSERT_THAT(IsNear(80.f, Harness.GetEnergy(Entity)->Energy.CurrentValue, 0.001f));
	}

	TEST_METHOD(Energy_ApplyDebitsComputedAmountThroughMeta)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Energy"));
		ASSERT_THAT(IsNotNull(Ability));
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeItem(TEXT("ID_Spell_Cascade2"), Item)));
		ArcRPGMechanicsTestHarness::AddCascadeStacks(Item, 2);

		ArcRPGMechanicsTestHarness::FCasterOptions Caster;
		Caster.Energy = 100.f;
		const FMassEntityHandle Entity = Harness.CreateCaster(Caster);

		const FArcRPGCost_Ability* AbilityCost = ArcRPGItemCostTestsPrivate::FindCost<FArcRPGCost_Ability>(*Ability);
		ASSERT_THAT(IsNotNull(AbilityCost));
		AbilityCost->ApplyCost(GrantWithItem(Ability, Entity, Item));
		Harness.DrainPendingOps(Entity);

		ASSERT_THAT(IsNear(20.f, Harness.GetEnergy(Entity)->Energy.CurrentValue, 0.001f));
	}

	TEST_METHOD(Stamina_RequiresAndDebitsStaminaCost)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Stamina"));
		ASSERT_THAT(IsNotNull(Ability));
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeItem(TEXT("ID_Spell_Stamina"), Item)));

		const FMassEntityHandle Entity = Harness.CreateCaster();
		const FArcMassAbilityCostContext Context = GrantWithItem(Ability, Entity, Item);
		const FArcRPGCost_Ability* Cost = ArcRPGItemCostTestsPrivate::FindCost<FArcRPGCost_Ability>(*Ability);
		ASSERT_THAT(IsNotNull(Cost));

		ASSERT_THAT(IsTrue(Cost->CheckCost(Context)));

		Cost->ApplyCost(Context);
		Harness.DrainPendingOps(Entity);
		ASSERT_THAT(IsNear(85.f, Harness.GetStamina(Entity)->Stamina.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(100.f, Harness.GetEnergy(Entity)->Energy.CurrentValue, 0.001f));
	}

	TEST_METHOD(Health_RequiresAndDebitsHealthCost)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Health"));
		ASSERT_THAT(IsNotNull(Ability));
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeItem(TEXT("ID_Spell_EnergyHealth"), Item)));

		ArcRPGMechanicsTestHarness::FCasterOptions Caster;
		Caster.Health = 10.f;
		const FMassEntityHandle Entity = Harness.CreateCaster(Caster);

		FArcCostCheckResult Result;
		const FArcRPGCost_Ability* AbilityCost = ArcRPGItemCostTestsPrivate::FindCost<FArcRPGCost_Ability>(*Ability);
		ASSERT_THAT(IsNotNull(AbilityCost));
		ASSERT_THAT(IsFalse(AbilityCost->CheckCost(GrantWithItem(Ability, Entity, Item), &Result)));
		ASSERT_THAT(IsNear(12.f, Result.RequiredAmount, 0.001f));
		ASSERT_THAT(IsNear(10.f, Result.CurrentAmount, 0.001f));
	}

	TEST_METHOD(EntropyGenerated_PassesWithoutSufficiencyAndAddsToEntropy)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Entropy"));
		ASSERT_THAT(IsNotNull(Ability));
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeItem(TEXT("ID_Spell_Entropy"), Item)));

		ArcRPGMechanicsTestHarness::FCasterOptions Caster;
		Caster.Entropy = 0.f;
		const FMassEntityHandle Entity = Harness.CreateCaster(Caster);
		const FArcMassAbilityCostContext Context = GrantWithItem(Ability, Entity, Item);
		const FArcRPGCost_Ability* Cost = ArcRPGItemCostTestsPrivate::FindCost<FArcRPGCost_Ability>(*Ability);
		ASSERT_THAT(IsNotNull(Cost));

		ASSERT_THAT(IsTrue(Cost->CheckCost(Context)));

		Cost->ApplyCost(Context);
		Harness.DrainPendingOps(Entity);
		ASSERT_THAT(IsNear(15.f, Harness.GetEntropy(Entity)->Entropy.CurrentValue, 0.001f));
	}

	TEST_METHOD(GetEntropyGenerated_ReadsTheAbilityEntropyCost)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Entropy"));
		ASSERT_THAT(IsNotNull(Ability));
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeItem(TEXT("ID_Spell_Entropy"), Item)));

		const FMassEntityHandle Entity = Harness.CreateCaster();
		const FArcMassAbilityCostContext Context = GrantWithItem(Ability, Entity, Item);

		ASSERT_THAT(IsNear(15.f, ArcRPGItemCost::GetEntropyGenerated(*Harness.EntityManager, Entity, Context.AbilityHandle), 0.001f));
	}

	TEST_METHOD(CascadeResetCooldown_ArmsTimer)
	{
		UArcAbilityDefinition* Ability = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Energy"));
		ASSERT_THAT(IsNotNull(Ability));
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeItem(TEXT("ID_Spell_Cascade2"), Item)));

		const FMassEntityHandle Entity = Harness.CreateCaster();
		const FArcMassAbilityCostContext Context = GrantWithItem(Ability, Entity, Item);
		Harness.World->TimeSeconds = 42.5;

		float AppliedDuration = 0.f;
		FArcRPGCooldown_CascadeReset Cooldown;
		Cooldown.ApplyCooldown(*Harness.EntityManager, Entity, Context.AbilityHandle, &AppliedDuration);

		ASSERT_THAT(IsNear(5.f, AppliedDuration, 0.001f));
		ASSERT_THAT(AreEqual(1, Harness.CountCooldownEntries(Entity)));
	}
};

TEST_CLASS(ArcRPGCost_Cascade, "ArcRPGMechanics.Costs.Cascade")
{
	FActorTestSpawner Spawner;
	ArcRPGMechanicsTestHarness::FHarness Harness;
	UArcAbilityDefinition* Definition = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));

		Definition = NewObject<UArcAbilityDefinition>();
		Definition->StateTree = ArcRPGItemCostTestsPrivate::MakeCommitThenAddStackTree(Harness.World);
		UArcAbilityDefinition* RPGCostEnergy = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Energy"));
		ASSERT_THAT(IsNotNull(RPGCostEnergy));
		Definition->Cost = RPGCostEnergy->Cost;
		Definition->Cooldown = FInstancedStruct::Make(FArcRPGCooldown_CascadeReset());
	}

	FMassEntityHandle SetUp(FArcItemData& Item)
	{
		const FArcItemDefinitionData* Cascade2 = ArcRPGMechanicsTestHarness::LoadItem(TEXT("ID_Spell_Cascade2"));
		if (!Cascade2)
		{
			return FMassEntityHandle();
		}
		ArcRPGMechanicsTestHarness::MakeItem(Cascade2, Item);

		ArcRPGMechanicsTestHarness::FCasterOptions Caster;
		Caster.Energy = 1000.f;
		const FMassEntityHandle Entity = Harness.CreateCaster(Caster);
		ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Definition, ArcRPGMechanicsTestTags::Input,
			FMassEntityHandle(), ArcRPGMechanicsTestHarness::MakeItemCustomData(Item));
		return Entity;
	}

	void ActivateAndDrain(FMassEntityHandle Entity)
	{
		Harness.Activate(Entity, ArcRPGMechanicsTestTags::Input);
		Harness.DrainPendingOps(Entity);
	}

	TEST_METHOD(EachActivation_PaysCurrentStackCostThenAddsAStack)
	{
		FArcItemData Item;
		const FMassEntityHandle Entity = SetUp(Item);
		ASSERT_THAT(IsTrue(Entity.IsValid()));
		ASSERT_THAT(AreEqual(0, ArcRPGMechanicsTestHarness::GetCascadeStacks(Item)));

		ActivateAndDrain(Entity);
		ASSERT_THAT(IsNear(980.f, Harness.GetEnergy(Entity)->Energy.CurrentValue, 0.001f));
		ASSERT_THAT(AreEqual(1, ArcRPGMechanicsTestHarness::GetCascadeStacks(Item)));

		ActivateAndDrain(Entity);
		ASSERT_THAT(IsNear(940.f, Harness.GetEnergy(Entity)->Energy.CurrentValue, 0.001f));
		ASSERT_THAT(AreEqual(2, ArcRPGMechanicsTestHarness::GetCascadeStacks(Item)));

		ActivateAndDrain(Entity);
		ASSERT_THAT(IsNear(860.f, Harness.GetEnergy(Entity)->Energy.CurrentValue, 0.001f));
		ASSERT_THAT(AreEqual(3, ArcRPGMechanicsTestHarness::GetCascadeStacks(Item)));
	}

	TEST_METHOD(SoftCooldownExpiry_ResetsStacksAndCost)
	{
		FArcItemData Item;
		const FMassEntityHandle Entity = SetUp(Item);
		ASSERT_THAT(IsTrue(Entity.IsValid()));
		ActivateAndDrain(Entity);
		ActivateAndDrain(Entity);
		ASSERT_THAT(AreEqual(2, ArcRPGMechanicsTestHarness::GetCascadeStacks(Item)));
		ASSERT_THAT(AreEqual(1, Harness.CountCooldownEntries(Entity)));

		Harness.TickCooldowns(2.f);
		ASSERT_THAT(AreEqual(2, ArcRPGMechanicsTestHarness::GetCascadeStacks(Item)));

		Harness.TickCooldowns(4.f);
		ASSERT_THAT(AreEqual(0, ArcRPGMechanicsTestHarness::GetCascadeStacks(Item)));
		ASSERT_THAT(AreEqual(0, Harness.CountCooldownEntries(Entity)));

		ActivateAndDrain(Entity);
		ASSERT_THAT(IsNear(920.f, Harness.GetEnergy(Entity)->Energy.CurrentValue, 0.001f));
		ASSERT_THAT(AreEqual(1, ArcRPGMechanicsTestHarness::GetCascadeStacks(Item)));
	}
};

// The one RPG ability cost ships the magic.yaml channeling and weaving defaults, set in its constructor.
TEST_CLASS(ArcRPGCost_Defaults, "ArcRPGMechanics.Costs.Defaults")
{
	TEST_METHOD(AbilityCostCarriesTheMagicYamlFactorDefaults)
	{
		const FArcRPGCost_Ability Cost;

		ASSERT_THAT(IsTrue(Cost.EnergyChanneling.Efficiency.Skill == FArcAttributeSet_RPGSkills::GetChannelingAttribute()));
		ASSERT_THAT(IsNear(0.4f, Cost.EnergyChanneling.Floor, 0.0001f));
		ASSERT_THAT(IsNear(0.005f, Cost.EnergyChanneling.SkillK, 0.0001f));
		ASSERT_THAT(IsTrue(Cost.EnergyChanneling.Attribute == FArcAttributeSet_RPGCoreStats::GetIntelligenceAttribute()));
		ASSERT_THAT(IsNear(0.04f, Cost.EnergyChanneling.AttributeK, 0.0001f));

		ASSERT_THAT(IsTrue(Cost.EntropyChanneling.Efficiency.Skill == FArcAttributeSet_RPGSkills::GetChannelingAttribute()));
		ASSERT_THAT(IsNear(0.5f, Cost.EntropyChanneling.Floor, 0.0001f));
		ASSERT_THAT(IsNear(0.003f, Cost.EntropyChanneling.SkillK, 0.0001f));
		ASSERT_THAT(IsTrue(Cost.EntropyChanneling.Attribute == FArcAttributeSet_RPGCoreStats::GetWillpowerAttribute()));
		ASSERT_THAT(IsNear(0.03f, Cost.EntropyChanneling.AttributeK, 0.0001f));

		ASSERT_THAT(IsTrue(Cost.Weaving.Efficiency.Skill == FArcAttributeSet_RPGSkills::GetWeavingAttribute()));
		ASSERT_THAT(IsNear(1.f, Cost.Weaving.Floor, 0.0001f));
		ASSERT_THAT(IsNear(1.5f, Cost.Weaving.Start, 0.0001f));
		ASSERT_THAT(IsNear(0.005f, Cost.Weaving.SkillK, 0.0001f));
	}
};
