// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcRPGMechanicsTestHarness.h"
#include "ArcRPGMechanicsTestTypes.h"
#include "ArcMassMechanics/Tiers/ArcTierTypes.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"
#include "Abilities/Tasks/ArcAbilityTask_Commit.h"
#include "Abilities/Tasks/ArcRPGAbilityTask_SurgeTier.h"
#include "Attributes/ArcAttributeSet_RPGEntropy.h"
#include "Components/ActorTestSpawner.h"
#include "Fragments/ArcAbilityCustomData.h"
#include "Items/ArcItemData.h"
#include "Mass/EntityManager.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"

namespace ArcRPGSurgeTierTestsPrivate
{
	UArcTierEffectTable* LoadSurgeTable()
	{
		return ArcRPGMechTest::LoadTestAsset<UArcTierEffectTable>(TEXT("Surge/DA_SurgeTable_Test"));
	}

	UStateTree* MakeSurgeTree(UObject* Outer, UArcTierEffectTable* Table, bool bCommitAfterSurge)
	{
		UStateTree* StateTree = NewObject<UStateTree>(Outer);
		UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
		StateTree->EditorData = EditorData;
		EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

		UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
		Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

		UStateTreeState& Run = Root.AddChildState(FName(TEXT("Surge")));
		TStateTreeEditorNode<FArcRPGAbilityTask_SurgeTier>& Surge = Run.AddTask<FArcRPGAbilityTask_SurgeTier>();
		FArcRPGAbilityTask_SurgeTier_InstanceData& InstanceData = Surge.GetInstanceData();
		InstanceData.ValueAttribute = FArcAttributeSet_RPGEntropy::GetEntropyAttribute();
		InstanceData.ThresholdAttribute = FArcAttributeSet_RPGEntropy::GetThresholdEntropyAttribute();
		InstanceData.Bands.Thresholds = {10.f, 20.f, 30.f};
		InstanceData.Table = Table;
		if (bCommitAfterSurge)
		{
			Run.AddTask<FArcAbilityTask_Commit>();
		}
		Run.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bCompiled = Compiler.Compile(*StateTree);
		ensureAlwaysMsgf(bCompiled, TEXT("MakeSurgeTree failed to compile"));
		return StateTree;
	}
}

TEST_CLASS(ArcRPGSurgeTier, "ArcRPGMechanics.Surge.Tier")
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
	}

	FMassEntityHandle CreateCaster(float Entropy, float ThresholdEntropy)
	{
		ArcRPGMechanicsTestHarness::FCasterOptions Caster;
		Caster.Entropy = Entropy;
		Caster.ThresholdEntropy = ThresholdEntropy;
		return Harness.CreateCaster(Caster);
	}

	bool MakeSpell(FArcItemData& OutItem, const TCHAR* ItemName = TEXT("ID_Spell_Entropy"))
	{
		const FArcItemDefinitionData* SpellEntropy = ArcRPGMechanicsTestHarness::LoadItem(ItemName);
		if (!SpellEntropy)
		{
			return false;
		}
		ArcRPGMechanicsTestHarness::MakeItem(SpellEntropy, OutItem);
		return true;
	}

	TEST_METHOD(TierFromEntropyPlusItemEntropyMinusThreshold_KeyedByDomainTag)
	{
		using namespace ArcRPGMechanicsTest;
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeSpell(Item)));
		const FMassEntityHandle Entity = CreateCaster(30.f, 20.f);
		UArcTierEffectTable* SurgeTable = ArcRPGSurgeTierTestsPrivate::LoadSurgeTable();
		ASSERT_THAT(IsNotNull(SurgeTable));
		Definition->StateTree = ArcRPGSurgeTierTestsPrivate::MakeSurgeTree(Harness.World, SurgeTable, false);
		UArcAbilityDefinition* RPGCostEntropy = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Entropy"));
		ASSERT_THAT(IsNotNull(RPGCostEntropy));
		Definition->Cost = RPGCostEntropy->Cost;
		ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Definition, ArcRPGMechanicsTestTags::Input,
			FMassEntityHandle(), ArcRPGMechanicsTestHarness::MakeItemCustomData(Item));

		Harness.Activate(Entity, ArcRPGMechanicsTestTags::Input);

		ASSERT_THAT(AreEqual(1, AppliedCounts[SlotDomainATier2]));
		ASSERT_THAT(AreEqual(1, TotalApplied()));
	}

	TEST_METHOD(OtherDomainItem_KeysTheTableByItsOwnDomainTag)
	{
		using namespace ArcRPGMechanicsTest;
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeSpell(Item, TEXT("ID_Spell_EntropyB"))));
		const FMassEntityHandle Entity = CreateCaster(30.f, 20.f);
		UArcTierEffectTable* SurgeTable = ArcRPGSurgeTierTestsPrivate::LoadSurgeTable();
		ASSERT_THAT(IsNotNull(SurgeTable));
		Definition->StateTree = ArcRPGSurgeTierTestsPrivate::MakeSurgeTree(Harness.World, SurgeTable, false);
		UArcAbilityDefinition* RPGCostEntropy = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Entropy"));
		ASSERT_THAT(IsNotNull(RPGCostEntropy));
		Definition->Cost = RPGCostEntropy->Cost;
		ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Definition, ArcRPGMechanicsTestTags::Input,
			FMassEntityHandle(), ArcRPGMechanicsTestHarness::MakeItemCustomData(Item));

		Harness.Activate(Entity, ArcRPGMechanicsTestTags::Input);

		ASSERT_THAT(AreEqual(1, AppliedCounts[SlotDomainBTier2]));
		ASSERT_THAT(AreEqual(0, AppliedCounts[SlotDomainATier2]));
		ASSERT_THAT(AreEqual(1, TotalApplied()));
	}

	TEST_METHOD(NoSourceItem_NoKey_AppliesNothing)
	{
		const FMassEntityHandle Entity = CreateCaster(25.f, 0.f);
		UArcTierEffectTable* SurgeTable = ArcRPGSurgeTierTestsPrivate::LoadSurgeTable();
		ASSERT_THAT(IsNotNull(SurgeTable));
		Definition->StateTree = ArcRPGSurgeTierTestsPrivate::MakeSurgeTree(Harness.World, SurgeTable, false);
		ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Definition, ArcRPGMechanicsTestTags::Input);

		Harness.Activate(Entity, ArcRPGMechanicsTestTags::Input);

		ASSERT_THAT(AreEqual(0, ArcRPGMechanicsTest::TotalApplied()));
	}

	TEST_METHOD(SurgeAndEntropyCost_ShareTheItemEntropyGenerated)
	{
		using namespace ArcRPGMechanicsTest;
		FArcItemData Item;
		ASSERT_THAT(IsTrue(MakeSpell(Item)));
		const FMassEntityHandle Entity = CreateCaster(30.f, 20.f);
		UArcTierEffectTable* SurgeTable = ArcRPGSurgeTierTestsPrivate::LoadSurgeTable();
		ASSERT_THAT(IsNotNull(SurgeTable));
		Definition->StateTree = ArcRPGSurgeTierTestsPrivate::MakeSurgeTree(Harness.World, SurgeTable, true);
		UArcAbilityDefinition* RPGCostEntropy = ArcRPGMechanicsTestHarness::LoadCostAbility(TEXT("AD_RPGCost_Entropy"));
		ASSERT_THAT(IsNotNull(RPGCostEntropy));
		Definition->Cost = RPGCostEntropy->Cost;
		ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Definition, ArcRPGMechanicsTestTags::Input,
			FMassEntityHandle(), ArcRPGMechanicsTestHarness::MakeItemCustomData(Item));

		Harness.Activate(Entity, ArcRPGMechanicsTestTags::Input);
		Harness.DrainPendingOps(Entity);

		ASSERT_THAT(AreEqual(1, AppliedCounts[SlotDomainATier2]));
		ASSERT_THAT(IsNear(45.f, Harness.GetEntropy(Entity)->Entropy.CurrentValue, 0.001f));

		Harness.Activate(Entity, ArcRPGMechanicsTestTags::Input);
		Harness.DrainPendingOps(Entity);

		ASSERT_THAT(AreEqual(1, AppliedCounts[SlotDomainATier3]));
		ASSERT_THAT(AreEqual(2, TotalApplied()));
		ASSERT_THAT(IsNear(60.f, Harness.GetEntropy(Entity)->Entropy.CurrentValue, 0.001f));
	}
};
