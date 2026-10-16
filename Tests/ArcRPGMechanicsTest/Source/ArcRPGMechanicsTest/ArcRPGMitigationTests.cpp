// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcCombatTestHarness.h"
#include "ArcCombatTestTypes.h"
#include "ArcRPGCombatTestSupport.h"
#include "ArcRPGMechanicsTags.h"
#include "ArcRPGMechanicsTestHarness.h"
#include "ArcRPGMechanicsTestTypes.h"
#include "ArcRPGMechTestSupport.h"
#include "ArcMassMechanics/Combat/ArcAttributeSet_Mitigation.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"
#include "Combat/ArcRPGArmorConfig.h"
#include "Combat/ArcRPGDamageExecution.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemDefinitionData.h"
#include "Mass/EntityBuilder.h"

namespace ArcRPGMitigationTestsPrivate
{
	// Armor 100 against a 10 damage hit with K 10: 10 * (1 - 100 / (100 + 10 * 10)) = 5.
	float CurveMitigated(float Damage, float Armor)
	{
		return Damage * (1.f - Armor / (Armor + 10.f * Damage));
	}
}

// Mitigation of the RPG damage execution, re-covering the cases of the deleted mech execution tests (c3e76f24c5b5).
TEST_CLASS(ArcRPGDamage_Mitigation, "ArcRPGMechanics.Combat.Damage.Mitigation")
{
	TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	FActorTestSpawner Spawner;
	ArcCombatTest::FCombatHarness Harness;
	UArcEffectDefinition* Slash = nullptr;
	UArcEffectDefinition* Fire = nullptr;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		ArcRPGCombatTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Slash = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"));
		ASSERT_THAT(IsNotNull(Slash));
		Fire = ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Fire"));
		ASSERT_THAT(IsNotNull(Fire));
	}

	float StrikeFinal(FMassEntityHandle Defender, FMassEntityHandle Attacker, UArcEffectDefinition* Strike)
	{
		return ArcRPGCombatTest::StrikeFinal(Harness, Defender, Attacker, Strike);
	}

	void AddResistance(FMassEntityHandle Entity, float Magnitude, const FGameplayTag& DamageType)
	{
		Harness.AddAttributeMod(Entity, FArcAttributeSet_Mitigation::GetResistanceAttribute(), Magnitude, DamageType);
	}

	void AddDamageReduction(FMassEntityHandle Entity, float Magnitude, const FGameplayTag& DamageType)
	{
		Harness.AddAttributeMod(Entity, FArcAttributeSet_Mitigation::GetDamageReductionAttribute(), Magnitude, DamageType);
	}

	TEST_METHOD(GlobalArmor_AppliesToEveryDamageType)
	{
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.Armor = 100.f;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);

		const FArcDamageDealtPayload* Payload = ArcRPGCombatTest::Strike(Harness, Defender, Attacker, Slash);
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsNear(10.f, Payload->PreMitigation, 0.001f));
		ASSERT_THAT(IsNear(5.f, Payload->Final, 0.001f));
		ASSERT_THAT(IsNear(5.f, StrikeFinal(Defender, Attacker, Fire), 0.001f));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(Layers_ArmorBeforeTheDamageReductionAndResistanceMultipliers)
	{
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.Armor = 100.f;
		DefenderOptions.DamageReduction = 0.5f;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);
		AddResistance(Defender, 0.2f, ArcCombatTestTags::DamageSlash);
		AddResistance(Defender, 0.5f, ArcCombatTestTags::DamageFire);

		ASSERT_THAT(IsNear(2.f, StrikeFinal(Defender, Attacker, Slash), 0.001f));
		ASSERT_THAT(IsNear(1.25f, StrikeFinal(Defender, Attacker, Fire), 0.001f));
		ASSERT_THAT(IsNear(996.75f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(Resistance_TagQualifiedModifier_IsPerType)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		AddResistance(Defender, 0.25f, ArcCombatTestTags::DamageFire);

		ASSERT_THAT(IsNear(10.f, StrikeFinal(Defender, Attacker, Slash), 0.001f));
		ASSERT_THAT(IsNear(7.5f, StrikeFinal(Defender, Attacker, Fire), 0.001f));
	}

	TEST_METHOD(Resistance_OneOrMore_IsImmunityForThatTypeOnly)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		AddResistance(Defender, 1.5f, ArcCombatTestTags::DamageFire);

		ASSERT_THAT(IsNear(10.f, StrikeFinal(Defender, Attacker, Slash), 0.001f));
		ASSERT_THAT(IsNear(0.f, StrikeFinal(Defender, Attacker, Fire), 0.001f));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(Resistance_ExactlyOne_IsImmunity_AndTheHitIsStillPublished)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		AddResistance(Defender, 1.f, ArcCombatTestTags::DamageFire);

		Harness.Apply(Fire, Defender, Attacker);

		ASSERT_THAT(IsNear(1000.f, Harness.GetHealth(Defender), 0.001f));
		ASSERT_THAT(AreEqual(1, Harness.CountHits()));
		ASSERT_THAT(AreEqual(0, Harness.CountDamageEvents()));
		const FArcHitResolvedPayload* Hit = Harness.FindLastHit();
		ASSERT_THAT(IsNotNull(Hit));
		ASSERT_THAT(IsNear(0.f, Hit->Damage, 0.001f));
	}

	TEST_METHOD(DamageReduction_PhysicalParentModifier_AppliesToPhysicalTypesOnly)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		AddDamageReduction(Defender, 0.5f, ArcCombatTestTags::DamagePhysical);

		ASSERT_THAT(IsNear(5.f, StrikeFinal(Defender, Attacker, Slash), 0.001f));
		ASSERT_THAT(IsNear(10.f, StrikeFinal(Defender, Attacker, Fire), 0.001f));
	}

	TEST_METHOD(DamageReduction_MagicalParentModifier_AppliesToMagicalTypesOnly)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		AddDamageReduction(Defender, 0.5f, ArcCombatTestTags::DamageMagical);

		ASSERT_THAT(IsNear(10.f, StrikeFinal(Defender, Attacker, Slash), 0.001f));
		ASSERT_THAT(IsNear(5.f, StrikeFinal(Defender, Attacker, Fire), 0.001f));
	}

	TEST_METHOD(DamageReduction_Untagged_AppliesToEveryType)
	{
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.DamageReduction = 0.5f;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);

		ASSERT_THAT(IsNear(5.f, StrikeFinal(Defender, Attacker, Slash), 0.001f));
		ASSERT_THAT(IsNear(5.f, StrikeFinal(Defender, Attacker, Fire), 0.001f));
	}
};

TEST_CLASS(ArcRPGDamage_FlatBonus, "ArcRPGMechanics.Combat.Damage.FlatBonus")
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

	TEST_METHOD(SingleHit_GetsTheWholeOutgoingAndIncomingFlatBonus)
	{
		ArcCombatTest::FCombatEntityOptions AttackerOptions;
		AttackerOptions.OutgoingBonus = 30.f;
		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.IncomingBonus = 20.f;
		const FMassEntityHandle Attacker = Harness.CreateEntity(AttackerOptions);
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);

		ASSERT_THAT(IsNear(60.f, ArcRPGCombatTest::StrikeFinal(Harness, Defender, Attacker, ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"))), 0.001f));
		ASSERT_THAT(IsNear(940.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(TypedIncomingBonus_GoesOnlyToItsType)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		Harness.AddIncomingBonusMod(Defender, 10.f, ArcCombatTestTags::DamageFire);

		ASSERT_THAT(IsNear(10.f, ArcRPGCombatTest::StrikeFinal(Harness, Defender, Attacker, ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Slash"))), 0.001f));
		ASSERT_THAT(IsNear(20.f, ArcRPGCombatTest::StrikeFinal(Harness, Defender, Attacker, ArcRPGCombatTest::LoadCombat(TEXT("ED_Strike_Fire"))), 0.001f));
		ASSERT_THAT(IsNear(970.f, Harness.GetHealth(Defender), 0.001f));
	}
};

// Final armor = struck piece (zone source, applied hits only) + global Armor read with the damage type (D-91 5d).
TEST_CLASS(ArcRPGDamage_ArmorTable, "ArcRPGMechanics.Combat.Damage.Armor")
{
	TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	FActorTestSpawner Spawner;
	ArcCombatTest::FCombatHarness Harness;
	FMassEntityHandle Attacker;
	FMassEntityHandle Defender;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		ArcRPGCombatTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));

		const FArcItemDefinitionData* Torso = ArcRPGMechanicsTestHarness::LoadItem(TEXT("ID_Armor_Torso60"));
		ASSERT_THAT(IsNotNull(Torso));
		const FArcItemDefinitionData* Shield = ArcRPGMechanicsTestHarness::LoadItem(TEXT("ID_Armor_Shield40"));
		ASSERT_THAT(IsNotNull(Shield));

		ArcCombatTest::FCombatEntityOptions Options;
		Options.AddFragments = [](UE::Mass::FEntityBuilder& Builder)
		{
			Builder.Add<FArcMassItemStoreFragment>();
		};
		Attacker = Harness.CreateEntity();
		Defender = Harness.CreateEntity(Options);
		ArcRPGCombatTest::Equip(*Harness.EntityManager, Defender, Torso, ArcRPGMechanicsTestTags::SlotTorso);
		ArcRPGCombatTest::Equip(*Harness.EntityManager, Defender, Shield, ArcRPGMechanicsTestTags::SlotShield);
		Harness.AddAttributeMod(Defender, FArcAttributeSet_Mitigation::GetArmorAttribute(), 40.f, ArcRPGMechanicsTags::Damage_Physical);
	}

	TEST_METHOD(AppliedHit_ZoneWithPiece_PieceAndGlobalArmorAdd)
	{
		const FArcDamageDealtPayload* Payload = ArcRPGCombatTest::Strike(Harness, Defender, Attacker, ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Zoned")));
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Zone == ArcRPGMechanicsTags::HitZone_Torso.GetTag()));
		ASSERT_THAT(IsNear(ArcRPGMitigationTestsPrivate::CurveMitigated(10.f, 100.f), Payload->Final, 0.001f));
	}

	TEST_METHOD(AppliedHit_ZoneWithoutPiece_GlobalArmorOnly)
	{
		const FArcDamageDealtPayload* Payload = ArcRPGCombatTest::Strike(Harness, Defender, Attacker, ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_ZonedHead")));
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Zone == ArcRPGMechanicsTags::HitZone_Head.GetTag()));
		ASSERT_THAT(IsNear(ArcRPGMitigationTestsPrivate::CurveMitigated(10.f, 40.f), Payload->Final, 0.001f));
	}

	TEST_METHOD(DamageOverTimeTick_StrikesNoPiece_GlobalArmorStillApplies)
	{
		UArcEffectDefinition* Dot = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGDoT_Zoned"));
		ASSERT_THAT(IsNotNull(Dot));
		Harness.Apply(Dot, Defender, Attacker);
		Harness.TickDuration(1.f, Defender);

		const FArcDamageDealtPayload* Payload = Harness.FindLastDamage();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->bPeriodicTick));
		ASSERT_THAT(IsFalse(Payload->Zone.IsValid()));
		ASSERT_THAT(IsNear(ArcRPGMitigationTestsPrivate::CurveMitigated(10.f, 40.f), Payload->Final, 0.001f));
	}

	TEST_METHOD(UncoveredDamageType_ReadsNoPieceAndNoGlobalArmor)
	{
		const FArcDamageDealtPayload* Payload = ArcRPGCombatTest::Strike(Harness, Defender, Attacker, ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Zoned_Fire")));
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Zone == ArcRPGMechanicsTags::HitZone_Torso.GetTag()));
		ASSERT_THAT(IsNear(10.f, Payload->Final, 0.001f));
	}
};

TEST_CLASS(ArcRPGDamage_ArmorConfig, "ArcRPGMechanics.Combat.Damage.ArmorConfig")
{
	TEST_METHOD(ArmorK_IsClampedAboveZeroInTheEditor)
	{
		const FFloatProperty* Property = FindFProperty<FFloatProperty>(UArcRPGArmorConfig::StaticClass(), GET_MEMBER_NAME_CHECKED(UArcRPGArmorConfig, ArmorK));
		ASSERT_THAT(IsNotNull(Property));
#if WITH_METADATA
		ASSERT_THAT(IsTrue(Property->GetMetaData(TEXT("ClampMin")) == TEXT("0.01")));
#endif
	}

	TEST_METHOD(AuthoredArmorConfig_UsesTheArmorCurveWithK10)
	{
		UArcRPGArmorConfig* Config = ArcRPGMechTest::LoadTestAsset<UArcRPGArmorConfig>(TEXT("Combat/DA_Armor_Test"));
		ASSERT_THAT(IsNotNull(Config));
		ASSERT_THAT(IsNotNull(Config->GetArmorCurve()));
		ASSERT_THAT(IsNear(10.f, Config->ArmorK, 0.0001f));
	}
};

// ValueTiming example from the plan: Bleed 10 per tick, then Vulnerable (+5 flat, +30% incoming) lands after the application.
TEST_CLASS(ArcRPGDamage_ValueTiming, "ArcRPGMechanics.Combat.Damage.ValueTiming")
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

	struct FTicks
	{
		float Before = -1.f;
		float After = -1.f;
		int32 DamageEventsAfterFirstTick = -1;
		int32 DamageEventsAfterSecondTick = -1;
		float Health = -1.f;
	};

	bool TickBeforeAndAfterVulnerable(const TCHAR* BleedName, FTicks& OutTicks)
	{
		UArcEffectDefinition* Bleed = ArcRPGCombatTest::LoadCombat(BleedName);
		UArcEffectDefinition* Vulnerable = ArcRPGCombatTest::LoadStatus(TEXT("ED_RPGStatus_Vulnerable"));
		if (!Bleed || !Vulnerable)
		{
			return false;
		}

		ArcCombatTest::FCombatEntityOptions DefenderOptions;
		DefenderOptions.MaxIncomingBonusPercent = 1.f;
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);

		Harness.Apply(Bleed, Defender, Attacker);
		Harness.TickDuration(1.f, Defender);
		OutTicks.DamageEventsAfterFirstTick = Harness.CountDamageEvents();
		const FArcDamageDealtPayload* Before = Harness.FindLastDamage();
		OutTicks.Before = Before ? Before->Final : -1.f;

		Harness.Apply(Vulnerable, Defender, FMassEntityHandle());
		Harness.TickDuration(1.f, Defender);
		OutTicks.DamageEventsAfterSecondTick = Harness.CountDamageEvents();
		const FArcDamageDealtPayload* After = Harness.FindLastDamage();
		OutTicks.After = After ? After->Final : -1.f;
		OutTicks.Health = Harness.GetHealth(Defender);
		return true;
	}

	TEST_METHOD(AtApplication_TicksKeepTheValuesReadOnApplication)
	{
		FTicks Ticks;
		ASSERT_THAT(IsTrue(TickBeforeAndAfterVulnerable(TEXT("ED_RPGDoT_Bleed_AtApplication"), Ticks)));
		ASSERT_THAT(AreEqual(1, Ticks.DamageEventsAfterFirstTick));
		ASSERT_THAT(AreEqual(2, Ticks.DamageEventsAfterSecondTick));
		ASSERT_THAT(IsNear(10.f, Ticks.Before, 0.001f));
		ASSERT_THAT(IsNear(10.f, Ticks.After, 0.001f));
		ASSERT_THAT(IsNear(980.f, Ticks.Health, 0.001f));
	}

	TEST_METHOD(EveryExecution_TicksReadTheCurrentValues)
	{
		FTicks Ticks;
		ASSERT_THAT(IsTrue(TickBeforeAndAfterVulnerable(TEXT("ED_RPGDoT_Bleed_EveryExecution"), Ticks)));
		ASSERT_THAT(AreEqual(1, Ticks.DamageEventsAfterFirstTick));
		ASSERT_THAT(AreEqual(2, Ticks.DamageEventsAfterSecondTick));
		ASSERT_THAT(IsNear(10.f, Ticks.Before, 0.001f));
		ASSERT_THAT(IsNear(19.5f, Ticks.After, 0.001f));
		ASSERT_THAT(IsNear(970.5f, Ticks.Health, 0.001f));
	}
};
