// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcCombatTestHarness.h"
#include "ArcCombatTestTypes.h"
#include "ArcRPGCombatTestSupport.h"
#include "ArcRPGMechTestSupport.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Events/ArcGameEvent.h"

// Hit chain rolls through the L2 hit gate on effects whose damage is the RPG damage execution (moved from ArcMassMechanicsTest, QC).
TEST_CLASS(ArcRPGHitChain_Rolls, "ArcRPGMechanics.Combat.HitChain")
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

	void ExpectVeto(const ArcCombatTest::FCombatEntityOptions& AttackerOptions, const ArcCombatTest::FCombatEntityOptions& DefenderOptions,
		const FGameplayTag& OutcomeTag, EArcHitOutcome Outcome)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity(AttackerOptions);
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(Strike));

		const FArcEffectApplyResult Result = Harness.Apply(Strike, Defender, Attacker);

		ASSERT_THAT(IsFalse(Result.bApplied));
		ASSERT_THAT(IsNear(1000.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(AreEqual(1, Harness.CountEvents(OutcomeTag)));
		ASSERT_THAT(AreEqual(1, Harness.CountOutcomeEvents()));
		ASSERT_THAT(AreEqual(0, Harness.CountHits()));
		ASSERT_THAT(AreEqual(0, Harness.CountDamageEvents()));

		const FArcGameEvent* Event = Harness.FindLastEvent(OutcomeTag);
		ASSERT_THAT(IsNotNull(Event));
		ASSERT_THAT(IsTrue(Event->Instigator == Attacker));
		ASSERT_THAT(IsTrue(Event->Target == Defender));

		const FArcHitResolvedPayload* Payload = Harness.FindLastPayload(OutcomeTag);
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Outcome == Outcome));
	}

	int32 ApplyAndCountHits(const TCHAR* StrikeName, const ArcCombatTest::FCombatEntityOptions& AttackerOptions,
		const ArcCombatTest::FCombatEntityOptions& DefenderOptions, FMassEntityHandle& OutDefender)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity(AttackerOptions);
		OutDefender = Harness.CreateEntity(DefenderOptions);
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(StrikeName);
		if (!Strike)
		{
			return -1;
		}
		Harness.Apply(Strike, OutDefender, Attacker);
		return Harness.CountHits();
	}

	TEST_METHOD(AttackerMiss_VetoesWithMissEvent_AndPublishesNoHit)
	{
		ArcCombatTest::FCombatEntityOptions Attacker;
		Attacker.MissChance = 1.f;
		ArcCombatTest::FCombatEntityOptions Defender;
		Defender.DodgeChance = 1.f;
		ExpectVeto(Attacker, Defender, ArcMassMechanicsTags::HitOutcome_Miss, EArcHitOutcome::Miss);
	}

	TEST_METHOD(DefenderDodge_VetoesWithDodgeEvent_AndPublishesNoHit)
	{
		ArcCombatTest::FCombatEntityOptions Defender;
		Defender.DodgeChance = 1.f;
		Defender.ParryChance = 1.f;
		ExpectVeto(ArcCombatTest::FCombatEntityOptions(), Defender, ArcMassMechanicsTags::HitOutcome_Dodge, EArcHitOutcome::Dodge);
	}

	TEST_METHOD(DefenderParry_VetoesWithParryEvent_AndPublishesNoHit)
	{
		ArcCombatTest::FCombatEntityOptions Defender;
		Defender.ParryChance = 1.f;
		Defender.BlockChance = 1.f;
		ExpectVeto(ArcCombatTest::FCombatEntityOptions(), Defender, ArcMassMechanicsTags::HitOutcome_Parry, EArcHitOutcome::Parry);
	}

	TEST_METHOD(DefenderBlock_VetoesWithBlockEvent_AndPublishesNoHit)
	{
		ArcCombatTest::FCombatEntityOptions Defender;
		Defender.BlockChance = 1.f;
		ExpectVeto(ArcCombatTest::FCombatEntityOptions(), Defender, ArcMassMechanicsTags::HitOutcome_Block, EArcHitOutcome::Block);
	}

	TEST_METHOD(ZeroChances_LandTheHit_AndTheExecutionPublishesTheHit)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(Strike));

		const FArcEffectApplyResult Result = Harness.Apply(Strike, Defender, Attacker);

		ASSERT_THAT(IsTrue(Result.bApplied));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(IsNear(0.f, Harness.GetDamageMeta(Defender), 0.001f));
		ASSERT_THAT(AreEqual(1, Harness.CountOutcomeEvents()));
		ASSERT_THAT(AreEqual(1, Harness.CountHits()));

		const FArcHitResolvedPayload* Hit = Harness.FindLastHit();
		ASSERT_THAT(IsNotNull(Hit));
		ASSERT_THAT(IsTrue(Hit->Outcome == EArcHitOutcome::Hit));
		ASSERT_THAT(IsTrue(Hit->Definition.Get() == Strike));
		ASSERT_THAT(IsTrue(Hit->DamageType == ArcCombatTestTags::DamageSlash.GetTag()));
		ASSERT_THAT(IsNear(10.f, Hit->Damage, 0.001f));
	}

	TEST_METHOD(FromBehind_DisablesParryAndBlock)
	{
		ArcCombatTest::FCombatEntityOptions AttackerOptions;
		AttackerOptions.Location = FVector(-100.f, 0.f, 0.f);
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.ParryChance = 1.f;
		DefenderOptions.BlockChance = 1.f;
		FMassEntityHandle Defender;
		ASSERT_THAT(AreEqual(1, ApplyAndCountHits(TEXT("ED_Strike_Slash"), AttackerOptions, DefenderOptions, Defender)));
		ASSERT_THAT(AreEqual(1, Harness.CountOutcomeEvents()));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(FromBehind_StillAllowsDodge)
	{
		ArcCombatTest::FCombatEntityOptions AttackerOptions;
		AttackerOptions.Location = FVector(-100.f, 0.f, 0.f);
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.DodgeChance = 1.f;
		FMassEntityHandle Defender;
		ASSERT_THAT(AreEqual(0, ApplyAndCountHits(TEXT("ED_Strike_Slash"), AttackerOptions, DefenderOptions, Defender)));
		ASSERT_THAT(AreEqual(1, Harness.CountEvents(ArcMassMechanicsTags::HitOutcome_Dodge)));
	}

	TEST_METHOD(FromFront_AllowsParry)
	{
		ArcCombatTest::FCombatEntityOptions AttackerOptions;
		AttackerOptions.Location = FVector(100.f, 0.f, 0.f);
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.ParryChance = 1.f;
		FMassEntityHandle Defender;
		ASSERT_THAT(AreEqual(0, ApplyAndCountHits(TEXT("ED_Strike_Slash"), AttackerOptions, DefenderOptions, Defender)));
		ASSERT_THAT(AreEqual(1, Harness.CountEvents(ArcMassMechanicsTags::HitOutcome_Parry)));
	}

	TEST_METHOD(NoCritChance_LeavesTheHitUnscaled)
	{
		ArcCombatTest::FCombatEntityOptions AttackerOptions;
		AttackerOptions.CritChance = 0.f;
		AttackerOptions.CritMultiplier = 4.5f;
		const FMassEntityHandle Attacker = Harness.CreateEntity(AttackerOptions);
		const FMassEntityHandle Defender = Harness.CreateEntity();

		const FArcDamageDealtPayload* Payload = ArcRPGCombatTest::Strike(Harness, Defender, Attacker, ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash")));
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsFalse(Payload->bCrit));
		ASSERT_THAT(IsNear(10.f, Payload->Final, 0.001f));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(CertainCrit_ScalesTheHitByTheCritMultiplier)
	{
		ArcCombatTest::FCombatEntityOptions AttackerOptions;
		AttackerOptions.CritChance = 1.f;
		AttackerOptions.CritMultiplier = 2.f;
		const FMassEntityHandle Attacker = Harness.CreateEntity(AttackerOptions);
		const FMassEntityHandle Defender = Harness.CreateEntity();

		const FArcDamageDealtPayload* Payload = ArcRPGCombatTest::Strike(Harness, Defender, Attacker, ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash")));
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->bCrit));
		ASSERT_THAT(IsNear(20.f, Payload->Final, 0.001f));
	}
};
