// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcCombatTestHarness.h"
#include "ArcCombatTestTypes.h"
#include "ArcRPGCombatTestSupport.h"
#include "ArcRPGMechTestSupport.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"
#include "AssetRegistry/AssetData.h"
#include "Combat/ArcRPGDamageExecution.h"
#include "Components/ActorTestSpawner.h"
#include "EditorValidatorBase.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDefinition.h"
#include "Events/ArcGameEvent.h"
#include "Misc/DataValidation.h"

// The Hit outcome is published by the damage execution on every execution (application and every tick); the gate publishes only vetoes.
TEST_CLASS(ArcRPGHitResolved, "ArcRPGMechanics.Combat.HitResolved")
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

	TEST_METHOD(InstantHit_PublishesOneHit_AndOneDamageEvent)
	{
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(Strike));
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();

		Harness.Apply(Strike, Defender, Attacker);

		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(AreEqual(1, Harness.CountOutcomeEvents()));
		ASSERT_THAT(AreEqual(1, Harness.CountHits()));
		ASSERT_THAT(AreEqual(1, Harness.CountDamageEvents()));

		const FArcGameEvent* Event = Harness.FindLastEvent(ArcMassMechanicsTags::DamageDealt);
		ASSERT_THAT(IsNotNull(Event));
		ASSERT_THAT(IsTrue(Event->Instigator == Attacker));
		ASSERT_THAT(IsTrue(Event->Target == Defender));
		ASSERT_THAT(IsNear(10.f, Event->Magnitude, 0.001f));

		const FArcDamageDealtPayload* Payload = Event->Payload.GetPtr<FArcDamageDealtPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsFalse(Payload->Zone.IsValid()));
		ASSERT_THAT(IsFalse(Payload->bCrit));
		ASSERT_THAT(IsTrue(Payload->DamageType == ArcCombatTestTags::DamageSlash.GetTag()));
		ASSERT_THAT(IsNear(10.f, Payload->PreMitigation, 0.001f));
		ASSERT_THAT(IsNear(10.f, Payload->Final, 0.001f));
	}

	TEST_METHOD(TwoHits_PublishTwoHitEvents)
	{
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(Strike));
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();

		Harness.Apply(Strike, Defender, Attacker);
		Harness.Apply(Strike, Defender, Attacker);

		ASSERT_THAT(AreEqual(2, Harness.CountHits()));
		ASSERT_THAT(AreEqual(2, Harness.CountDamageEvents()));
		ASSERT_THAT(IsNear(980.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(ImmuneDefender_LandsNothing_AndPublishesNoOutcome)
	{
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(Strike));
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.ImmunityTags = FGameplayTagContainer(ArcCombatTestTags::DamageSlash);
		DefenderOptions.BlockChance = 1.f;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);

		const FArcEffectApplyResult Result = Harness.Apply(Strike, Defender, Attacker);

		ASSERT_THAT(IsFalse(Result.bApplied));
		ASSERT_THAT(IsNear(1000.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(AreEqual(0, Harness.CountOutcomeEvents()));
	}

	TEST_METHOD(ImmuneDefender_WithCertainDodge_RollsNothing_AndPublishesNoDodgeOrHit)
	{
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(Strike));
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.ImmunityTags = FGameplayTagContainer(ArcCombatTestTags::DamageSlash);
		DefenderOptions.DodgeChance = 1.f;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);

		const FArcEffectApplyResult Result = Harness.Apply(Strike, Defender, Attacker);
		Harness.Settle(Defender);

		ASSERT_THAT(IsFalse(Result.bApplied));
		ASSERT_THAT(IsNear(1000.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(AreEqual(0, Harness.CountEvents(ArcMassMechanicsTags::HitOutcome_Dodge)));
		ASSERT_THAT(AreEqual(0, Harness.CountHits()));
		ASSERT_THAT(AreEqual(0, Harness.CountDamageEvents()));
		ASSERT_THAT(AreEqual(0, Harness.CountOutcomeEvents()));
	}

	TEST_METHOD(DamageOverTime_WithoutGate_EveryTickPublishesAHit)
	{
		UArcEffectDefinition* Dot = ArcRPGCombatTest::LoadCombat(TEXT("ED_DoT_Stored"));
		ASSERT_THAT(IsNotNull(Dot));
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();

		const FArcEffectApplyResult Result = Harness.Apply(Dot, Defender, Attacker);
		ASSERT_THAT(IsTrue(Result.Handle.IsValid()));
		ASSERT_THAT(AreEqual(0, Harness.CountOutcomeEvents()));

		Harness.TickDuration(1.f, Defender);
		Harness.TickDuration(1.f, Defender);
		Harness.TickDuration(1.f, Defender);

		ASSERT_THAT(AreEqual(3, Harness.CountHits()));
		ASSERT_THAT(AreEqual(3, Harness.CountOutcomeEvents()));
		const FArcHitResolvedPayload* Hit = Harness.FindLastHit();
		ASSERT_THAT(IsNotNull(Hit));
		ASSERT_THAT(IsTrue(Hit->Definition.Get() == Dot));
		ASSERT_THAT(IsNear(10.f, Hit->Damage, 0.001f));
		const FArcDamageDealtPayload* Damage = Harness.FindLastDamage();
		ASSERT_THAT(IsNotNull(Damage));
		ASSERT_THAT(IsTrue(Damage->bPeriodicTick));
		ASSERT_THAT(IsNear(970.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(DamageOverTime_TicksRollNoCrit)
	{
		UArcEffectDefinition* Dot = ArcRPGCombatTest::LoadCombat(TEXT("ED_DoT_Stored"));
		ASSERT_THAT(IsNotNull(Dot));
		ArcCombatTest::FCombatEntityOptions AttackerOptions;
		AttackerOptions.CritChance = 1.f;
		AttackerOptions.CritMultiplier = 4.5f;
		const FMassEntityHandle Attacker = Harness.CreateEntity(AttackerOptions);
		const FMassEntityHandle Defender = Harness.CreateEntity();

		Harness.Apply(Dot, Defender, Attacker);
		Harness.TickDuration(1.f, Defender);

		const FArcDamageDealtPayload* Damage = Harness.FindLastDamage();
		ASSERT_THAT(IsNotNull(Damage));
		ASSERT_THAT(IsFalse(Damage->bCrit));
		ASSERT_THAT(IsNear(10.f, Damage->Final, 0.001f));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(GatedDamageOverTime_GatePublishesNoHit_EveryTickDoes)
	{
		UArcEffectDefinition* Dot = ArcRPGCombatTest::LoadCombat(TEXT("ED_DoT_Gated"));
		ASSERT_THAT(IsNotNull(Dot));
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();

		const FArcEffectApplyResult Result = Harness.Apply(Dot, Defender, Attacker);
		ASSERT_THAT(IsTrue(Result.Handle.IsValid()));
		ASSERT_THAT(AreEqual(0, Harness.CountOutcomeEvents()));

		Harness.TickDuration(1.f, Defender);
		Harness.TickDuration(1.f, Defender);
		Harness.TickDuration(1.f, Defender);

		ASSERT_THAT(AreEqual(3, Harness.CountHits()));
		ASSERT_THAT(AreEqual(3, Harness.CountOutcomeEvents()));
		ASSERT_THAT(IsNear(985.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(VetoedDamageOverTime_PublishesTheVetoOnly_AndNoHitOnLaterTicks)
	{
		UArcEffectDefinition* Dot = ArcRPGCombatTest::LoadCombat(TEXT("ED_DoT_Gated"));
		ASSERT_THAT(IsNotNull(Dot));
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.BlockChance = 1.f;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);

		const FArcEffectApplyResult Result = Harness.Apply(Dot, Defender, Attacker);
		Harness.TickDuration(1.f, Defender);
		Harness.TickDuration(1.f, Defender);

		ASSERT_THAT(IsFalse(Result.Handle.IsValid()));
		ASSERT_THAT(IsNull(Harness.FindActiveByDefinition(Defender, Dot)));
		ASSERT_THAT(AreEqual(1, Harness.CountEvents(ArcMassMechanicsTags::HitOutcome_Block)));
		ASSERT_THAT(AreEqual(0, Harness.CountHits()));
		ASSERT_THAT(AreEqual(0, Harness.CountDamageEvents()));
		ASSERT_THAT(IsNear(1000.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(StoredGate_VetoPublishesTheOutcome_AndStoresNothing)
	{
		UArcEffectDefinition* Stored = ArcRPGCombatTest::LoadCombat(TEXT("ED_Gate_StoredOnly"));
		ASSERT_THAT(IsNotNull(Stored));
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.BlockChance = 1.f;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);

		const FArcEffectApplyResult Result = Harness.Apply(Stored, Defender, Attacker);

		ASSERT_THAT(IsFalse(Result.Handle.IsValid()));
		ASSERT_THAT(IsNull(Harness.FindActiveByDefinition(Defender, Stored)));
		ASSERT_THAT(AreEqual(1, Harness.CountEvents(ArcMassMechanicsTags::HitOutcome_Block)));
		ASSERT_THAT(AreEqual(1, Harness.CountOutcomeEvents()));
	}

	TEST_METHOD(StoredGateWithoutDamageExecution_PublishesNoHit)
	{
		UArcEffectDefinition* Stored = ArcRPGCombatTest::LoadCombat(TEXT("ED_Gate_StoredOnly"));
		ASSERT_THAT(IsNotNull(Stored));
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();

		const FArcEffectApplyResult Result = Harness.Apply(Stored, Defender, Attacker);

		ASSERT_THAT(IsTrue(Result.Handle.IsValid()));
		ASSERT_THAT(AreEqual(0, Harness.CountOutcomeEvents()));
	}
};

TEST_CLASS(ArcRPGHitResolved_Validators, "ArcRPGMechanics.Combat.Validator")
{
	TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	UEditorValidatorBase* GateValidator = nullptr;
	UEditorValidatorBase* DamageTypeValidator = nullptr;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		GateValidator = ArcRPGCombatTest::MakeValidator(TEXT("/Script/ArcMassMechanicsEditor.ArcHitGateValidator"));
		ASSERT_THAT(IsNotNull(GateValidator));
		DamageTypeValidator = ArcRPGCombatTest::MakeValidator(TEXT("/Script/ArcMassMechanicsEditor.ArcDamageTypeValidator"));
		ASSERT_THAT(IsNotNull(DamageTypeValidator));
	}

	static int32 CountDamageExecutions(const UArcEffectDefinition& Definition)
	{
		int32 Count = 0;
		for (const FArcEffectExecutionEntry& Entry : Definition.Executions)
		{
			if (Entry.CustomExecution.GetPtr<FArcRPGDamageExecution>())
			{
				++Count;
			}
		}
		return Count;
	}

	TEST_METHOD(InstantGateWithExecution_Passes)
	{
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(Strike));
		ASSERT_THAT(AreEqual(1, CountDamageExecutions(*Strike)));
		ASSERT_THAT(IsTrue(ArcRPGCombatTest::Passes(*GateValidator, Strike)));
	}

	TEST_METHOD(TwoGates_Fails)
	{
		UArcEffectDefinition* Effect = ArcCombatTest::MakeEffect();
		ArcCombatTest::AddGate(Effect);
		ArcCombatTest::AddGate(Effect);
		ASSERT_THAT(IsTrue(ArcRPGCombatTest::FailsWith(*GateValidator, Effect, TEXT("keep one Hit Gate"))));
	}

	TEST_METHOD(StoredGateWithoutExecution_Passes)
	{
		UArcEffectDefinition* Stored = ArcRPGCombatTest::LoadCombat(TEXT("ED_Gate_StoredOnly"));
		ASSERT_THAT(IsNotNull(Stored));
		ASSERT_THAT(AreEqual(0, Stored->Executions.Num()));
		ASSERT_THAT(IsTrue(ArcRPGCombatTest::Passes(*GateValidator, Stored)));
	}

	TEST_METHOD(StoredExecutionWithoutGate_Passes)
	{
		UArcEffectDefinition* Dot = ArcRPGCombatTest::LoadCombat(TEXT("ED_DoT_Stored"));
		ASSERT_THAT(IsNotNull(Dot));
		ASSERT_THAT(AreEqual(1, CountDamageExecutions(*Dot)));
		ASSERT_THAT(IsTrue(ArcRPGCombatTest::Passes(*GateValidator, Dot)));
	}

	TEST_METHOD(OneDamageExecutionPerEffect_TwoFail)
	{
		UArcEffectDefinition* Strike = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(Strike));
		FDataValidationContext SingleContext;
		ASSERT_THAT(IsFalse(DamageTypeValidator->ValidateLoadedAsset(FAssetData(), Strike, SingleContext) == EDataValidationResult::Invalid));

		UArcEffectDefinition* Effect = ArcCombatTest::MakeEffect();
		FArcEffectComp_EffectTags EffectTags;
		EffectTags.OwnedTags.AddTag(ArcCombatTestTags::DamageSlash);
		Effect->Components.Add(FInstancedStruct::Make(EffectTags));
		FArcRPGDamageExecution Execution;
		Execution.Base = 10.f;
		ArcCombatTest::AddExecution(Effect, FInstancedStruct::Make(Execution));
		ArcCombatTest::AddExecution(Effect, FInstancedStruct::Make(Execution));
		ASSERT_THAT(IsTrue(ArcRPGCombatTest::FailsWith(*DamageTypeValidator, Effect, TEXT("damage executions"))));
	}
};
