// Copyright Lukasz Baran. All Rights Reserved.
//
// CQTest coverage for the signed resource rate and the Pips rate (Plan 13 Step 4, D-100). Expected values come from
// the plan's case table. Every test ticks the production resource processor and the pending-ops drain and asserts the
// resource attribute written through the Delta meta handler. Topology: standalone world (no replication involved).

#include "CQTest.h"
#include "ArcMassMechanicsTestHarness.h"
#include "ArcMassMechanicsTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Components/ActorTestSpawner.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcResourceRateTestsPrivate
{
	UArcResourceConfig* MakeSignedConfig(ArcMassMechanicsTest::FHarness& Harness, float Rate, float Target, bool bMinTarget = false)
	{
		UArcResourceConfig* Config = Harness.MakeConfig();
		Config->Resources.Add(ArcMassMechanicsTest::MakeResourceA(0.f, 100.f, Target, Rate));
		if (bMinTarget)
		{
			Config->Resources[0].Target = FInstancedStruct::Make(FArcResourceTarget_Min());
		}
		return Config;
	}

	UArcResourceConfig* MakePipsConfig(ArcMassMechanicsTest::FHarness& Harness)
	{
		UArcResourceConfig* Config = Harness.MakeConfig();
		Config->Resources.Add(ArcMassMechanicsTest::MakeResourceA(0.f, 100.f, 100.f, 0.f));
		ArcMassMechanicsTest::SetPipsRate(*Config, 0);
		return Config;
	}

	FArcResourceRate_Pips& PipsRate(UArcResourceConfig& Config)
	{
		FArcResourceRate_Pips* Rate = Config.Resources[0].Rate.GetMutablePtr<FArcResourceRate_Pips>();
		check(Rate);
		return *Rate;
	}

	ArcMassMechanicsTest::FEntityOptions MakeOptions(float Value, float Regen, float Degen, float PerPip)
	{
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.ResourceA = Value;
		Options.RegenPips = Regen;
		Options.DegenPips = Degen;
		Options.PerPip = PerPip;
		return Options;
	}

	FGameplayTagContainer Tags(const FGameplayTag& Tag)
	{
		return FGameplayTagContainer(Tag);
	}
}

TEST_CLASS(ArcResourceSignedRate, "ArcMassMechanics.Resources.SignedRate")
{
	FActorTestSpawner Spawner;
	ArcMassMechanicsTest::FHarness Harness;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	TEST_METHOD(NegativeRate_BelowTarget_StopsAtMin)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakeSignedConfig(Harness, -10.f, 80.f);
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.ResourceA = 25.f;
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, Options);

		const float Expected[] = {15.f, 5.f, 0.f, 0.f};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		{
			Harness.Tick();
			ASSERT_THAT(IsNear(Expected[Index], Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		}
	}

	TEST_METHOD(NegativeRate_ValueAtMaxTarget_MovesDown)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakeSignedConfig(Harness, -10.f, 100.f);
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.ResourceA = 100.f;
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, Options);

		Harness.Tick();
		ASSERT_THAT(IsNear(90.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(NegativeRate_TieAtMinTarget_MovesUp_ThenKeepsMovingAwayFromMin)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakeSignedConfig(Harness, -10.f, 0.f, true);
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.ResourceA = 0.f;
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, Options);

		Harness.Tick();
		ASSERT_THAT(IsNear(10.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		Harness.Tick();
		ASSERT_THAT(IsNear(20.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(NegativeRate_AboveTarget_MovesUpToUpperBound_AndStops)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakeSignedConfig(Harness, -10.f, 50.f);
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.ResourceA = 85.f;
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, Options);

		const float Expected[] = {95.f, 100.f, 100.f};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		{
			Harness.Tick();
			ASSERT_THAT(IsNear(Expected[Index], Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		}
	}

	TEST_METHOD(NegativeRate_ValueBelowMin_NeverMovesTowardTheTarget)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakeSignedConfig(Harness, -10.f, 80.f);
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.ResourceA = -20.f;
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, Options);

		Harness.Tick(1.f, 3);
		ASSERT_THAT(IsNear(-20.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(BlockedBy_StopsAPositiveRate_ThenReleasesWhenTheTagIsRemoved)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakeSignedConfig(Harness, 10.f, 80.f);
		Config->Resources[0].BlockedBy.AddTag(ArcMassMechanicsTestTags::Key_A);
		const FMassEntityHandle Entity = Harness.CreateEntity(Config);

		Harness.Tick();
		ASSERT_THAT(IsNear(10.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));

		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Entity, ArcResourceRateTestsPrivate::Tags(ArcMassMechanicsTestTags::Key_A));
		Harness.Tick(1.f, 3);
		ASSERT_THAT(IsNear(10.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));

		ArcAbilities::RemoveOwnedTags(*Harness.EntityManager, Entity, ArcResourceRateTestsPrivate::Tags(ArcMassMechanicsTestTags::Key_A));
		Harness.Tick();
		ASSERT_THAT(IsNear(20.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(BlockedBy_DoesNotStopANegativeRate)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakeSignedConfig(Harness, -10.f, 80.f);
		Config->Resources[0].BlockedBy.AddTag(ArcMassMechanicsTestTags::Key_A);
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.ResourceA = 50.f;
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, Options);
		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Entity, ArcResourceRateTestsPrivate::Tags(ArcMassMechanicsTestTags::Key_A));
		ASSERT_THAT(AreEqual(1, ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Entity, ArcMassMechanicsTestTags::Key_A)));

		Harness.Tick(1.f, 2);
		ASSERT_THAT(IsNear(30.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(RateMultipliers_ScaleANegativeRate_OnlyWhileTheConditionIsMet)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakeSignedConfig(Harness, -10.f, 80.f);
		FArcResourceRateMultiplier Multiplier;
		Multiplier.Condition = ArcMassMechanicsTestTags::Key_A;
		Multiplier.Multiplier = 0.5f;
		Config->Resources[0].RateMultipliers.Add(Multiplier);
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.ResourceA = 50.f;
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, Options);

		Harness.Tick();
		ASSERT_THAT(IsNear(40.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));

		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Entity, ArcResourceRateTestsPrivate::Tags(ArcMassMechanicsTestTags::Key_A));
		Harness.Tick();
		ASSERT_THAT(IsNear(35.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}
};

TEST_CLASS(ArcResourcePipsRate, "ArcMassMechanics.Resources.Pips")
{
	FActorTestSpawner Spawner;
	ArcMassMechanicsTest::FHarness Harness;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	TEST_METHOD(Base_FourPips_AtThreeQuartersPerPip_GivesThreePerSecond)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, 4.f, 0.f, 0.75f));

		Harness.Tick();
		ASSERT_THAT(IsNear(53.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		Harness.Tick(2.f);
		ASSERT_THAT(IsNear(59.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(DegenAboveRegen_DrainsAwayFromTheTarget)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, 4.f, 6.f, 0.75f));

		Harness.Tick();
		ASSERT_THAT(IsNear(48.5f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(DegenAboveRegen_StopsAtMin)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(1.f, 0.f, 6.f, 0.75f));

		Harness.Tick(1.f, 3);
		ASSERT_THAT(IsNear(0.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(RegenIsCappedAtTenPips)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, 15.f, 0.f, 0.75f));

		Harness.Tick();
		ASSERT_THAT(IsNear(57.5f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(DegenIsCappedAtTenPips)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, 0.f, 15.f, 0.75f));

		Harness.Tick();
		ASSERT_THAT(IsNear(42.5f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(FractionalPips_AreFlooredBeforeTheSubtraction)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, 3.7f, 0.9f, 1.f));
		const FMassEntityHandle RoundsUp = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, 3.6f, 0.f, 1.f));
		const FMassEntityHandle NegativeFraction = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, -0.5f, 0.f, 1.f));

		Harness.Tick();
		ASSERT_THAT(IsNear(53.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		// Rounding would give +4; flooring gives +3.
		ASSERT_THAT(IsNear(53.f, Harness.GetSet(RoundsUp)->GetResourceA(), 0.001f));
		// Truncation toward zero would give 0 pips; flooring gives -1.
		ASSERT_THAT(IsNear(49.f, Harness.GetSet(NegativeFraction)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(NegativeRegenPips_DrainPassively)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, -1.f, 0.f, 0.75f));

		Harness.Tick();
		ASSERT_THAT(IsNear(49.25f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(NegativePerPip_CountsAsZero)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		const FMassEntityHandle Regen = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, 4.f, 0.f, -1.f));
		const FMassEntityHandle Degen = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, 0.f, 4.f, -1.f));

		Harness.Tick(1.f, 2);
		ASSERT_THAT(IsNear(50.f, Harness.GetSet(Regen)->GetResourceA(), 0.001f));
		ASSERT_THAT(IsNear(50.f, Harness.GetSet(Degen)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(UnsetRegenOrPerPip_GivesZeroRate)
	{
		UArcResourceConfig* UnsetRegen = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		ArcResourceRateTestsPrivate::PipsRate(*UnsetRegen).RegenPips = FArcAttributeRef();
		const FMassEntityHandle A = Harness.CreateEntity(UnsetRegen, ArcResourceRateTestsPrivate::MakeOptions(50.f, 4.f, 6.f, 0.75f));

		UArcResourceConfig* UnsetPerPip = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		ArcResourceRateTestsPrivate::PipsRate(*UnsetPerPip).PerPip = FArcAttributeRef();
		const FMassEntityHandle B = Harness.CreateEntity(UnsetPerPip, ArcResourceRateTestsPrivate::MakeOptions(50.f, 4.f, 0.f, 0.75f));

		Harness.Tick(1.f, 2);
		ASSERT_THAT(IsNear(50.f, Harness.GetSet(A)->GetResourceA(), 0.001f));
		ASSERT_THAT(IsNear(50.f, Harness.GetSet(B)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(RegenOrPerPipMissingOnTheEntity_GivesZeroRate)
	{
		UArcResourceConfig* MissingRegen = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		ArcResourceRateTestsPrivate::PipsRate(*MissingRegen).RegenPips = FArcMechTest_CostSet::GetManaAttribute();
		const FMassEntityHandle A = Harness.CreateEntity(MissingRegen, ArcResourceRateTestsPrivate::MakeOptions(50.f, 4.f, 6.f, 0.75f));

		UArcResourceConfig* MissingPerPip = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		ArcResourceRateTestsPrivate::PipsRate(*MissingPerPip).PerPip = FArcMechTest_CostSet::GetManaAttribute();
		const FMassEntityHandle B = Harness.CreateEntity(MissingPerPip, ArcResourceRateTestsPrivate::MakeOptions(50.f, 4.f, 0.f, 0.75f));

		Harness.Tick(1.f, 2);
		ASSERT_THAT(IsNear(50.f, Harness.GetSet(A)->GetResourceA(), 0.001f));
		ASSERT_THAT(IsNear(50.f, Harness.GetSet(B)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(UnsetOrMissingDegen_CountsAsZero)
	{
		UArcResourceConfig* UnsetDegen = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		ArcResourceRateTestsPrivate::PipsRate(*UnsetDegen).DegenPips = FArcAttributeRef();
		const FMassEntityHandle A = Harness.CreateEntity(UnsetDegen, ArcResourceRateTestsPrivate::MakeOptions(50.f, 4.f, 6.f, 0.75f));

		UArcResourceConfig* MissingDegen = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		ArcResourceRateTestsPrivate::PipsRate(*MissingDegen).DegenPips = FArcMechTest_CostSet::GetManaAttribute();
		const FMassEntityHandle B = Harness.CreateEntity(MissingDegen, ArcResourceRateTestsPrivate::MakeOptions(50.f, 4.f, 6.f, 0.75f));

		Harness.Tick();
		ASSERT_THAT(IsNear(53.f, Harness.GetSet(A)->GetResourceA(), 0.001f));
		ASSERT_THAT(IsNear(53.f, Harness.GetSet(B)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(BlockedBy_StopsRegenButNotDegen)
	{
		UArcResourceConfig* Config = ArcResourceRateTestsPrivate::MakePipsConfig(Harness);
		Config->Resources[0].BlockedBy.AddTag(ArcMassMechanicsTestTags::Key_A);
		const FMassEntityHandle Regen = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, 4.f, 0.f, 0.75f));
		const FMassEntityHandle Degen = Harness.CreateEntity(Config, ArcResourceRateTestsPrivate::MakeOptions(50.f, 0.f, 4.f, 0.75f));
		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Regen, ArcResourceRateTestsPrivate::Tags(ArcMassMechanicsTestTags::Key_A));
		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Degen, ArcResourceRateTestsPrivate::Tags(ArcMassMechanicsTestTags::Key_A));

		Harness.Tick();
		ASSERT_THAT(IsNear(50.f, Harness.GetSet(Regen)->GetResourceA(), 0.001f));
		ASSERT_THAT(IsNear(47.f, Harness.GetSet(Degen)->GetResourceA(), 0.001f));
	}
};
