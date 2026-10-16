// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassMechanicsAbilityHarness.h"
#include "ArcMassMechanicsTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcMassMechanics/Tiers/ArcAbilityTask_EvalThresholdTier.h"
#include "ArcMassMechanics/Tiers/ArcTierTypes.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectTypes.h"
#include "Fragments/ArcAbilityCustomData.h"
#include "Items/ArcItemData.h"
#include "Mass/EntityManager.h"
#include "NativeGameplayTags.h"
#include "Sources/ArcAbilitySource_Item.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StructUtils/InstancedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TierTaskTest_Input, "Test.MassMechanics.TierTask.Input");

namespace ArcThresholdTierTaskTestsPrivate
{
	struct FTierTaskParams
	{
		FArcAttributeRef ValueAttribute = FArcMechTest_CostSet::GetTierValueAttribute();
		FArcAttributeRef ThresholdAttribute = FArcMechTest_CostSet::GetTierThresholdAttribute();
		UArcTierEffectTable* Table = nullptr;
		FGameplayTag Key = ArcMassMechanicsTestTags::Key_A.GetTag();
	};

	FArcTierBands MakeBands()
	{
		FArcTierBands Bands;
		Bands.Thresholds = {16.f, 17.f, 18.f};
		return Bands;
	}

	UArcTierEffectTable* LoadTaskTable()
	{
		return ArcMechTest::LoadTestAsset<UArcTierEffectTable>(TEXT("Tiers/DA_TierTable_Task"));
	}

	template <typename TaskType>
	TStateTreeEditorNode<TaskType>& BeginTree(UStateTree& StateTree)
	{
		UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(&StateTree);
		StateTree.EditorData = EditorData;
		EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

		UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
		Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

		UStateTreeState& Run = Root.AddChildState(FName(TEXT("EvalTier")));
		Run.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);
		return Run.AddTask<TaskType>();
	}

	void ApplyParams(FArcAbilityTask_EvalThresholdTier_InstanceData& InstanceData, const FTierTaskParams& Params)
	{
		InstanceData.ValueAttribute = Params.ValueAttribute;
		InstanceData.ThresholdAttribute = Params.ThresholdAttribute;
		InstanceData.Bands = MakeBands();
		InstanceData.Table = Params.Table;
		InstanceData.Key = Params.Key;
	}

	void CompileTree(UStateTree& StateTree)
	{
		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bCompiled = Compiler.Compile(StateTree);
		ensureAlwaysMsgf(bCompiled, TEXT("Threshold tier test tree failed to compile"));
	}

	UStateTree* MakeBaseTree(UObject* Outer, const FTierTaskParams& Params)
	{
		UStateTree* StateTree = NewObject<UStateTree>(Outer);
		TStateTreeEditorNode<FArcAbilityTask_EvalThresholdTier>& Task = BeginTree<FArcAbilityTask_EvalThresholdTier>(*StateTree);
		ApplyParams(Task.GetInstanceData(), Params);
		CompileTree(*StateTree);
		return StateTree;
	}
}

TEST_CLASS(ArcThresholdTierTask, "ArcMassMechanics.Tiers.ThresholdTierTask")
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

	FMassEntityHandle GrantAndActivate(UStateTree* StateTree, float TierValue, float TierThreshold,
		FArcAbilityCustomData CustomData = FArcAbilityCustomData())
	{
		ArcMassMechanicsAbilityTest::FCostEntityOptions Options;
		Options.TierValue = TierValue;
		Options.TierThreshold = TierThreshold;
		const FMassEntityHandle Entity = Harness.CreateEntity(Options);

		Definition->StateTree = StateTree;
		ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Definition, TAG_TierTaskTest_Input, FMassEntityHandle(), MoveTemp(CustomData));
		Harness.Activate(Entity, TAG_TierTaskTest_Input);
		return Entity;
	}

	TEST_METHOD(Magnitude_IsValueMinusThreshold)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		using namespace ArcMassMechanicsTest;
		FTierTaskParams Params;
		Params.Table = LoadTaskTable();
		ASSERT_THAT(IsNotNull(Params.Table));

		GrantAndActivate(MakeBaseTree(Harness.World, Params), 22.f, 5.f);

		ASSERT_THAT(AreEqual(1, AppliedCounts[SlotTaskTier2]));
		ASSERT_THAT(AreEqual(1, TotalApplied()));
	}

	TEST_METHOD(UnsetThreshold_MagnitudeIsValue)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		using namespace ArcMassMechanicsTest;
		FTierTaskParams Params;
		Params.ThresholdAttribute = FArcAttributeRef();
		Params.Table = LoadTaskTable();
		ASSERT_THAT(IsNotNull(Params.Table));

		GrantAndActivate(MakeBaseTree(Harness.World, Params), 16.5f, 100.f);

		ASSERT_THAT(AreEqual(1, AppliedCounts[SlotTaskTier1]));
		ASSERT_THAT(AreEqual(1, TotalApplied()));
	}

	TEST_METHOD(BelowFirstBand_AppliesTierZeroEntry)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		using namespace ArcMassMechanicsTest;
		FTierTaskParams Params;
		Params.Table = LoadTaskTable();
		ASSERT_THAT(IsNotNull(Params.Table));

		GrantAndActivate(MakeBaseTree(Harness.World, Params), 13.f, 0.f);

		ASSERT_THAT(AreEqual(1, AppliedCounts[SlotTaskTier0]));
		ASSERT_THAT(AreEqual(1, TotalApplied()));
	}

	TEST_METHOD(BaseTask_UsesInstanceKey)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		using namespace ArcMassMechanicsTest;
		FTierTaskParams Params;
		Params.Table = LoadTaskTable();
		ASSERT_THAT(IsNotNull(Params.Table));
		Params.Key = ArcMassMechanicsTestTags::Key_B.GetTag();

		GrantAndActivate(MakeBaseTree(Harness.World, Params), 17.5f, 0.5f);

		ASSERT_THAT(AreEqual(1, AppliedCounts[SlotTaskDecoyKey]));
		ASSERT_THAT(AreEqual(1, TotalApplied()));
	}

	TEST_METHOD(InstantEntry_AppliedOnceWithoutStoredEffect)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		using namespace ArcMassMechanicsTest;
		FTierTaskParams Params;
		Params.Table = LoadTaskTable();
		ASSERT_THAT(IsNotNull(Params.Table));

		const FMassEntityHandle Entity = GrantAndActivate(MakeBaseTree(Harness.World, Params), 20.f, 0.f);

		ASSERT_THAT(AreEqual(1, AppliedCounts[SlotTaskTier3]));
		ASSERT_THAT(AreEqual(1, TotalApplied()));
		ASSERT_THAT(AreEqual(0, Harness.CountActiveEffects(Entity)));
	}

	TEST_METHOD(DurationEntry_AppliedOnceAndOutlivesTheAbility)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		using namespace ArcMassMechanicsTest;
		FTierTaskParams Params;
		Params.Table = ArcMechTest::LoadTestAsset<UArcTierEffectTable>(TEXT("Tiers/DA_TierTable_TaskDuration"));
		ASSERT_THAT(IsNotNull(Params.Table));

		const FMassEntityHandle Entity = GrantAndActivate(MakeBaseTree(Harness.World, Params), 20.f, 0.f);

		ASSERT_THAT(AreEqual(1, AppliedCounts[SlotTaskTier3]));
		ASSERT_THAT(AreEqual(1, TotalApplied()));
		ASSERT_THAT(AreEqual(0, TotalRemoved()));
		ASSERT_THAT(AreEqual(1, Harness.CountActiveEffects(Entity)));
	}

	TEST_METHOD(MissingTable_AppliesNothing)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		FTierTaskParams Params;

		GrantAndActivate(MakeBaseTree(Harness.World, Params), 20.f, 0.f);

		ASSERT_THAT(AreEqual(0, ArcMassMechanicsTest::TotalApplied()));
	}

	TEST_METHOD(MissingEntryForTier_AppliesNothing)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		FTierTaskParams Params;
		Params.Table = LoadTaskTable();
		ASSERT_THAT(IsNotNull(Params.Table));
		Params.Key = ArcMassMechanicsTestTags::Key_B.GetTag();

		GrantAndActivate(MakeBaseTree(Harness.World, Params), 20.f, 0.f);

		ASSERT_THAT(AreEqual(0, ArcMassMechanicsTest::TotalApplied()));
	}

	TEST_METHOD(UnresolvedValueAttribute_AppliesNothing)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		FTierTaskParams Params;
		Params.ValueAttribute = FArcMechTest_ResourceSet::GetResourceAAttribute();
		Params.Table = LoadTaskTable();
		ASSERT_THAT(IsNotNull(Params.Table));

		GrantAndActivate(MakeBaseTree(Harness.World, Params), 20.f, 0.f);

		ASSERT_THAT(AreEqual(0, ArcMassMechanicsTest::TotalApplied()));
	}

	TEST_METHOD(UnresolvedThresholdAttribute_AppliesNothing)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		FTierTaskParams Params;
		Params.ThresholdAttribute = FArcMechTest_ResourceSet::GetResourceAAttribute();
		Params.Table = LoadTaskTable();
		ASSERT_THAT(IsNotNull(Params.Table));

		GrantAndActivate(MakeBaseTree(Harness.World, Params), 20.f, 0.f);

		ASSERT_THAT(AreEqual(0, ArcMassMechanicsTest::TotalApplied()));
	}

	TEST_METHOD(AppliedEffect_CarriesTheAbilitySourceItem)
	{
		using namespace ArcThresholdTierTaskTestsPrivate;
		using namespace ArcMassMechanicsTest;
		FTierTaskParams Params;
		Params.Table = LoadTaskTable();
		ASSERT_THAT(IsNotNull(Params.Table));

		FArcItemData Item;
		FArcAbilitySource_Item ItemSource;
		ItemSource.ItemDataHandle = FArcItemDataHandle(&Item);
		FArcAbilityCustomData CustomData;
		CustomData.AddOrReplace<FArcAbilitySource_Item>(MoveTemp(ItemSource));

		GrantAndActivate(MakeBaseTree(Harness.World, Params), 20.f, 0.f, MoveTemp(CustomData));

		ASSERT_THAT(AreEqual(1, SourceProbeCount));
		ASSERT_THAT(IsTrue(LastProbedSourceItem == &Item));
	}
};
