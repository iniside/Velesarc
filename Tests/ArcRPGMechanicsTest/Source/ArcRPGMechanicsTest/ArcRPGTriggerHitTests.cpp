// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcCombatTestHarness.h"
#include "ArcCombatTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcRPGCombatTestSupport.h"
#include "ArcRPGMechTestSupport.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"
#include "ArcMassMechanics/Triggers/ArcTriggerIf.h"
#include "ArcMassMechanics/Triggers/ArcTriggerTypes.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Events/ArcGameEvent.h"
#include "Fragments/ArcOwnedTagsFragment.h"

namespace ArcRPGTriggerHitTestsPrivate
{
	UArcEffectDefinition* MakeIfHost(EArcEventRole Role, EArcIfChannel Channel, float Value)
	{
		FArcIfModifier Modifier;
		Modifier.Channel = Channel;
		Modifier.Value = Value;
		FArcEffectTask_TriggerIf Task;
		Task.Role = Role;
		Task.Modifiers.Add(Modifier);

		UArcEffectDefinition* Host = NewObject<UArcEffectDefinition>();
		Host->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		Host->Tasks.Add(FInstancedStruct::Make(Task));
		return Host;
	}

	int32 Burning(ArcCombatTest::FCombatHarness& Harness, FMassEntityHandle Entity)
	{
		return ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Entity, ArcMassMechanicsTags::Condition_Burning);
	}

	int32 Bleeding(ArcCombatTest::FCombatHarness& Harness, FMassEntityHandle Entity)
	{
		return ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Entity, ArcMassMechanicsTags::Condition_Bleeding);
	}

	void RemoveByHandle(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcActiveEffectHandle Handle)
	{
		FArcEffectRemovalQuery Query;
		Query.Handle = Handle;
		Query.Reason = EArcEffectRemovalReason::Removed;
		ArcEffects::RemoveEffects(EntityManager, Entity, Query);
	}

	// Rewards are real statuses (one instance, a new application replaces the old one), so a test counting firings removes them between hits.
	bool ClearRewards(ArcCombatTest::FCombatHarness& Harness, FMassEntityHandle Entity)
	{
		UArcEffectDefinition* BurningReward = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Reward_Burning"));
		UArcEffectDefinition* BleedingReward = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Reward_Bleeding"));
		if (!BurningReward || !BleedingReward)
		{
			return false;
		}
		for (const UArcEffectDefinition* Reward : {BurningReward, BleedingReward})
		{
			FArcEffectRemovalQuery Query;
			Query.Definition = Reward;
			Query.Reason = EArcEffectRemovalReason::Removed;
			ArcEffects::RemoveEffects(*Harness.EntityManager, Entity, Query);
		}
		Harness.Settle(Entity);
		return Burning(Harness, Entity) == 0 && Bleeding(Harness, Entity) == 0;
	}
}

// On triggers reacting to hit and damage events of the RPG damage execution (moved from ArcMassMechanicsTest, QC).
TEST_CLASS(ArcRPGTrigger_Hits, "ArcRPGMechanics.Triggers.Hits")
{
	TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	FActorTestSpawner Spawner;
	ArcCombatTest::FCombatHarness Harness;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		ArcRPGCombatTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	void Pump(std::initializer_list<FMassEntityHandle> Entities, int32 Frames = 6)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			for (const FMassEntityHandle& Entity : Entities)
			{
				Harness.Settle(Entity);
			}
		}
	}

	TEST_METHOD(OnDamage_HitResultDamageTypeCondition)
	{
		using namespace ArcRPGTriggerHitTestsPrivate;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_On_DamageType"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Attacker, Attacker);

		UArcEffectDefinition* StrikeSlash = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(StrikeSlash));
		Harness.Apply(StrikeSlash, Defender, Attacker);
		Pump({Attacker, Defender}, 2);

		ASSERT_THAT(AreEqual(1, Burning(Harness, Attacker)));
		ASSERT_THAT(AreEqual(0, Bleeding(Harness, Attacker)));
	}

	TEST_METHOD(OnHitEffectTags_AndOnDamageMagnitudeConditions)
	{
		using namespace ArcRPGTriggerHitTestsPrivate;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_On_HitFilters"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Attacker, Attacker);

		UArcEffectDefinition* StrikeSlash = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(StrikeSlash));
		Harness.Apply(StrikeSlash, Defender, Attacker);
		Pump({Attacker, Defender}, 2);
		ASSERT_THAT(AreEqual(0, Burning(Harness, Attacker)));
		ASSERT_THAT(AreEqual(0, Bleeding(Harness, Attacker)));

		UArcEffectDefinition* StrikeHeavy = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Heavy"));
		ASSERT_THAT(IsNotNull(StrikeHeavy));
		Harness.Apply(StrikeHeavy, Defender, Attacker);
		Pump({Attacker, Defender}, 2);
		ASSERT_THAT(AreEqual(1, Burning(Harness, Attacker)));
		ASSERT_THAT(AreEqual(1, Bleeding(Harness, Attacker)));
	}

	TEST_METHOD(ProcsNeverTriggerProcs_TwoOnHitTriggersOnOneOwner_AreBounded)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_On_Hit"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Attacker, Attacker);

		UArcEffectDefinition* StrikeSlash = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(StrikeSlash));
		Harness.Apply(StrikeSlash, Defender, Attacker);
		Pump({Attacker, Defender});

		ASSERT_THAT(AreEqual(3, Harness.CountHits()));
		ASSERT_THAT(IsNear(970.f, Harness.GetHealth(Defender), 0.001f));
		const FArcGameEvent* Last = Harness.FindLastEvent(ArcMassMechanicsTags::DamageDealt);
		ASSERT_THAT(IsNotNull(Last));
		ASSERT_THAT(IsTrue(Last->SourceTags.HasTagExact(ArcMassMechanicsTags::TriggerOrigin)));
	}

	TEST_METHOD(ProcsNeverTriggerProcs_TwoEntityRetaliation_IsBounded)
	{
		const FMassEntityHandle First = Harness.CreateEntity();
		const FMassEntityHandle Second = Harness.CreateEntity();
		UArcEffectDefinition* Retaliate = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_On_Retaliate"));
		ASSERT_THAT(IsNotNull(Retaliate));
		Harness.Apply(Retaliate, First, First);
		Harness.Apply(Retaliate, Second, Second);

		UArcEffectDefinition* StrikeSlash = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(StrikeSlash));
		Harness.Apply(StrikeSlash, Second, First);
		Pump({First, Second});

		ASSERT_THAT(AreEqual(2, Harness.CountHits()));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(First), 0.001f));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Second), 0.001f));
	}

	TEST_METHOD(ProcsNeverTriggerProcs_ModifiersOnlyIfStillAppliesOnProcHits_AndIfEffectsDoNot)
	{
		using namespace ArcRPGTriggerHitTestsPrivate;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_ProcGuard_IfMix"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Attacker, Attacker);

		UArcEffectDefinition* StrikeSlash = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(StrikeSlash));
		Harness.Apply(StrikeSlash, Defender, Attacker);
		Pump({Attacker, Defender});

		ASSERT_THAT(AreEqual(2, Harness.CountHits()));
		ASSERT_THAT(IsNear(970.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(IsNear(1001.f, Harness.GetHealth(Attacker), 0.001f));
	}
};

TEST_CLASS(ArcRPGTrigger_If, "ArcRPGMechanics.Triggers.If")
{
	TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	FActorTestSpawner Spawner;
	ArcCombatTest::FCombatHarness Harness;
	UArcEffectDefinition* Strike = nullptr;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		ArcRPGCombatTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(Strike));
	}

	int32 Count(FMassEntityHandle Entity, const UArcEffectDefinition* Definition) const
	{
		return ArcMechTest::CountActiveEffects(*Harness.EntityManager, Entity, Definition);
	}

	TEST_METHOD(OwnerAsSource_DoneMultiplierScalesTheHit)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		Harness.Apply(Strike, Defender, Attacker);
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));

		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_DoneMultiplier"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Attacker, Attacker);
		Harness.Apply(Strike, Defender, Attacker);
		ASSERT_THAT(IsNear(975.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(OwnerAsTarget_TakenFlatThenTakenMultiplierScaleTheHit)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		Harness.Apply(Strike, Defender, Attacker);
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));

		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_TakenFlatMultiplier"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Defender, Defender);
		Harness.Apply(Strike, Defender, Attacker);
		ASSERT_THAT(IsNear(982.5f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(OwnerAsSource_DoneFlatAddsToTheHit)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Baseline = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		Harness.Apply(Strike, Baseline, Attacker);
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Baseline), 0.001f));

		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_DoneFlat"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Attacker, Attacker);
		const FArcDamageDealtPayload* Payload = ArcRPGCombatTest::Strike(Harness, Defender, Attacker, Strike);
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsNear(15.f, Payload->Final, 0.001f));
		ASSERT_THAT(IsNear(985.f, Harness.GetHealth(Defender), 0.001f));
	}

	// Taken on Role Instigator fails the validator, so both hosts are code-built (decision 7); they differ only in Role.
	TEST_METHOD(RoleInstigator_IgnoresHitsWhereTheOwnerIsStruck_RoleTargetAppliesThem)
	{
		using namespace ArcRPGTriggerHitTestsPrivate;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle InstigatorDefender = Harness.CreateEntity();
		const FMassEntityHandle TargetDefender = Harness.CreateEntity();

		Harness.Apply(MakeIfHost(EArcEventRole::Instigator, EArcIfChannel::DamageTakenMultiplier, -0.5f), InstigatorDefender, InstigatorDefender);
		Harness.Apply(MakeIfHost(EArcEventRole::Target, EArcIfChannel::DamageTakenMultiplier, -0.5f), TargetDefender, TargetDefender);

		Harness.Apply(Strike, InstigatorDefender, Attacker);
		Harness.Apply(Strike, TargetDefender, Attacker);
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(InstigatorDefender), 0.001f));
		ASSERT_THAT(IsNear(995.f, Harness.GetHealth(TargetDefender), 0.001f));
	}

	TEST_METHOD(TagsCondition_ReadsTheTargetsTagsCapturedForThisHit)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_TargetTags"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Attacker, Attacker);

		Harness.Apply(Strike, Defender, Attacker);
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));

		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Defender, ArcRPGCombatTest::Tags({ArcCombatTestTags::Primer}));
		Harness.Apply(Strike, Defender, Attacker);
		ASSERT_THAT(IsNear(975.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(Effects_AreDeferredPastTheExecution)
	{
		using namespace ArcRPGTriggerHitTestsPrivate;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_Effects"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Attacker, Attacker);

		ArcEffects::TryApplyEffect(*Harness.EntityManager, Defender, Strike, Attacker);
		ASSERT_THAT(AreEqual(0, Burning(Harness, Attacker)));

		Harness.Settle(Defender);
		ASSERT_THAT(AreEqual(1, Burning(Harness, Attacker)));
	}

	TEST_METHOD(TriggerCharges_AreSpentPerHit_OneTargetAtATime)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle FirstTarget = Harness.CreateEntity();
		const FMassEntityHandle SecondTarget = Harness.CreateEntity();
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_DoneMultiplier_Charge1"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Attacker, Attacker);

		Harness.Apply(Strike, FirstTarget, Attacker);
		Harness.Apply(Strike, SecondTarget, Attacker);
		Harness.Settle(Attacker);

		ASSERT_THAT(IsNear(985.f, Harness.GetHealth(FirstTarget), 0.001f));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(SecondTarget), 0.001f));
		ASSERT_THAT(AreEqual(0, Count(Attacker, Host)));
	}

	TEST_METHOD(SelfHit_IsEvaluatedOnce_DoneAndTakenPayOneCharge)
	{
		using namespace ArcRPGTriggerHitTestsPrivate;
		const FMassEntityHandle Self = Harness.CreateEntity();
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_SelfHit"));
		ASSERT_THAT(IsNotNull(Host));
		Harness.Apply(Host, Self, Self);

		Harness.Apply(Strike, Self, Self);
		ASSERT_THAT(AreEqual(1, Burning(Harness, Self)));
		ASSERT_THAT(AreEqual(1, Bleeding(Harness, Self)));
		ASSERT_THAT(IsNear(985.f, Harness.GetHealth(Self), 0.001f));
		ASSERT_THAT(IsTrue(ClearRewards(Harness, Self)));

		Harness.Apply(Strike, Self, Self);
		ASSERT_THAT(AreEqual(1, Burning(Harness, Self)));
		ASSERT_THAT(AreEqual(1, Bleeding(Harness, Self)));
		ASSERT_THAT(IsNear(970.f, Harness.GetHealth(Self), 0.001f));
		ASSERT_THAT(IsTrue(ClearRewards(Harness, Self)));

		Harness.Apply(Strike, Self, Self);
		ASSERT_THAT(AreEqual(0, Burning(Harness, Self)));
		ASSERT_THAT(AreEqual(0, Bleeding(Harness, Self)));
		ASSERT_THAT(IsNear(960.f, Harness.GetHealth(Self), 0.001f));
		ASSERT_THAT(AreEqual(1, Count(Self, Host)));
	}

	TEST_METHOD(RemovedHost_StopsApplyingModifiers)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_DoneMultiplier"));
		ASSERT_THAT(IsNotNull(Host));
		const FArcEffectApplyResult Applied = Harness.Apply(Host, Attacker, Attacker);
		Harness.Apply(Strike, Defender, Attacker);
		ASSERT_THAT(IsNear(985.f, Harness.GetHealth(Defender), 0.001f));

		ArcRPGTriggerHitTestsPrivate::RemoveByHandle(*Harness.EntityManager, Attacker, Applied.Handle);
		Harness.Settle(Attacker);
		Harness.Apply(Strike, Defender, Attacker);
		ASSERT_THAT(IsNear(975.f, Harness.GetHealth(Defender), 0.001f));
	}
};

// SpendOn table (plan 6 Step 5). Crit outcomes are forced with data: an attacker Crit Chance that puts the rolled chance at <= 0 or >= 1.
// Each host has one Trigger charge with remove-when-spent on, so a paid charge removes the host and a refund keeps it.
TEST_CLASS(ArcRPGTrigger_IfSpendOn, "ArcRPGMechanics.Triggers.If.SpendOn")
{
	TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	FActorTestSpawner Spawner;
	ArcCombatTest::FCombatHarness Harness;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		ArcRPGCombatTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	struct FRow
	{
		bool bCrit = false;
		float Final = 0.f;
		int32 HostsLeft = -1;
	};

	bool RunRow(const TCHAR* HostName, float AttackerCritChance, FRow& OutRow)
	{
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(HostName);
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Plain"));
		if (!Host || !Strike)
		{
			return false;
		}
		ArcCombatTest::FCombatEntityOptions AttackerOptions;
		AttackerOptions.CritChance = AttackerCritChance;
		AttackerOptions.CritMultiplier = 1.5f;
		const FMassEntityHandle Attacker = Harness.CreateEntity(AttackerOptions);
		const FMassEntityHandle Defender = Harness.CreateEntity();
		Harness.Apply(Host, Attacker, Attacker);

		const FArcDamageDealtPayload* Payload = ArcRPGCombatTest::Strike(Harness, Defender, Attacker, Strike);
		if (!Payload)
		{
			return false;
		}
		OutRow.bCrit = Payload->bCrit;
		OutRow.Final = Payload->Final;
		Harness.Settle(Attacker);
		OutRow.HostsLeft = ArcMechTest::CountActiveEffects(*Harness.EntityManager, Attacker, Host);
		return true;
	}

	TEST_METHOD(CritChance_AnyContributingHit_NoCrit_ChargePaid)
	{
		FRow Row;
		ASSERT_THAT(IsTrue(RunRow(TEXT("ED_Host_If_CritChanceHalf_AnyHit"), -0.5f, Row)));
		ASSERT_THAT(IsFalse(Row.bCrit));
		ASSERT_THAT(IsNear(10.f, Row.Final, 0.001f));
		ASSERT_THAT(AreEqual(0, Row.HostsLeft));
	}

	TEST_METHOD(CritChance_OnlyCrit_NoCrit_ChargeRefunded)
	{
		FRow Row;
		ASSERT_THAT(IsTrue(RunRow(TEXT("ED_Host_If_CritChanceHalf_OnlyCrit"), -0.5f, Row)));
		ASSERT_THAT(IsFalse(Row.bCrit));
		ASSERT_THAT(IsNear(10.f, Row.Final, 0.001f));
		ASSERT_THAT(AreEqual(1, Row.HostsLeft));
	}

	TEST_METHOD(CritChance_OnlyCrit_Crit_ModifierAppliedAndChargePaid)
	{
		FRow Row;
		ASSERT_THAT(IsTrue(RunRow(TEXT("ED_Host_If_CritChanceHalf_OnlyCrit"), 0.5f, Row)));
		ASSERT_THAT(IsTrue(Row.bCrit));
		ASSERT_THAT(IsNear(15.f, Row.Final, 0.001f));
		ASSERT_THAT(AreEqual(0, Row.HostsLeft));
	}

	TEST_METHOD(DoneMultiplier_AnyContributingHit_NoCrit_ModifierAppliedAndChargePaid)
	{
		FRow Row;
		ASSERT_THAT(IsTrue(RunRow(TEXT("ED_Host_If_DoneMultiplier30_AnyHit"), 0.f, Row)));
		ASSERT_THAT(IsFalse(Row.bCrit));
		ASSERT_THAT(IsNear(13.f, Row.Final, 0.001f));
		ASSERT_THAT(AreEqual(0, Row.HostsLeft));
	}

	TEST_METHOD(DoneMultiplier_OnlyCrit_NoCrit_ModifierAppliedAndChargeRefunded)
	{
		FRow Row;
		ASSERT_THAT(IsTrue(RunRow(TEXT("ED_Host_If_DoneMultiplier30_OnlyCrit"), 0.f, Row)));
		ASSERT_THAT(IsFalse(Row.bCrit));
		ASSERT_THAT(IsNear(13.f, Row.Final, 0.001f));
		ASSERT_THAT(AreEqual(1, Row.HostsLeft));
	}

	TEST_METHOD(DoneMultiplier_OnlyCrit_Crit_ModifierAppliedAndChargePaid)
	{
		FRow Row;
		ASSERT_THAT(IsTrue(RunRow(TEXT("ED_Host_If_DoneMultiplier30_OnlyCrit"), 1.f, Row)));
		ASSERT_THAT(IsTrue(Row.bCrit));
		ASSERT_THAT(IsNear(19.5f, Row.Final, 0.001f));
		ASSERT_THAT(AreEqual(0, Row.HostsLeft));
	}

	TEST_METHOD(OnlyCrit_RefundedChargeStaysAvailable_UntilACritPaysIt)
	{
		UArcEffectDefinition* Host = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_DoneMultiplier30_OnlyCrit"));
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Plain"));
		ASSERT_THAT(IsNotNull(Host));
		ASSERT_THAT(IsNotNull(Strike));
		ArcCombatTest::FCombatEntityOptions AttackerOptions;
		AttackerOptions.CritMultiplier = 1.5f;
		const FMassEntityHandle Attacker = Harness.CreateEntity(AttackerOptions);
		const FMassEntityHandle Defender = Harness.CreateEntity();
		Harness.Apply(Host, Attacker, Attacker);

		ASSERT_THAT(IsNear(13.f, ArcRPGCombatTest::StrikeFinal(Harness, Defender, Attacker, Strike), 0.001f));
		ASSERT_THAT(IsNear(13.f, ArcRPGCombatTest::StrikeFinal(Harness, Defender, Attacker, Strike), 0.001f));
		Harness.Settle(Attacker);
		ASSERT_THAT(AreEqual(1, ArcMechTest::CountActiveEffects(*Harness.EntityManager, Attacker, Host)));
	}
};
