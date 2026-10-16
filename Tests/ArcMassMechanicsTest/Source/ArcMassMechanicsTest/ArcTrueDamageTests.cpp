// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcCombatTestHarness.h"
#include "ArcCombatTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcStatusTestHarness.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcCombatFunctions.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"

namespace ArcTrueDamageTestsPrivate
{
	UArcEffectDefinition* LoadTrueDamage()
	{
		return ArcMechTest::LoadTestAsset<UArcEffectDefinition>(TEXT("Combat/ED_TrueDamage_Test"));
	}

	UArcEffectDefinition* LoadTrueHeal()
	{
		return ArcMechTest::LoadTestAsset<UArcEffectDefinition>(TEXT("Combat/ED_TrueHeal_Test"));
	}
}

TEST_CLASS(ArcMechTrueDamage, "ArcMassMechanics.Combat.TrueDamage")
{
	FActorTestSpawner Spawner;
	ArcCombatTest::FCombatHarness Harness;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	TEST_METHOD(TrueDamage_LowersHealthThroughTheDamageMeta_IgnoringMitigation)
	{
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.Armor = 1000.f;
		DefenderOptions.Resistance = 0.5f;
		DefenderOptions.DamageReduction = 0.5f;
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);

		UArcEffectDefinition* TrueDamage = ArcTrueDamageTestsPrivate::LoadTrueDamage();
		ASSERT_THAT(IsNotNull(TrueDamage));
		const FArcEffectApplyResult Result = ArcMechCombat::ApplyEffectWithSetByCaller(*Harness.EntityManager, Defender, FMassEntityHandle(),
			TrueDamage, ArcMassMechanicsTags::SBC_TrueDamage, 40.f);
		Harness.Settle(Defender);

		ASSERT_THAT(IsTrue(Result.bApplied));
		ASSERT_THAT(IsNear(960.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(IsNear(0.f, Harness.GetDamageMeta(Defender), 0.001f));
		ASSERT_THAT(AreEqual(0, Harness.CountOutcomeEvents()));
	}

	TEST_METHOD(TrueHeal_RaisesHealthThroughTheHealMeta)
	{
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.Health = 500.f;
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);

		UArcEffectDefinition* TrueHeal = ArcTrueDamageTestsPrivate::LoadTrueHeal();
		ASSERT_THAT(IsNotNull(TrueHeal));
		ArcMechCombat::ApplyEffectWithSetByCaller(*Harness.EntityManager, Defender, FMassEntityHandle(),
			TrueHeal, ArcMassMechanicsTags::SBC_TrueHeal, 25.f);
		Harness.Settle(Defender);

		ASSERT_THAT(IsNear(525.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(TrueDamage_CarriesTheSourceEntity)
	{
		const FMassEntityHandle Source = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();

		UArcEffectDefinition* TrueDamage = ArcTrueDamageTestsPrivate::LoadTrueDamage();
		ASSERT_THAT(IsNotNull(TrueDamage));
		const FArcEffectApplyResult Result = ArcMechCombat::ApplyEffectWithSetByCaller(*Harness.EntityManager, Defender, Source,
			TrueDamage, ArcMassMechanicsTags::SBC_TrueDamage, 10.f);
		Harness.Settle(Defender);

		ASSERT_THAT(IsTrue(Result.bApplied));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(IsNear(1000.f, Harness.GetHealth(Source), 0.001f));
	}

	TEST_METHOD(NullEffect_AppliesNothing)
	{
		const FMassEntityHandle Defender = Harness.CreateEntity();

		const FArcEffectApplyResult Result = ArcMechCombat::ApplyEffectWithSetByCaller(*Harness.EntityManager, Defender, FMassEntityHandle(),
			nullptr, ArcMassMechanicsTags::SBC_TrueDamage, 10.f);
		Harness.Settle(Defender);

		ASSERT_THAT(IsFalse(Result.bApplied));
		ASSERT_THAT(IsNear(1000.f, Harness.GetHealth(Defender), 0.001f));
	}
};
