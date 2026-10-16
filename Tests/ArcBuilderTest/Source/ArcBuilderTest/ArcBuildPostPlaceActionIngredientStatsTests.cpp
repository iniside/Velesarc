// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcItemTestRecordFiles.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"

#include "ArcBuildPostPlaceAction.h"

#include "ArcBuilderTestHelpers.h"

namespace ArcBuildPostPlaceActionTestHelpers
{
	using namespace ArcBuilderTest::TestHelpers;

	/** Item definition record carrying one FArcItemFragment_ItemStats.DefaultStats entry
	 *  mapping FArcTestAttributeSet_Vitality::Health, Additive, Value. */
	FPrimaryAssetId MakeHealthStatItemDef(FArcItemTestRecordFiles& Records, float Value)
	{
		FArcItemDefinitionData Def;

		FArcItemFragment_ItemStats StatsFrag;
		FArcItemAttributeStat Stat;
		Stat.Type = EArcModType::Additive;
		Stat.Value = FArcScalableFloat(Value);
		Stat.Stat = FArcTestAttributeSet_Vitality::GetHealthAttribute();
		StatsFrag.DefaultStats.Add(Stat);
		ArcItemTestRecords::AddFragment(Def, StatsFrag);

		return Records.Write(Def);
	}

	/** FArcItemSpec referencing DefId with a fresh item id and the given consumed stack count. */
	FArcItemSpec MakeConsumedSpec(const FPrimaryAssetId& DefId, int32 Stacks)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(DefId);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(Stacks);
		return Spec;
	}

	FArcBuildStatMapping MakeMapping(EArcBuildStatAgg Agg, float Scale)
	{
		FArcBuildStatMapping Mapping;
		Mapping.SourceItemStat = FArcTestAttributeSet_Vitality::GetHealthAttribute();
		Mapping.TargetAttribute = FArcTestAttributeSet_Vitality::GetHealthAttribute();
		Mapping.Agg = Agg;
		Mapping.Scale = Scale;
		return Mapping;
	}

	const FArcTestAttributeSet_Vitality* FindHealthOverride(const TArray<FInstancedStruct>& Overrides)
	{
		for (const FInstancedStruct& Override : Overrides)
		{
			if (Override.GetScriptStruct() == FArcTestAttributeSet_Vitality::StaticStruct())
			{
				return Override.GetPtr<FArcTestAttributeSet_Vitality>();
			}
		}
		return nullptr;
	}
}

TEST_CLASS(ArcBuildPostPlaceActionIngredientStats, "ArcBuilder.PostPlaceAction.IngredientStats")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	FArcItemTestRecordFiles Records;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();
	}

	// =========================================================================
	// Sum_WeightsByStacks — Sum aggregation weights each item's stat value by the
	// stacks consumed, then applies Scale.
	// =========================================================================
	TEST_METHOD(Sum_WeightsByStacks)
	{
		using namespace ArcBuildPostPlaceActionTestHelpers;

		const FPrimaryAssetId DefA = MakeHealthStatItemDef(Records, 10.f);
		ASSERT_THAT(IsTrue(DefA.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId DefB = MakeHealthStatItemDef(Records, 5.f);
		ASSERT_THAT(IsTrue(DefB.IsValid(), TEXT("Item record must be written and indexed")));

		TArray<FArcItemSpec> Consumed;
		Consumed.Add(MakeConsumedSpec(DefA, 2)); // 10 * 2 = 20
		Consumed.Add(MakeConsumedSpec(DefB, 3)); // 5 * 3 = 15 -> Sum = 35

		FArcBuildPostPlaceAction_IngredientStats Action;
		Action.Mappings.Add(MakeMapping(EArcBuildStatAgg::Sum, 1.f));

		FArcBuildPlacementContext Ctx;
		Ctx.ConsumedItems = Consumed;

		TArray<FInstancedStruct> Overrides;
		Action.BuildSpawnOverrides(Ctx, Overrides);

		ASSERT_THAT(AreEqual(1, Overrides.Num(), TEXT("Exactly one override fragment (FArcTestAttributeSet_Vitality) must be produced")));
		const FArcTestAttributeSet_Vitality* HealthOverride = FindHealthOverride(Overrides);
		ASSERT_THAT(IsNotNull(HealthOverride));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOverride->Health.GetCurrentValue(), 35.f),
			TEXT("Sum must be stack-weighted: 10*2 + 5*3 = 35")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOverride->Health.GetBaseValue(), 35.f),
			TEXT("BuildSpawnOverrides must seed both Base and Current via InitValue")));
	}

	// =========================================================================
	// Average_IsStackWeighted — Average divides the stack-weighted sum by total
	// stacks consumed, then Scale is applied last.
	// =========================================================================
	TEST_METHOD(Average_IsStackWeighted)
	{
		using namespace ArcBuildPostPlaceActionTestHelpers;

		const FPrimaryAssetId DefA = MakeHealthStatItemDef(Records, 10.f);
		ASSERT_THAT(IsTrue(DefA.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId DefB = MakeHealthStatItemDef(Records, 5.f);
		ASSERT_THAT(IsTrue(DefB.IsValid(), TEXT("Item record must be written and indexed")));

		TArray<FArcItemSpec> Consumed;
		Consumed.Add(MakeConsumedSpec(DefA, 2)); // 20
		Consumed.Add(MakeConsumedSpec(DefB, 3)); // 15 -> Sum=35, TotalStacks=5, Avg=7

		FArcBuildPostPlaceAction_IngredientStats Action;
		Action.Mappings.Add(MakeMapping(EArcBuildStatAgg::Average, 2.f)); // Avg(7) * Scale(2) = 14

		FArcBuildPlacementContext Ctx;
		Ctx.ConsumedItems = Consumed;

		TArray<FInstancedStruct> Overrides;
		Action.BuildSpawnOverrides(Ctx, Overrides);

		const FArcTestAttributeSet_Vitality* HealthOverride = FindHealthOverride(Overrides);
		ASSERT_THAT(IsNotNull(HealthOverride));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOverride->Health.GetCurrentValue(), 14.f),
			TEXT("Average(35/5=7) * Scale(2) = 14")));
	}

	// =========================================================================
	// Max_IgnoresStackCount — Max picks the highest per-item value regardless of
	// stacks consumed.
	// =========================================================================
	TEST_METHOD(Max_IgnoresStackCount)
	{
		using namespace ArcBuildPostPlaceActionTestHelpers;

		const FPrimaryAssetId DefA = MakeHealthStatItemDef(Records, 10.f);
		ASSERT_THAT(IsTrue(DefA.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId DefB = MakeHealthStatItemDef(Records, 50.f);
		ASSERT_THAT(IsTrue(DefB.IsValid(), TEXT("Item record must be written and indexed")));

		TArray<FArcItemSpec> Consumed;
		Consumed.Add(MakeConsumedSpec(DefA, 100)); // huge stack count, still loses on value
		Consumed.Add(MakeConsumedSpec(DefB, 1));

		FArcBuildPostPlaceAction_IngredientStats Action;
		Action.Mappings.Add(MakeMapping(EArcBuildStatAgg::Max, 1.f));

		FArcBuildPlacementContext Ctx;
		Ctx.ConsumedItems = Consumed;

		TArray<FInstancedStruct> Overrides;
		Action.BuildSpawnOverrides(Ctx, Overrides);

		const FArcTestAttributeSet_Vitality* HealthOverride = FindHealthOverride(Overrides);
		ASSERT_THAT(IsNotNull(HealthOverride));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOverride->Health.GetCurrentValue(), 50.f),
			TEXT("Max must ignore stack counts and pick the higher per-item value (50)")));
	}

	// =========================================================================
	// PostPlaced_ReApplies_OnLiveEntity — PostPlaced writes the same computed
	// value onto the live entity's sparse attribute fragment (the re-apply that
	// wins over UArcInitialAttributesObserver's Commit-time preset seeding).
	// =========================================================================
	TEST_METHOD(PostPlaced_ReApplies_OnLiveEntity)
	{
		using namespace ArcBuildPostPlaceActionTestHelpers;

		const FPrimaryAssetId DefA = MakeHealthStatItemDef(Records, 8.f);
		ASSERT_THAT(IsTrue(DefA.IsValid(), TEXT("Item record must be written and indexed")));

		TArray<FArcItemSpec> Consumed;
		Consumed.Add(MakeConsumedSpec(DefA, 4)); // Sum = 32

		FArcBuildPostPlaceAction_IngredientStats Action;
		Action.Mappings.Add(MakeMapping(EArcBuildStatAgg::Sum, 1.f));

		FArcBuildPlacementContext Ctx;
		Ctx.ConsumedItems = Consumed;

		// Live entity carrying the sparse attribute fragment, seeded with an
		// unrelated (stale) value the way UArcInitialAttributesObserver would.
		UE::Mass::FEntityBuilder Builder(*EM);
		FArcTestAttributeSet_Vitality InitialHealth;
		InitialHealth.Health.InitValue(999.f);
		const FMassEntityHandle Placed = Builder
			.Add<FTransformFragment>()
			.Add<FArcTestAttributeSet_Vitality>(InitialHealth)
			.Commit();

		Action.PostPlaced(*EM, Placed, Ctx);

		const FArcTestAttributeSet_Vitality* LiveHealth = EM->GetMutableSparseElementDataForEntity(
			FArcTestAttributeSet_Vitality::StaticStruct(), Placed).GetPtr<FArcTestAttributeSet_Vitality>();
		ASSERT_THAT(IsNotNull(LiveHealth));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(LiveHealth->Health.GetCurrentValue(), 32.f),
			TEXT("PostPlaced must overwrite the stale preset-seeded value with the computed 32")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(LiveHealth->Health.GetBaseValue(), 32.f)));
	}

	// =========================================================================
	// NoMatchingConsumedStat_NoOverrideProduced — none of the consumed items
	// carry the mapped source stat: no override fragment is added.
	// =========================================================================
	TEST_METHOD(NoMatchingConsumedStat_NoOverrideProduced)
	{
		using namespace ArcBuildPostPlaceActionTestHelpers;

		// Item with no FArcItemFragment_ItemStats at all.
		const FPrimaryAssetId PlainDef = Records.Write(FArcItemDefinitionData());
		ASSERT_THAT(IsTrue(PlainDef.IsValid(), TEXT("Item record must be written and indexed")));

		TArray<FArcItemSpec> Consumed;
		Consumed.Add(MakeConsumedSpec(PlainDef, 1));

		FArcBuildPostPlaceAction_IngredientStats Action;
		Action.Mappings.Add(MakeMapping(EArcBuildStatAgg::Sum, 1.f));

		FArcBuildPlacementContext Ctx;
		Ctx.ConsumedItems = Consumed;

		TArray<FInstancedStruct> Overrides;
		Action.BuildSpawnOverrides(Ctx, Overrides);

		ASSERT_THAT(AreEqual(0, Overrides.Num(), TEXT("No consumed item carries the mapped stat — no override fragment should be created")));
	}
};
