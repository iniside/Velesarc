// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcCombatTestHarness.h"
#include "ArcRPGMechanicsTags.h"
#include "ArcRPGCombatTestSupport.h"
#include "ArcRPGMechanicsTestHarness.h"
#include "ArcRPGMechanicsTestTypes.h"
#include "ArcRPGMechTestSupport.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcAttributeSet_Crit.h"
#include "ArcMassMechanics/Combat/ArcAttributeSet_HitChain.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"
#include "ArcMassMechanics/Triggers/ArcTriggerIf.h"
#include "ArcMassMechanics/Triggers/ArcTriggerTypes.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcAttributeSet_RPGSkills.h"
#include "Attributes/ArcInstantModifier.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "Processors/ArcAttributeObserverProcessor.h"
#include "Signals/ArcAttributeChangedSignal.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcRPGCombatTestsPrivate
{
	void AddSkills(UE::Mass::FEntityBuilder& Builder, float Aiming, float TwoHanded)
	{
		FArcAttributeSet_RPGSkills Skills;
		Skills.InitAiming(Aiming);
		Skills.InitTwoHanded(TwoHanded);
		Skills.InitDodging(0.f);
		Builder.Add<FArcAttributeSet_RPGSkills>(Skills);
	}

	FArcAttributeSet_RPGSkills* GetSkills(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
	{
		return FMassEntityView(EntityManager, Entity).GetSparseFragmentDataPtr<FArcAttributeSet_RPGSkills>();
	}

	FArcAttributeSet_HitChain* GetHitChain(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
	{
		return FMassEntityView(EntityManager, Entity).GetSparseFragmentDataPtr<FArcAttributeSet_HitChain>();
	}

	FArcAttributeSet_Crit* GetCrit(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
	{
		return FMassEntityView(EntityManager, Entity).GetSparseFragmentDataPtr<FArcAttributeSet_Crit>();
	}
}

TEST_CLASS(ArcRPGCombat_DamageExecution, "ArcRPGMechanics.Combat.DamageExecution")
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

	FMassEntityHandle CreateAttacker(float TwoHanded = 0.f, float OutgoingBonus = 0.f)
	{
		ArcCombatTest::FCombatEntityOptions Options;
		Options.OutgoingBonus = OutgoingBonus;
		Options.AddFragments = [TwoHanded](UE::Mass::FEntityBuilder& Builder)
		{
			ArcRPGCombatTestsPrivate::AddSkills(Builder, 0.f, TwoHanded);
		};
		return Harness.CreateEntity(Options);
	}

	TEST_METHOD(NeitherAttackPath_OnlyTheAuthoredBaseHits)
	{
		const FMassEntityHandle Attacker = CreateAttacker();
		const FMassEntityHandle Defender = Harness.CreateEntity();

		UArcEffectDefinition* RPGStrikePlain = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Plain"));
		ASSERT_THAT(IsNotNull(RPGStrikePlain));
		Harness.Apply(RPGStrikePlain, Defender, Attacker);

		const FArcDamageDealtPayload* Payload = Harness.FindLastDamage();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->DamageType == ArcRPGMechanicsTags::Damage_Slash.GetTag()));
		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(AttributeStep_ScalesTheBase_ButNotTheFlatBonus)
	{
		const FMassEntityHandle Attacker = CreateAttacker(14.f, 30.f);
		const FMassEntityHandle Defender = Harness.CreateEntity();

		UArcEffectDefinition* RPGStrikeMelee = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Melee"));
		ASSERT_THAT(IsNotNull(RPGStrikeMelee));
		FArcItemData Weapon;
		ASSERT_THAT(IsTrue(ArcRPGCombatTest::MakeSkillWeapon(Weapon)));
		Harness.Apply(RPGStrikeMelee, Defender, Attacker, ArcRPGCombatTest::MakeItemSource(Weapon));

		const FArcDamageDealtPayload* Payload = Harness.FindLastDamage();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsNear(45.f, Payload->Final, 0.001f));
		ASSERT_THAT(IsNear(955.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(AttributeStep_NeverGoesNegative)
	{
		const FMassEntityHandle Attacker = CreateAttacker(0.f);
		const FMassEntityHandle Defender = Harness.CreateEntity();

		UArcEffectDefinition* RPGStrikeMelee = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Melee"));
		ASSERT_THAT(IsNotNull(RPGStrikeMelee));
		FArcItemData Weapon;
		ASSERT_THAT(IsTrue(ArcRPGCombatTest::MakeSkillWeapon(Weapon)));
		Harness.Apply(RPGStrikeMelee, Defender, Attacker, ArcRPGCombatTest::MakeItemSource(Weapon));

		ASSERT_THAT(IsNear(1000.f, Harness.GetHealth(Defender), 0.001f));
	}
};

TEST_CLASS(ArcRPGCombat_ZoneArmor, "ArcRPGMechanics.Combat.ZoneArmor")
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

	static float TorsoMitigated()
	{
		return 10.f * (1.f - 60.f / (60.f + 10.f * 10.f));
	}

	FMassEntityHandle CreateArmoredDefender(const TCHAR* TorsoPiece = TEXT("ID_Armor_Torso60"))
	{
		using namespace ArcRPGCombatTestsPrivate;

		ArcCombatTest::FCombatEntityOptions Options;
		Options.AddFragments = [](UE::Mass::FEntityBuilder& Builder)
		{
			Builder.Add<FArcMassItemStoreFragment>();
		};
		const FArcItemDefinitionData* Torso = ArcRPGMechanicsTestHarness::LoadItem(TorsoPiece);
		const FArcItemDefinitionData* Shield = ArcRPGMechanicsTestHarness::LoadItem(TEXT("ID_Armor_Shield40"));
		if (!Torso || !Shield)
		{
			return FMassEntityHandle();
		}
		const FMassEntityHandle Defender = Harness.CreateEntity(Options);
		ArcRPGCombatTest::Equip(*Harness.EntityManager, Defender, Torso, ArcRPGMechanicsTestTags::SlotTorso);
		ArcRPGCombatTest::Equip(*Harness.EntityManager, Defender, Shield, ArcRPGMechanicsTestTags::SlotShield);
		return Defender;
	}

	TEST_METHOD(StruckZonePiece_MitigatesEveryPhysicalType_ShieldAddsNoArmor)
	{
		using namespace ArcRPGCombatTestsPrivate;

		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = CreateArmoredDefender();
		ASSERT_THAT(IsTrue(Defender.IsValid()));

		UArcEffectDefinition* RPGStrikeZoned = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Zoned"));
		ASSERT_THAT(IsNotNull(RPGStrikeZoned));
		Harness.Apply(RPGStrikeZoned, Defender, Attacker);
		const FArcDamageDealtPayload* Payload = Harness.FindLastDamage();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Zone == ArcRPGMechanicsTags::HitZone_Torso.GetTag()));
		ASSERT_THAT(IsNear(TorsoMitigated(), Payload->Final, 0.01f));

		UArcEffectDefinition* RPGStrikeZonedPierce = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Zoned_Pierce"));
		ASSERT_THAT(IsNotNull(RPGStrikeZonedPierce));
		ASSERT_THAT(IsNear(TorsoMitigated(), ArcRPGCombatTest::StrikeFinal(Harness, Defender, Attacker, RPGStrikeZonedPierce), 0.01f));
		ASSERT_THAT(IsNear(1000.f - 2.f * TorsoMitigated(), Harness.GetHealth(Defender), 0.01f));
	}

	TEST_METHOD(StruckZoneWithoutPiece_GetsNoArmor)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = CreateArmoredDefender();
		ASSERT_THAT(IsTrue(Defender.IsValid()));

		UArcEffectDefinition* RPGStrikeZonedHead = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_ZonedHead"));
		ASSERT_THAT(IsNotNull(RPGStrikeZonedHead));
		Harness.Apply(RPGStrikeZonedHead, Defender, Attacker);

		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.01f));
	}

	TEST_METHOD(MagicalDamage_IgnoresZoneArmor)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = CreateArmoredDefender();
		ASSERT_THAT(IsTrue(Defender.IsValid()));

		UArcEffectDefinition* RPGStrikeZonedFire = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Zoned_Fire"));
		ASSERT_THAT(IsNotNull(RPGStrikeZonedFire));
		Harness.Apply(RPGStrikeZonedFire, Defender, Attacker);

		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(TaggedArmorStat_AppliesOnlyToMatchingDamageTypes)
	{
		using namespace ArcRPGCombatTestsPrivate;

		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = CreateArmoredDefender(TEXT("ID_Armor_Torso60_Slash"));
		ASSERT_THAT(IsTrue(Defender.IsValid()));

		UArcEffectDefinition* RPGStrikeZoned = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Zoned"));
		ASSERT_THAT(IsNotNull(RPGStrikeZoned));
		ASSERT_THAT(IsNear(TorsoMitigated(), ArcRPGCombatTest::StrikeFinal(Harness, Defender, Attacker, RPGStrikeZoned), 0.01f));
		UArcEffectDefinition* RPGStrikeZonedPierce = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Zoned_Pierce"));
		ASSERT_THAT(IsNotNull(RPGStrikeZonedPierce));
		ASSERT_THAT(IsNear(10.f, ArcRPGCombatTest::StrikeFinal(Harness, Defender, Attacker, RPGStrikeZonedPierce), 0.01f));
	}

	TEST_METHOD(NoStruckZone_GivesNoArmor)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = CreateArmoredDefender();
		ASSERT_THAT(IsTrue(Defender.IsValid()));

		UArcEffectDefinition* RPGStrikePlain = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Plain"));
		ASSERT_THAT(IsNotNull(RPGStrikePlain));
		Harness.Apply(RPGStrikePlain, Defender, Attacker);

		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(DamageOverTime_WithoutGate_GivesNoArmor)
	{
		const FMassEntityHandle Attacker = Harness.CreateEntity();
		const FMassEntityHandle Defender = CreateArmoredDefender();
		ASSERT_THAT(IsTrue(Defender.IsValid()));

		UArcEffectDefinition* RPGDoTZoned = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGDoT_Zoned"));
		ASSERT_THAT(IsNotNull(RPGDoTZoned));
		Harness.Apply(RPGDoTZoned, Defender, Attacker);
		Harness.TickDuration(1.f, Defender);

		ASSERT_THAT(IsNear(990.f, Harness.GetHealth(Defender), 0.001f));
	}
};

TEST_CLASS(ArcRPGCombat_HitGate, "ArcRPGMechanics.Combat.HitGate")
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

	FGameplayTag Outcome(const TCHAR* StrikeName, const ArcCombatTest::FCombatEntityOptions& DefenderOptions,
		float AttackerMiss = 0.f, float AttackerAiming = 0.f)
	{
		ArcCombatTest::FCombatEntityOptions AttackerOptions;
		AttackerOptions.MissChance = AttackerMiss;
		AttackerOptions.AddFragments = [AttackerAiming](UE::Mass::FEntityBuilder& Builder)
		{
			ArcRPGCombatTestsPrivate::AddSkills(Builder, AttackerAiming, 0.f);
		};

		const FMassEntityHandle Attacker = Harness.CreateEntity(AttackerOptions);
		const FMassEntityHandle Defender = Harness.CreateEntity(DefenderOptions);
		UArcEffectDefinition* DefenseRules = ArcRPGCombatTest::LoadCombat(TEXT("ED_DefenseRules_Test"));
		UArcEffectDefinition* AimingMiss = ArcRPGCombatTest::LoadCombat(TEXT("ED_AimingMiss_Test"));
		if (!DefenseRules || !AimingMiss)
		{
			return FGameplayTag();
		}
		Harness.Apply(DefenseRules, Defender, Defender);
		Harness.Apply(AimingMiss, Attacker, Attacker);
		Harness.ClearEvents();
		UArcEffectDefinition* StrikeEffect = ArcRPGCombatTest::LoadCombat(StrikeName);
		if (!StrikeEffect)
		{
			return FGameplayTag();
		}
		Harness.Apply(StrikeEffect, Defender, Attacker);

		if (Harness.CountHits() == 1 && Harness.CountOutcomeEvents() == 1)
		{
			return ArcMassMechanicsTags::HitOutcome_Hit.GetTag();
		}
		if (Harness.CountOutcomeEvents() != 1)
		{
			return FGameplayTag();
		}
		const FGameplayTag OutcomeTags[] = {
			ArcMassMechanicsTags::HitOutcome_Miss, ArcMassMechanicsTags::HitOutcome_Dodge,
			ArcMassMechanicsTags::HitOutcome_Parry, ArcMassMechanicsTags::HitOutcome_Block
		};
		for (const FGameplayTag& Tag : OutcomeTags)
		{
			if (Harness.CountEvents(Tag) == 1)
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}

	ArcCombatTest::FCombatEntityOptions Defender(float Dodge, float Parry, float Block, std::initializer_list<FGameplayTag> DefenderTags)
	{
		ArcCombatTest::FCombatEntityOptions Options;
		Options.DodgeChance = Dodge;
		Options.ParryChance = Parry;
		Options.BlockChance = Block;
		Options.OwnedTags = ArcRPGCombatTest::Tags(DefenderTags);
		return Options;
	}

	TEST_METHOD(Dodge_RollsUnderTheDefenseRules)
	{
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Melee"), Defender(1.f, 0.f, 0.f, {})) == ArcMassMechanicsTags::HitOutcome_Dodge.GetTag()));
	}

	TEST_METHOD(Parry_AgainstArmedAttack_NeedsArmed)
	{
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Melee"), Defender(0.f, 1.f, 0.f, {})) == ArcMassMechanicsTags::HitOutcome_Hit.GetTag()));
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Melee"), Defender(0.f, 1.f, 0.f, {ArcRPGMechanicsTags::Defense_Armed}))
			== ArcMassMechanicsTags::HitOutcome_Parry.GetTag()));
	}

	TEST_METHOD(UnarmedAttack_ParryAndBlockNeedNoArmed)
	{
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Unarmed"), Defender(0.f, 1.f, 0.f, {})) == ArcMassMechanicsTags::HitOutcome_Parry.GetTag()));
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Unarmed"), Defender(0.f, 0.f, 1.f, {})) == ArcMassMechanicsTags::HitOutcome_Block.GetTag()));
	}

	TEST_METHOD(Block_AgainstMelee_NeedsArmed_NotShield)
	{
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Melee"), Defender(0.f, 0.f, 1.f, {})) == ArcMassMechanicsTags::HitOutcome_Hit.GetTag()));
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Melee"), Defender(0.f, 0.f, 1.f, {ArcRPGMechanicsTags::Defense_Armed}))
			== ArcMassMechanicsTags::HitOutcome_Block.GetTag()));
	}

	TEST_METHOD(Block_AgainstRanged_NeedsShield)
	{
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Ranged"), Defender(0.f, 0.f, 1.f, {ArcRPGMechanicsTags::Defense_Armed}))
			== ArcMassMechanicsTags::HitOutcome_Hit.GetTag()));
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Ranged"), Defender(0.f, 0.f, 1.f,
			{ArcRPGMechanicsTags::Defense_Armed, ArcRPGMechanicsTags::Defense_Shield})) == ArcMassMechanicsTags::HitOutcome_Block.GetTag()));
	}

	TEST_METHOD(Aiming_LowersMissChance_ForSpellsOnly)
	{
		const ArcCombatTest::FCombatEntityOptions NoDefense = Defender(0.f, 0.f, 0.f, {});
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Spell"), NoDefense, 1.f, 100.f) == ArcMassMechanicsTags::HitOutcome_Hit.GetTag()));
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Spell"), NoDefense, 1.f, 0.f) == ArcMassMechanicsTags::HitOutcome_Miss.GetTag()));
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Melee"), NoDefense, 1.f, 100.f) == ArcMassMechanicsTags::HitOutcome_Miss.GetTag()));
		ASSERT_THAT(IsTrue(Outcome(TEXT("ED_RPGStrike_Ranged"), NoDefense, 1.f, 100.f) == ArcMassMechanicsTags::HitOutcome_Miss.GetTag()));
	}
};

TEST_CLASS(ArcRPGCombat_IfChannels, "ArcRPGMechanics.Combat.IfChannels")
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

	// Crit chance 0 never crits and an If Crit Chance of +1.0 always does, so no roll outcome depends on the seed.
	// The attacker's Crit Multiplier 1.5 is the full crit factor.
	FMassEntityHandle CreateAttacker()
	{
		ArcCombatTest::FCombatEntityOptions Options;
		Options.CritChance = 0.f;
		Options.CritMultiplier = 1.5f;
		return Harness.CreateEntity(Options);
	}

	const FArcDamageDealtPayload* Strike(FMassEntityHandle Defender, FMassEntityHandle Attacker, const FInstancedStruct& SourceData = FInstancedStruct())
	{
		UArcEffectDefinition* PlainStrike = ArcRPGCombatTest::LoadCombat(TEXT("ED_RPGStrike_Plain"));
		if (!PlainStrike)
		{
			return nullptr;
		}
		const int32 EventsBefore = Harness.CountDamageEvents();
		Harness.Apply(PlainStrike, Defender, Attacker, SourceData);
		return Harness.CountDamageEvents() == EventsBefore + 1 ? Harness.FindLastDamage() : nullptr;
	}

	TEST_METHOD(CritChance_IsAddedBeforeTheRoll)
	{
		const FMassEntityHandle Attacker = CreateAttacker();
		const FMassEntityHandle Defender = Harness.CreateEntity();

		const FArcDamageDealtPayload* Baseline = Strike(Defender, Attacker);
		ASSERT_THAT(IsNotNull(Baseline));
		ASSERT_THAT(IsFalse(Baseline->bCrit));
		ASSERT_THAT(IsNear(10.f, Baseline->Final, 0.001f));

		UArcEffectDefinition* HostIfCritChance = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_CritChance"));
		ASSERT_THAT(IsNotNull(HostIfCritChance));
		Harness.Apply(HostIfCritChance, Attacker, Attacker);
		const FArcDamageDealtPayload* Crit = Strike(Defender, Attacker);
		ASSERT_THAT(IsNotNull(Crit));
		ASSERT_THAT(IsTrue(Crit->bCrit));
		ASSERT_THAT(IsNear(15.f, Crit->Final, 0.001f));
		ASSERT_THAT(IsNear(975.f, Harness.GetHealth(Defender), 0.001f));
	}

	TEST_METHOD(CritDamage_RaisesTheCritMultiplier_OnlyOnACrit)
	{
		using namespace ArcRPGCombatTestsPrivate;
		const FMassEntityHandle Defender = Harness.CreateEntity();

		const FMassEntityHandle Critting = CreateAttacker();
		UArcEffectDefinition* HostIfCritChance = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_CritChance"));
		ASSERT_THAT(IsNotNull(HostIfCritChance));
		Harness.Apply(HostIfCritChance, Critting, Critting);
		UArcEffectDefinition* HostIfCritDamage = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_CritDamage"));
		ASSERT_THAT(IsNotNull(HostIfCritDamage));
		Harness.Apply(HostIfCritDamage, Critting, Critting);
		const FArcDamageDealtPayload* Crit = Strike(Defender, Critting);
		ASSERT_THAT(IsNotNull(Crit));
		ASSERT_THAT(IsTrue(Crit->bCrit));
		ASSERT_THAT(IsNear(20.f, Crit->Final, 0.001f));

		const FMassEntityHandle NoCrit = CreateAttacker();
		UArcEffectDefinition* HostIfCritDamage2 = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_CritDamage"));
		ASSERT_THAT(IsNotNull(HostIfCritDamage2));
		Harness.Apply(HostIfCritDamage2, NoCrit, NoCrit);
		const FArcDamageDealtPayload* Plain = Strike(Defender, NoCrit);
		ASSERT_THAT(IsNotNull(Plain));
		ASSERT_THAT(IsFalse(Plain->bCrit));
		ASSERT_THAT(IsNear(10.f, Plain->Final, 0.001f));
	}

	// The host fails CritChanceNeedsRoll in the editor, so it stays code-built (decision 7). Before the roll bCrit is still false,
	// so a NotCrit filter would be met if the bCritRolled gate were missing and the +1.0 would force a crit.
	TEST_METHOD(CritRolledGate_CritFilterIsUnmetForCritChance)
	{
		FArcTriggerCondition_HitResult NotCrit;
		NotCrit.Crit = EArcTriggerCritFilter::NotCrit;

		FArcIfModifier CritChance;
		CritChance.Channel = EArcIfChannel::CritChance;
		CritChance.Value = 1.f;

		FArcEffectTask_TriggerIf Task;
		Task.Role = EArcEventRole::Instigator;
		Task.Conditions.Add(FInstancedStruct::Make(NotCrit));
		Task.Modifiers.Add(CritChance);
		UArcEffectDefinition* Host = NewObject<UArcEffectDefinition>();
		Host->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		Host->Tasks.Add(FInstancedStruct::Make(Task));

		const FMassEntityHandle Defender = Harness.CreateEntity();
		const FMassEntityHandle Gated = CreateAttacker();
		Harness.Apply(Host, Gated, Gated);
		const FArcDamageDealtPayload* GatedHit = Strike(Defender, Gated);
		ASSERT_THAT(IsNotNull(GatedHit));
		ASSERT_THAT(IsFalse(GatedHit->bCrit));
		ASSERT_THAT(IsNear(10.f, GatedHit->Final, 0.001f));

		const FMassEntityHandle Ungated = CreateAttacker();
		UArcEffectDefinition* HostIfCritChance = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_CritChance"));
		ASSERT_THAT(IsNotNull(HostIfCritChance));
		Harness.Apply(HostIfCritChance, Ungated, Ungated);
		const FArcDamageDealtPayload* UngatedHit = Strike(Defender, Ungated);
		ASSERT_THAT(IsNotNull(UngatedHit));
		ASSERT_THAT(IsTrue(UngatedHit->bCrit));
	}

	TEST_METHOD(SameSourceItem_HitFromTheHostItem_AppliesTheModifier)
	{
		using namespace ArcRPGCombatTestsPrivate;
		FArcItemData Sword;
		const FArcItemDefinitionData* WeaponTest = ArcRPGMechanicsTestHarness::LoadItem(TEXT("ID_Weapon_Test"));
		ASSERT_THAT(IsNotNull(WeaponTest));
		ArcRPGMechanicsTestHarness::MakeItem(WeaponTest, Sword);

		const FMassEntityHandle Attacker = CreateAttacker();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		UArcEffectDefinition* HostIfSameSourceItem = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_SameSourceItem"));
		ASSERT_THAT(IsNotNull(HostIfSameSourceItem));
		Harness.Apply(HostIfSameSourceItem, Attacker, Attacker, ArcRPGCombatTest::MakeItemSource(Sword));

		const FArcDamageDealtPayload* Hit = Strike(Defender, Attacker, ArcRPGCombatTest::MakeItemSource(Sword));
		ASSERT_THAT(IsNotNull(Hit));
		ASSERT_THAT(IsNear(15.f, Hit->Final, 0.001f));
	}

	TEST_METHOD(SameSourceItem_HitFromAnotherItemOrNoItem_DoesNotApply)
	{
		using namespace ArcRPGCombatTestsPrivate;
		const FArcItemDefinitionData* WeaponDefinition = ArcRPGMechanicsTestHarness::LoadItem(TEXT("ID_Weapon_Test"));
		ASSERT_THAT(IsNotNull(WeaponDefinition));
		FArcItemData Sword;
		ArcRPGMechanicsTestHarness::MakeItem(WeaponDefinition, Sword);
		FArcItemData OtherSword;
		ArcRPGMechanicsTestHarness::MakeItem(WeaponDefinition, OtherSword);
		ASSERT_THAT(IsFalse(Sword.ItemId == OtherSword.ItemId));

		const FMassEntityHandle Attacker = CreateAttacker();
		const FMassEntityHandle Defender = Harness.CreateEntity();
		UArcEffectDefinition* HostIfSameSourceItem = ArcRPGCombatTest::LoadTrigger(TEXT("ED_Host_If_SameSourceItem"));
		ASSERT_THAT(IsNotNull(HostIfSameSourceItem));
		Harness.Apply(HostIfSameSourceItem, Attacker, Attacker, ArcRPGCombatTest::MakeItemSource(Sword));

		const FArcDamageDealtPayload* OtherItemHit = Strike(Defender, Attacker, ArcRPGCombatTest::MakeItemSource(OtherSword));
		ASSERT_THAT(IsNotNull(OtherItemHit));
		ASSERT_THAT(IsNear(10.f, OtherItemHit->Final, 0.001f));

		const FArcDamageDealtPayload* NoItemHit = Strike(Defender, Attacker);
		ASSERT_THAT(IsNotNull(NoItemHit));
		ASSERT_THAT(IsNear(10.f, NoItemHit->Final, 0.001f));
	}
};

TEST_CLASS(ArcRPGCombat_HitChainDerivation, "ArcRPGMechanics.Combat.HitChainDerivation")
{
	TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	FActorTestSpawner Spawner;
	ArcCombatTest::FCombatHarness Harness;
	UArcAttributeObserverProcessor* ObserverProcessor = nullptr;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		ArcRPGCombatTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));

		ObserverProcessor = NewObject<UArcAttributeObserverProcessor>();
		ObserverProcessor->CallInitialize(Harness.EntityManager->GetOwner(), Harness.EntityManager->AsShared());
	}

	UArcAttributeHandlerConfig* LoadConfig()
	{
		return ArcRPGMechTest::LoadTestAsset<UArcAttributeHandlerConfig>(TEXT("Handlers/AHC_RPG_DerivedChance"));
	}

	FMassEntityHandle CreateEntity(UArcAttributeHandlerConfig* Config, float Dodging, float TwoHanded)
	{
		ArcCombatTest::FCombatEntityOptions Options;
		Options.HandlerConfigOverride = Config;
		Options.AddFragments = [Dodging, TwoHanded](UE::Mass::FEntityBuilder& Builder)
		{
			FArcAttributeSet_RPGSkills Skills;
			Skills.InitDodging(Dodging);
			Skills.InitTwoHanded(TwoHanded);
			Builder.Add<FArcAttributeSet_RPGSkills>(Skills);
		};
		return Harness.CreateEntity(Options);
	}

	void Initialize(FMassEntityHandle Entity, UArcAttributeHandlerConfig* Config, const FArcAttributeRef& Attribute)
	{
		FArcAttributeSet_HitChain* HitChain = ArcRPGCombatTestsPrivate::GetHitChain(*Harness.EntityManager, Entity);
		FArcAttributeSet_Crit* Crit = ArcRPGCombatTestsPrivate::GetCrit(*Harness.EntityManager, Entity);
		check(HitChain && Crit);
		uint8* SetMemory = Attribute.FragmentType == FArcAttributeSet_Crit::StaticStruct()
			? reinterpret_cast<uint8*>(Crit) : reinterpret_cast<uint8*>(HitChain);
		FArcAttributeHandlerContext Context(*Harness.EntityManager, nullptr, Entity, SetMemory, Config, Attribute);
		Context.InitInline(Attribute, 0.f);
	}

	void ObserveChanges(FMassEntityHandle Entity)
	{
		UMassSignalSubsystem* Signals = Harness.World->GetSubsystem<UMassSignalSubsystem>();
		Signals->SignalEntity(UE::ArcMassAbilities::Signals::AttributeChanged, Entity);
		FMassProcessingContext ProcessingContext(*Harness.EntityManager, 0.f);
		UE::Mass::Executor::Run(*ObserverProcessor, ProcessingContext);
		Harness.Settle(Entity);
	}

	TEST_METHOD(Initialize_WritesBasePlusPerPointTerm)
	{
		UArcAttributeHandlerConfig* Config = LoadConfig();
		ASSERT_THAT(IsNotNull(Config));
		const FMassEntityHandle Entity = CreateEntity(Config, 20.f, 10.f);

		Initialize(Entity, Config, FArcAttributeSet_HitChain::GetDodgeChanceAttribute());

		const FArcAttributeSet_HitChain* HitChain = ArcRPGCombatTestsPrivate::GetHitChain(*Harness.EntityManager, Entity);
		ASSERT_THAT(IsNear(0.25f, HitChain->DodgeChance.BaseValue, 0.0001f));
	}

	TEST_METHOD(Initialize_WritesBasePlusPerModifierTerm)
	{
		UArcAttributeHandlerConfig* Config = LoadConfig();
		ASSERT_THAT(IsNotNull(Config));
		const FMassEntityHandle Entity = CreateEntity(Config, 0.f, 14.f);

		Initialize(Entity, Config, FArcAttributeSet_Crit::GetCritChanceAttribute());

		const FArcAttributeSet_Crit* Crit = ArcRPGCombatTestsPrivate::GetCrit(*Harness.EntityManager, Entity);
		ASSERT_THAT(IsNear(0.2f, Crit->CritChance.BaseValue, 0.0001f));
	}

	TEST_METHOD(ObservedInputChange_RederivesTheChance)
	{
		UArcAttributeHandlerConfig* Config = LoadConfig();
		ASSERT_THAT(IsNotNull(Config));
		const FMassEntityHandle Entity = CreateEntity(Config, 20.f, 10.f);
		Initialize(Entity, Config, FArcAttributeSet_HitChain::GetDodgeChanceAttribute());

		ArcAttributes::EnqueueInstantModifier(*Harness.EntityManager, Entity, FArcAttributeSet_RPGSkills::GetDodgingAttribute(),
			EArcModifierOp::Override, 50.f, EArcAttributeOpType::Execute);
		Harness.Settle(Entity);
		ObserveChanges(Entity);

		const FArcAttributeSet_RPGSkills* Skills = ArcRPGCombatTestsPrivate::GetSkills(*Harness.EntityManager, Entity);
		ASSERT_THAT(IsNear(50.f, Skills->Dodging.BaseValue, 0.0001f));
		const FArcAttributeSet_HitChain* HitChain = ArcRPGCombatTestsPrivate::GetHitChain(*Harness.EntityManager, Entity);
		ASSERT_THAT(IsNear(0.55f, HitChain->DodgeChance.BaseValue, 0.0001f));
	}

	// The observer processor builds the handler context; each Linear handler writes the attribute it is keyed on (Context.Attribute).
	TEST_METHOD(ObservedInputChange_EachLinearHandlerWritesItsOwnAttribute)
	{
		UArcAttributeHandlerConfig* Config = LoadConfig();
		ASSERT_THAT(IsNotNull(Config));
		const FMassEntityHandle Entity = CreateEntity(Config, 20.f, 14.f);
		Initialize(Entity, Config, FArcAttributeSet_HitChain::GetDodgeChanceAttribute());
		Initialize(Entity, Config, FArcAttributeSet_Crit::GetCritChanceAttribute());
		const FArcAttributeSet_HitChain* HitChain = ArcRPGCombatTestsPrivate::GetHitChain(*Harness.EntityManager, Entity);
		const FArcAttributeSet_Crit* Crit = ArcRPGCombatTestsPrivate::GetCrit(*Harness.EntityManager, Entity);
		ASSERT_THAT(IsNear(0.25f, HitChain->DodgeChance.BaseValue, 0.0001f));
		ASSERT_THAT(IsNear(0.2f, Crit->CritChance.BaseValue, 0.0001f));

		ArcAttributes::EnqueueInstantModifier(*Harness.EntityManager, Entity, FArcAttributeSet_RPGSkills::GetDodgingAttribute(),
			EArcModifierOp::Override, 50.f, EArcAttributeOpType::Execute);
		Harness.Settle(Entity);
		ObserveChanges(Entity);
		HitChain = ArcRPGCombatTestsPrivate::GetHitChain(*Harness.EntityManager, Entity);
		Crit = ArcRPGCombatTestsPrivate::GetCrit(*Harness.EntityManager, Entity);
		ASSERT_THAT(IsNear(0.55f, HitChain->DodgeChance.BaseValue, 0.0001f));
		ASSERT_THAT(IsNear(0.2f, Crit->CritChance.BaseValue, 0.0001f));

		ArcAttributes::EnqueueInstantModifier(*Harness.EntityManager, Entity, FArcAttributeSet_RPGSkills::GetTwoHandedAttribute(),
			EArcModifierOp::Override, 18.f, EArcAttributeOpType::Execute);
		Harness.Settle(Entity);
		ObserveChanges(Entity);
		HitChain = ArcRPGCombatTestsPrivate::GetHitChain(*Harness.EntityManager, Entity);
		Crit = ArcRPGCombatTestsPrivate::GetCrit(*Harness.EntityManager, Entity);
		ASSERT_THAT(IsNear(0.3f, Crit->CritChance.BaseValue, 0.0001f));
		ASSERT_THAT(IsNear(0.55f, HitChain->DodgeChance.BaseValue, 0.0001f));
	}
};
