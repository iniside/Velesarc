// Copyright Lukasz Baran. All Rights Reserved.
//
// L3 end-to-end coverage of the D-100 regen/degen pips (Plan 13 Step 6). Real assets: the Linear pip handlers and the
// signed AddEnergy/AddStamina metas (AHC_RPG_Pips_Test), the Pips resource config (DA_PipsResources_Test) and the
// degen effect (ED_EnergyDegenPips_Test), driven by the production resource processor and the pending-ops drain.
// Expected values come from the plan's case table: 4 pips x 0.75 = 3/s at core stats 10. Topology: standalone world
// (no replication involved).

#include "CQTest.h"
#include "ArcRPGMechanicsTestHarness.h"
#include "ArcRPGMechTestSupport.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Resources/ArcResourceProcessor.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcAttributeSet_RPGCoreStats.h"
#include "Attributes/ArcAttributeSet_RPGEnergy.h"
#include "Attributes/ArcAttributeSet_RPGStamina.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "GameplayTagContainer.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "UObject/Package.h"

namespace ArcRPGPipsTestsPrivate
{
	struct FPipsEntityOptions
	{
		float Wisdom = 10.f;
		float Magic = 10.f;
		float Constitution = 10.f;
		float Agility = 10.f;
		float Energy = 0.f;
		float Stamina = 0.f;
	};

	void InitAttribute(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UArcAttributeHandlerConfig* HandlerConfig,
		void* SetMemory, const FArcAttributeRef& Attribute)
	{
		FArcAttributeHandlerContext Context(EntityManager, nullptr, Entity, static_cast<uint8*>(SetMemory), HandlerConfig, Attribute);
		Context.InitInline(Attribute, 0.f);
	}
}

TEST_CLASS(ArcRPGPipsRegen, "ArcRPGMechanics.Resources.Pips")
{
	FActorTestSpawner Spawner;
	ArcRPGMechanicsTestHarness::FHarness Harness;
	UArcAttributeHandlerConfig* PipsHandlers = nullptr;
	UArcResourceConfig* PipsResources = nullptr;
	UArcEffectDefinition* DegenEffect = nullptr;
	UArcResourceProcessor* ResourceProcessor = nullptr;
	UArcAttributeAggregatorProcessor* AggregatorProcessor = nullptr;
	TArray<FMassEntityHandle> Entities;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		PipsHandlers = ArcRPGMechTest::LoadTestAsset<UArcAttributeHandlerConfig>(TEXT("Handlers/AHC_RPG_Pips_Test"));
		PipsResources = ArcRPGMechTest::LoadTestAsset<UArcResourceConfig>(TEXT("Resources/DA_PipsResources_Test"));
		DegenEffect = ArcRPGMechTest::LoadTestAsset<UArcEffectDefinition>(TEXT("Resources/ED_EnergyDegenPips_Test"));
		ASSERT_THAT(IsNotNull(PipsHandlers));
		ASSERT_THAT(IsNotNull(PipsResources));
		ASSERT_THAT(IsNotNull(DegenEffect));
		ASSERT_THAT(AreEqual(2, PipsResources->Resources.Num()));

		ResourceProcessor = NewObject<UArcResourceProcessor>();
		ResourceProcessor->CallInitialize(Harness.EntityManager->GetOwner(), Harness.EntityManager->AsShared());
		AggregatorProcessor = NewObject<UArcAttributeAggregatorProcessor>();
		AggregatorProcessor->CallInitialize(Harness.EntityManager->GetOwner(), Harness.EntityManager->AsShared());
	}

	FMassEntityHandle CreateEntity(UArcResourceConfig* Config, const ArcRPGPipsTestsPrivate::FPipsEntityOptions& Options)
	{
		FArcAttributeSet_RPGCoreStats CoreStats;
		CoreStats.InitStrength(10.f);
		CoreStats.InitDexterity(10.f);
		CoreStats.InitConstitution(Options.Constitution);
		CoreStats.InitAgility(Options.Agility);
		CoreStats.InitPerception(10.f);
		CoreStats.InitMagic(Options.Magic);
		CoreStats.InitIntelligence(10.f);
		CoreStats.InitWillpower(10.f);
		CoreStats.InitWisdom(Options.Wisdom);

		FArcAttributeSet_RPGEnergy Energy;
		Energy.InitEnergy(Options.Energy);
		Energy.InitAddEnergy(0.f);
		Energy.InitRemoveEnergy(0.f);

		FArcAttributeSet_RPGStamina Stamina;
		Stamina.InitStamina(Options.Stamina);
		Stamina.InitAddStamina(0.f);
		Stamina.InitRemoveStamina(0.f);

		FArcAttributeHandlerSharedFragment HandlerShared;
		HandlerShared.Config = PipsHandlers;
		FArcResourceConfigSharedFragment ResourceShared;
		ResourceShared.Config = Config;

		UE::Mass::FEntityBuilder Builder(*Harness.EntityManager);
		Builder.Add<FArcAttributeSet_RPGCoreStats>(CoreStats)
			.Add<FArcAttributeSet_RPGEnergy>(Energy)
			.Add<FArcAttributeSet_RPGStamina>(Stamina)
			.Add<FArcAggregatorFragment>()
			.Add<FArcPendingAttributeOpsFragment>()
			.Add<FArcOwnedTagsFragment>()
			.Add<FArcEffectStackFragment>()
			.Add<FArcResourceStateFragment>()
			.Add<FArcAttributeHandlerSharedFragment>(HandlerShared)
			.Add<FArcResourceConfigSharedFragment>(ResourceShared);
		const FMassEntityHandle Entity = Builder.Commit();

		// The handlers derive their base values the way the initial-attributes observer would at spawn.
		FArcAttributeSet_RPGEnergy* EnergySet = Harness.GetEnergy(Entity);
		FArcAttributeSet_RPGStamina* StaminaSet = Harness.GetStamina(Entity);
		FMassEntityManager& EntityManager = *Harness.EntityManager;
		ArcRPGPipsTestsPrivate::InitAttribute(EntityManager, Entity, PipsHandlers, EnergySet, FArcAttributeSet_RPGEnergy::GetMaxEnergyAttribute());
		ArcRPGPipsTestsPrivate::InitAttribute(EntityManager, Entity, PipsHandlers, EnergySet, FArcAttributeSet_RPGEnergy::GetEnergyRegenPipsAttribute());
		ArcRPGPipsTestsPrivate::InitAttribute(EntityManager, Entity, PipsHandlers, EnergySet, FArcAttributeSet_RPGEnergy::GetEnergyPerPipAttribute());
		ArcRPGPipsTestsPrivate::InitAttribute(EntityManager, Entity, PipsHandlers, StaminaSet, FArcAttributeSet_RPGStamina::GetMaxStaminaAttribute());
		ArcRPGPipsTestsPrivate::InitAttribute(EntityManager, Entity, PipsHandlers, StaminaSet, FArcAttributeSet_RPGStamina::GetStaminaRegenPipsAttribute());
		ArcRPGPipsTestsPrivate::InitAttribute(EntityManager, Entity, PipsHandlers, StaminaSet, FArcAttributeSet_RPGStamina::GetStaminaPerPipAttribute());

		Entities.Add(Entity);
		return Entity;
	}

	void Tick(float DeltaSeconds = 1.f, int32 Count = 1)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FMassProcessingContext ProcessingContext(*Harness.EntityManager, DeltaSeconds);
			UE::Mass::Executor::Run(*ResourceProcessor, ProcessingContext);
			for (const FMassEntityHandle Entity : Entities)
			{
				Harness.DrainPendingOps(Entity);
			}
		}
	}

	bool ApplyDegen(FMassEntityHandle Entity)
	{
		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Harness.EntityManager, Entity, DegenEffect, Entity);
		Harness.DrainPendingOps(Entity);
		// The effect's modifier reaches the attribute's current value through the aggregator pass.
		Harness.SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
		FMassProcessingContext AggregatorContext(*Harness.EntityManager, 0.f);
		UE::Mass::Executor::Run(*AggregatorProcessor, AggregatorContext);
		Harness.DrainPendingOps(Entity);
		return Result.bApplied;
	}

	void GrantBlockingTag(FMassEntityHandle Entity)
	{
		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Entity, FGameplayTagContainer(ArcMassMechanicsTags::Condition_Burning));
	}

	// A transient copy of the loaded resource config whose every def is blocked by Condition_Burning; the asset stays untouched.
	UArcResourceConfig* MakeBlockedConfig()
	{
		UArcResourceConfig* Copy = DuplicateObject<UArcResourceConfig>(PipsResources, GetTransientPackage());
		for (FArcResourceDef& Resource : Copy->Resources)
		{
			Resource.BlockedBy.AddTag(ArcMassMechanicsTags::Condition_Burning);
		}
		return Copy;
	}

	TEST_METHOD(CoreStatsTen_HandlersDerivePipBasesFromTheAsset)
	{
		const FMassEntityHandle Entity = CreateEntity(PipsResources, {});

		const FArcAttributeSet_RPGEnergy* Energy = Harness.GetEnergy(Entity);
		const FArcAttributeSet_RPGStamina* Stamina = Harness.GetStamina(Entity);
		ASSERT_THAT(IsNear(4.f, Energy->GetEnergyRegenPips(), 0.001f));
		ASSERT_THAT(IsNear(0.75f, Energy->GetEnergyPerPip(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Energy->GetEnergyDegenPips(), 0.001f));
		ASSERT_THAT(IsNear(4.f, Stamina->GetStaminaRegenPips(), 0.001f));
		ASSERT_THAT(IsNear(0.75f, Stamina->GetStaminaPerPip(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Stamina->GetStaminaDegenPips(), 0.001f));
		ASSERT_THAT(IsTrue(Energy->GetMaxEnergy() > 30.f));
		ASSERT_THAT(IsTrue(Stamina->GetMaxStamina() > 30.f));
	}

	TEST_METHOD(CoreStatsTen_EnergyAndStaminaRegenThreePerSecondFromZero)
	{
		const FMassEntityHandle Entity = CreateEntity(PipsResources, {});

		const float Expected[] = {3.f, 6.f, 9.f};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		{
			Tick();
			ASSERT_THAT(IsNear(Expected[Index], Harness.GetEnergy(Entity)->GetEnergy(), 0.001f));
			ASSERT_THAT(IsNear(Expected[Index], Harness.GetStamina(Entity)->GetStamina(), 0.001f));
		}
	}

	TEST_METHOD(Wisdom14_GivesSixEnergyPips_StaminaUnchanged)
	{
		ArcRPGPipsTestsPrivate::FPipsEntityOptions Options;
		Options.Wisdom = 14.f;
		const FMassEntityHandle Entity = CreateEntity(PipsResources, Options);

		ASSERT_THAT(IsNear(6.f, Harness.GetEnergy(Entity)->GetEnergyRegenPips(), 0.001f));
		Tick();
		ASSERT_THAT(IsNear(4.5f, Harness.GetEnergy(Entity)->GetEnergy(), 0.001f));
		ASSERT_THAT(IsNear(3.f, Harness.GetStamina(Entity)->GetStamina(), 0.001f));
	}

	TEST_METHOD(Magic14_RaisesEnergyPerPipToZeroPointNine_StaminaUnchanged)
	{
		ArcRPGPipsTestsPrivate::FPipsEntityOptions Options;
		Options.Magic = 14.f;
		const FMassEntityHandle Entity = CreateEntity(PipsResources, Options);

		ASSERT_THAT(IsNear(0.9f, Harness.GetEnergy(Entity)->GetEnergyPerPip(), 0.001f));
		Tick();
		ASSERT_THAT(IsNear(3.6f, Harness.GetEnergy(Entity)->GetEnergy(), 0.001f));
		ASSERT_THAT(IsNear(3.f, Harness.GetStamina(Entity)->GetStamina(), 0.001f));
	}

	TEST_METHOD(Constitution14_GivesSixStaminaPips_EnergyUnchanged)
	{
		ArcRPGPipsTestsPrivate::FPipsEntityOptions Options;
		Options.Constitution = 14.f;
		const FMassEntityHandle Entity = CreateEntity(PipsResources, Options);

		ASSERT_THAT(IsNear(6.f, Harness.GetStamina(Entity)->GetStaminaRegenPips(), 0.001f));
		Tick();
		ASSERT_THAT(IsNear(4.5f, Harness.GetStamina(Entity)->GetStamina(), 0.001f));
		ASSERT_THAT(IsNear(3.f, Harness.GetEnergy(Entity)->GetEnergy(), 0.001f));
	}

	TEST_METHOD(Agility14_RaisesStaminaPerPipToZeroPointNine_EnergyUnchanged)
	{
		ArcRPGPipsTestsPrivate::FPipsEntityOptions Options;
		Options.Agility = 14.f;
		const FMassEntityHandle Entity = CreateEntity(PipsResources, Options);

		ASSERT_THAT(IsNear(0.9f, Harness.GetStamina(Entity)->GetStaminaPerPip(), 0.001f));
		Tick();
		ASSERT_THAT(IsNear(3.6f, Harness.GetStamina(Entity)->GetStamina(), 0.001f));
		ASSERT_THAT(IsNear(3.f, Harness.GetEnergy(Entity)->GetEnergy(), 0.001f));
	}

	TEST_METHOD(DegenEffect_DrainsEnergyToZeroAtOnePointFivePerSecond_AndStopsThere)
	{
		const FMassEntityHandle Entity = CreateEntity(PipsResources, {});
		Tick(1.f, 2);
		ASSERT_THAT(IsNear(6.f, Harness.GetEnergy(Entity)->GetEnergy(), 0.001f));

		ASSERT_THAT(IsTrue(ApplyDegen(Entity)));
		ASSERT_THAT(IsNear(6.f, Harness.GetEnergy(Entity)->GetEnergyDegenPips(), 0.001f));

		// Regen 4 - Degen 6 = -2 pips x 0.75 = -1.5/s; stamina has no degen and keeps regenerating.
		const float Expected[] = {4.5f, 3.f, 1.5f, 0.f, 0.f, 0.f};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		{
			Tick();
			ASSERT_THAT(IsNear(Expected[Index], Harness.GetEnergy(Entity)->GetEnergy(), 0.001f));
		}
		ASSERT_THAT(IsNear(24.f, Harness.GetStamina(Entity)->GetStamina(), 0.001f));
	}

	TEST_METHOD(BlockedBy_StopsRegen)
	{
		UArcResourceConfig* Blocked = MakeBlockedConfig();
		const FMassEntityHandle Entity = CreateEntity(Blocked, {});
		GrantBlockingTag(Entity);

		Tick(1.f, 3);
		ASSERT_THAT(IsNear(0.f, Harness.GetEnergy(Entity)->GetEnergy(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Harness.GetStamina(Entity)->GetStamina(), 0.001f));
	}

	TEST_METHOD(BlockedBy_DoesNotStopTheDegenDrain_WhileRegenStaysStopped)
	{
		UArcResourceConfig* Blocked = MakeBlockedConfig();
		ArcRPGPipsTestsPrivate::FPipsEntityOptions Options;
		Options.Energy = 6.f;
		Options.Stamina = 6.f;
		const FMassEntityHandle Entity = CreateEntity(Blocked, Options);
		GrantBlockingTag(Entity);
		ASSERT_THAT(IsTrue(ApplyDegen(Entity)));

		const float Expected[] = {4.5f, 3.f, 1.5f, 0.f, 0.f};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		{
			Tick();
			ASSERT_THAT(IsNear(Expected[Index], Harness.GetEnergy(Entity)->GetEnergy(), 0.001f));
			ASSERT_THAT(IsNear(6.f, Harness.GetStamina(Entity)->GetStamina(), 0.001f));
		}
	}
};
