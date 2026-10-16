// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcCombatTestHarness.h"
#include "ArcMechTestSupport.h"
#include "ArcRPGCombatTestSupport.h"
#include "ArcRPGMechanicsTags.h"
#include "ArcRPGMechTestSupport.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Attributes/ArcAttributeSet_RPGCoreStats.h"
#include "Attributes/ArcAttributeSet_RPGSkills.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Items/ArcItemData.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassEntitySubsystem.h"
#include "NativeGameplayTags.h"

namespace ArcRPGStatusTestsPrivate
{
	struct FStats
	{
		float Willpower = 10.f;
		float Wisdom = 10.f;
		float Skill = 0.f;
	};

	FMassEntityHandle CreateEntity(FMassEntityManager& EntityManager, const FStats& Stats)
	{
		FArcAttributeSet_RPGCoreStats CoreStats;
		CoreStats.InitWillpower(Stats.Willpower);
		CoreStats.InitWisdom(Stats.Wisdom);

		FArcAttributeSet_RPGSkills Skills;
		Skills.InitOneHanded(Stats.Skill);
		Skills.InitImposition(Stats.Skill);

		UE::Mass::FEntityBuilder Builder(EntityManager);
		return Builder.Add<FArcAttributeSet_RPGCoreStats>(CoreStats)
			.Add<FArcAttributeSet_RPGSkills>(Skills)
			.Add<FArcEffectStackFragment>()
			.Add<FArcAggregatorFragment>()
			.Add<FArcOwnedTagsFragment>()
			.Add<FArcPendingAttributeOpsFragment>()
			.Commit();
	}
}

TEST_CLASS(ArcRPGStatus_StatusDuration, "ArcRPGMechanics.Statuses.StatusDuration")
{
	TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		EntityManager = &EntitySubsystem->GetMutableEntityManager();
	}

	float AppliedDuration(const TCHAR* EffectName, const ArcRPGStatusTestsPrivate::FStats& SourceStats)
	{
		using namespace ArcRPGStatusTestsPrivate;
		UArcEffectDefinition* Effect = ArcRPGCombatTest::LoadStatus(EffectName);
		if (!Effect)
		{
			return -1.f;
		}
		const FMassEntityHandle Source = CreateEntity(*EntityManager, SourceStats);
		const FMassEntityHandle Target = CreateEntity(*EntityManager, {});

		FArcItemData Weapon;
		if (!ArcRPGCombatTest::MakeSkillWeapon(Weapon))
		{
			return -1.f;
		}

		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, Source, ArcRPGCombatTest::MakeItemSource(Weapon));
		FMassEntityView EntityView(*EntityManager, Target);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		const FArcActiveEffect* Active = Stack ? ArcEffects::FindActiveEffect(*Stack, Result.Handle) : nullptr;
		return Active ? Active->RemainingDuration : -1.f;
	}

	TEST_METHOD(Combat_SkillAndWisdomMultiplier)
	{
		const float Result = AppliedDuration(TEXT("ED_RPGStatus_Duration"), {10.f, 16.f, 40.f});
		ASSERT_THAT(IsNear(15.6f, Result, 0.001f));
	}

	TEST_METHOD(Combat_LowWisdomShortens)
	{
		const float Result = AppliedDuration(TEXT("ED_RPGStatus_Duration"), {10.f, 7.f, 0.f});
		ASSERT_THAT(IsNear(8.f, Result, 0.001f));
	}

	TEST_METHOD(World_ConvertsGameHoursWithDefaultSecondsPerHour)
	{
		// The test world has no DaySequence actor, so a game hour resolves to the 120 s default.
		const float Result = AppliedDuration(TEXT("ED_RPGStatus_DurationWorld"), {10.f, 16.f, 40.f});
		ASSERT_THAT(IsNear(3744.f, Result, 0.01f));
	}
};

// Real status lifecycle: Burning is a stored periodic effect that grants ArcCondition.Burning and deals Fire damage on each tick.
TEST_CLASS(ArcRPGStatus_BurningLifecycle, "ArcRPGMechanics.Statuses.Lifecycle")
{
	TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	FActorTestSpawner Spawner;
	ArcCombatTest::FCombatHarness Harness;
	UArcEffectDefinition* Burning = nullptr;
	FMassEntityHandle Attacker;
	FMassEntityHandle Defender;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		ArcRPGCombatTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Burning = ArcRPGCombatTest::LoadStatus(TEXT("ED_RPGStatus_Burning"));
		ASSERT_THAT(IsNotNull(Burning));
		Attacker = Harness.CreateEntity();
		Defender = Harness.CreateEntity();
	}

	int32 BurningTags()
	{
		return ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Defender, ArcMassMechanicsTags::Condition_Burning);
	}

	TEST_METHOD(GrantsTheConditionTag_TicksFireDamage_AndRemovalStopsBoth)
	{
		const FArcEffectApplyResult Applied = Harness.Apply(Burning, Defender, Attacker);
		ASSERT_THAT(IsTrue(Applied.Handle.IsValid()));
		ASSERT_THAT(AreEqual(1, BurningTags()));
		ASSERT_THAT(IsNear(1000.f, Harness.GetHealth(Defender), 0.001f));

		Harness.TickDuration(1.f, Defender);
		ASSERT_THAT(IsNear(996.f, Harness.GetHealth(Defender), 0.001f));
		const FArcDamageDealtPayload* Tick = Harness.FindLastDamage();
		ASSERT_THAT(IsNotNull(Tick));
		ASSERT_THAT(IsTrue(Tick->DamageType == ArcRPGMechanicsTags::Damage_Fire.GetTag()));
		ASSERT_THAT(IsTrue(Tick->bPeriodicTick));

		Harness.TickDuration(1.f, Defender);
		ASSERT_THAT(IsNear(992.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(AreEqual(2, Harness.CountHits()));

		FArcEffectRemovalQuery Removal;
		Removal.Handle = Applied.Handle;
		ArcEffects::RemoveEffects(*Harness.EntityManager, Defender, Removal);
		ASSERT_THAT(AreEqual(0, BurningTags()));
		ASSERT_THAT(AreEqual(0, ArcMechTest::CountActiveEffects(*Harness.EntityManager, Defender, Burning)));

		Harness.TickDuration(1.f, Defender);
		Harness.TickDuration(1.f, Defender);
		ASSERT_THAT(IsNear(992.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(AreEqual(2, Harness.CountHits()));
	}

	TEST_METHOD(ReapplyWhileBurning_ReplacesTheOldInstance_TagStaysSingle)
	{
		const FArcEffectApplyResult First = Harness.Apply(Burning, Defender, Attacker);
		ASSERT_THAT(IsTrue(First.Handle.IsValid()));
		Harness.TickDuration(1.f, Defender);
		Harness.TickDuration(1.f, Defender);
		ASSERT_THAT(IsNear(992.f, Harness.GetHealth(Defender), 0.001f));
		const FArcActiveEffect* Aged = Harness.FindActiveByDefinition(Defender, Burning);
		ASSERT_THAT(IsNotNull(Aged));
		ASSERT_THAT(IsNear(8.f, Aged->RemainingDuration, 0.001f));

		const FArcEffectApplyResult Second = Harness.Apply(Burning, Defender, Attacker);
		ASSERT_THAT(IsTrue(Second.Handle.IsValid()));
		ASSERT_THAT(IsTrue(Second.Handle != First.Handle));

		ASSERT_THAT(AreEqual(1, ArcMechTest::CountActiveEffects(*Harness.EntityManager, Defender, Burning)));
		ASSERT_THAT(AreEqual(1, BurningTags()));
		const FArcActiveEffect* Fresh = Harness.FindActiveByDefinition(Defender, Burning);
		ASSERT_THAT(IsNotNull(Fresh));
		ASSERT_THAT(IsTrue(Fresh->Handle == Second.Handle));
		ASSERT_THAT(IsNear(10.f, Fresh->RemainingDuration, 0.001f));

		Harness.TickDuration(1.f, Defender);
		ASSERT_THAT(IsNear(988.f, Harness.GetHealth(Defender), 0.001f));
	}
};
