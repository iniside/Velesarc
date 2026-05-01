// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcConditionEffectsTestHelpers.h"
#include "ArcConditionEffectsSubsystem.h"
#include "ArcConditionFragments.h"
#include "ArcConditionGameplayTags.h"
#include "ArcConditionMassEffectsConfig.h"
#include "ArcConditionReactionProcessor.h"
#include "ArcConditionTickProcessors.h"
#include "ArcConditionTypes.h"
#include "ArcThermalConditionProcessor.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectTypes.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "StructUtils/InstancedStruct.h"

// ============================================================================
// End-to-End — Cross-Interaction and Effect Lifecycle Tests
// ============================================================================

TEST_CLASS(ArcCondition_EndToEnd, "ArcConditionEffects.Integration.EndToEnd")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcConditionEffectsSubsystem* Subsystem = nullptr;
	UArcThermalConditionProcessor* ThermalProcessor = nullptr;
	UArcConditionReactionProcessor* ReactionProcessor = nullptr;
	UArcConditionTickProcessor* BurningTickProcessor = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();

		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);

		Subsystem = Spawner.GetWorld().GetSubsystem<UArcConditionEffectsSubsystem>();
		check(Subsystem);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();

		ThermalProcessor = NewObject<UArcThermalConditionProcessor>();
		ThermalProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		ReactionProcessor = NewObject<UArcConditionReactionProcessor>();
		ReactionProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		BurningTickProcessor = NewObject<UArcConditionTickProcessor>();
		BurningTickProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		UArcConditionMassEffectsConfig* MassConfig = NewObject<UArcConditionMassEffectsConfig>();
		Subsystem->SetMassEffectsConfig(MassConfig);
	}

	// Creates a UArcEffectDefinition with HasDuration type, long duration, and an FArcEffectComp_EffectTags
	// component containing GrantedTag in OwnedTags — required for RemoveEffectsByTag to locate this effect.
	UArcEffectDefinition* CreateTestEffect(FGameplayTag GrantedTag)
	{
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 999.f;

		FArcEffectComp_EffectTags TagsComp;
		TagsComp.OwnedTags.AddTag(GrantedTag);
		Effect->Components.Add(FInstancedStruct::Make(TagsComp));

		return Effect;
	}

	// Apply a condition via subsystem and run the thermal processor.
	void ApplyConditionAndProcess(FMassEntityHandle Entity, EArcConditionType Type, float Amount)
	{
		Subsystem->ApplyCondition(Entity, Type, Amount);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*ThermalProcessor, ProcessingContext);
	}

	// Signal ConditionStateChanged and ConditionOverloadChanged, then run the reaction processor.
	void RunReaction(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcConditionEffects::Signals::ConditionStateChanged, Entity);
		SignalSubsystem->SignalEntity(UE::ArcConditionEffects::Signals::ConditionOverloadChanged, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*ReactionProcessor, ProcessingContext);
	}

	int32 CountActiveEffects(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		const FArcEffectStackFragment* EffectStack = View.GetFragmentDataPtr<FArcEffectStackFragment>();
		if (!EffectStack)
		{
			return 0;
		}
		return EffectStack->ActiveEffects.Num();
	}

	// ------------------------------------------------------------------
	// FireCauterizeBleeding_RemovesBleedingEffect
	//
	// Activate bleeding -> reaction applies bleeding activation effect (1 effect).
	// Apply fire -> thermal cauterizes bleeding (clears saturation + deactivates).
	// Run reaction again -> detects deactivation -> removes effect by tag (0 effects).
	// ------------------------------------------------------------------
	TEST_METHOD(FireCauterizeBleeding_RemovesBleedingEffect)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcEffectDefinition* BleedingEffect = CreateTestEffect(ArcConditionTags::GetActiveTag(EArcConditionType::Bleeding));

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();
		FArcConditionMassEffectEntry& BleedingEntry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Bleeding);
		BleedingEntry.ActivationEffect = BleedingEffect;

		// Manually activate bleeding so reaction processor sees it as newly active.
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Bleeding].bActive = true;
			Frag->States[(int32)EArcConditionType::Bleeding].Saturation = 40.f;
		}

		RunReaction(Entity);
		ASSERT_THAT(AreEqual(1, CountActiveEffects(Entity)));

		// Apply fire — thermal processor cauterizes bleeding.
		ApplyConditionAndProcess(Entity, EArcConditionType::Burning, 10.f);

		// Verify bleeding is cleared by the thermal processor.
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			ASSERT_THAT(IsNear(0.f, Frag->States[(int32)EArcConditionType::Bleeding].Saturation, 0.001f));
			ASSERT_THAT(IsFalse(Frag->States[(int32)EArcConditionType::Bleeding].bActive));
		}

		// Reaction processor detects deactivation and removes bleeding effect.
		RunReaction(Entity);
		ASSERT_THAT(AreEqual(0, CountActiveEffects(Entity)));
	}

	// ------------------------------------------------------------------
	// FireSterilizeDisease_RemovesDiseaseEffect
	//
	// Activate disease -> reaction applies disease activation effect (1 effect).
	// Apply fire -> thermal sterilizes disease (clears saturation + deactivates).
	// Run reaction again -> detects deactivation -> removes effect by tag (0 effects).
	// ------------------------------------------------------------------
	TEST_METHOD(FireSterilizeDisease_RemovesDiseaseEffect)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcEffectDefinition* DiseaseEffect = CreateTestEffect(ArcConditionTags::GetActiveTag(EArcConditionType::Diseased));

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();
		FArcConditionMassEffectEntry& DiseaseEntry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Diseased);
		DiseaseEntry.ActivationEffect = DiseaseEffect;

		// Manually activate disease so reaction processor sees it as newly active.
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Diseased].bActive = true;
			Frag->States[(int32)EArcConditionType::Diseased].Saturation = 50.f;
		}

		RunReaction(Entity);
		ASSERT_THAT(AreEqual(1, CountActiveEffects(Entity)));

		// Apply fire — thermal processor sterilizes disease.
		ApplyConditionAndProcess(Entity, EArcConditionType::Burning, 10.f);

		// Verify disease is cleared by the thermal processor.
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			ASSERT_THAT(IsNear(0.f, Frag->States[(int32)EArcConditionType::Diseased].Saturation, 0.001f));
		}

		// Reaction processor detects deactivation and removes disease effect.
		RunReaction(Entity);
		ASSERT_THAT(AreEqual(0, CountActiveEffects(Entity)));
	}

	// ------------------------------------------------------------------
	// DecayToZero_RemovesActivationEffect
	//
	// Activate burning (saturation=25) -> reaction applies burning activation effect (1 effect).
	// Tick 10 times at 1.0s each (decay=3/sec -> 25 - 30 = 0, deactivates).
	// Run reaction -> detects deactivation -> removes effect (0 effects).
	// ------------------------------------------------------------------
	TEST_METHOD(DecayToZero_RemovesActivationEffect)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcEffectDefinition* BurningEffect = CreateTestEffect(ArcConditionTags::GetActiveTag(EArcConditionType::Burning));

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();
		FArcConditionMassEffectEntry& BurningEntry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Burning);
		BurningEntry.ActivationEffect = BurningEffect;

		// Activate burning at saturation=25.
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 25.f;
		}

		RunReaction(Entity);
		ASSERT_THAT(AreEqual(1, CountActiveEffects(Entity)));

		// Tick 10 times at 1.0s each — default burning decay=3/sec -> 25 - 30 = 0 -> deactivates.
		for (int32 i = 0; i < 10; ++i)
		{
			FMassProcessingContext ProcessingContext(*EntityManager, 1.f);
			UE::Mass::Executor::Run(*BurningTickProcessor, ProcessingContext);
		}

		// Verify burning has deactivated after decay.
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			ASSERT_THAT(IsFalse(Frag->States[(int32)EArcConditionType::Burning].bActive));
		}

		// Reaction processor detects deactivation and removes burning effect.
		RunReaction(Entity);
		ASSERT_THAT(AreEqual(0, CountActiveEffects(Entity)));
	}

	// ------------------------------------------------------------------
	// OverloadLifecycle_AppliesAndRemovesEffects
	//
	// Configure burning activation + overload effects.
	// Set burning to saturation=100/active/Overloaded (run reaction -> 2 effects).
	// Tick ~7s — overload timer expires -> transitions to Burnout.
	// Run reaction -> overload effect removed (< 2 effects).
	// ------------------------------------------------------------------
	TEST_METHOD(OverloadLifecycle_AppliesAndRemovesEffects)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcEffectDefinition* BurningActivationEffect = CreateTestEffect(ArcConditionTags::GetActiveTag(EArcConditionType::Burning));
		UArcEffectDefinition* BurningOverloadEffect = CreateTestEffect(ArcConditionTags::GetOverloadTag(EArcConditionType::Burning));

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();
		FArcConditionMassEffectEntry& BurningEntry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Burning);
		BurningEntry.ActivationEffect = BurningActivationEffect;
		BurningEntry.OverloadEffect = BurningOverloadEffect;

		// Set burning to saturation=100, active, Overloaded with a short timer (6s).
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 100.f;
			Frag->States[(int32)EArcConditionType::Burning].OverloadPhase = EArcConditionOverloadPhase::Overloaded;
			Frag->States[(int32)EArcConditionType::Burning].OverloadTimeRemaining = 6.f;
		}

		// Reaction processor sees active + overloaded -> applies both activation and overload effects.
		RunReaction(Entity);
		ASSERT_THAT(AreEqual(2, CountActiveEffects(Entity)));

		// Tick 7 times at 1.0s each — overload timer expires (6s) -> transitions to Burnout.
		for (int32 i = 0; i < 7; ++i)
		{
			FMassProcessingContext ProcessingContext(*EntityManager, 1.f);
			UE::Mass::Executor::Run(*BurningTickProcessor, ProcessingContext);
		}

		// Verify overload phase has left Overloaded.
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			ASSERT_THAT(IsFalse(Frag->States[(int32)EArcConditionType::Burning].OverloadPhase == EArcConditionOverloadPhase::Overloaded));
		}

		// Reaction processor detects overload end -> removes overload effect; activation effect remains.
		RunReaction(Entity);
		ASSERT_THAT(IsTrue(CountActiveEffects(Entity) < 2));
	}
};
