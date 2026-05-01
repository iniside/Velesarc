// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcConditionEffectsTestHelpers.h"
#include "ArcConditionEffectsSubsystem.h"
#include "ArcConditionFragments.h"
#include "ArcConditionGameplayTags.h"
#include "ArcConditionMassEffectsConfig.h"
#include "ArcConditionReactionProcessor.h"
#include "ArcConditionTypes.h"
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
// Reaction Processor — Mass Effect Application Tests
// ============================================================================

TEST_CLASS(ArcCondition_Reaction_MassEffects, "ArcConditionEffects.Integration.Reaction.MassEffects")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcConditionEffectsSubsystem* Subsystem = nullptr;
	UArcConditionReactionProcessor* ReactionProcessor = nullptr;

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
		ReactionProcessor = NewObject<UArcConditionReactionProcessor>();
		ReactionProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

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

	void SignalAndExecute(FMassEntityHandle Entity, FName SignalName)
	{
		SignalSubsystem->SignalEntity(SignalName, Entity);
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
	// Activation_AppliesEffect
	// ------------------------------------------------------------------
	TEST_METHOD(Activation_AppliesEffect)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcEffectDefinition* BurningEffect = CreateTestEffect(ArcConditionTags::GetActiveTag(EArcConditionType::Burning));

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();
		FArcConditionMassEffectEntry& Entry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Burning);
		Entry.ActivationEffect = BurningEffect;

		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 30.f;
		}

		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

		ASSERT_THAT(AreEqual(1, CountActiveEffects(Entity)));
	}

	// ------------------------------------------------------------------
	// Deactivation_RemovesEffectByTag
	// ------------------------------------------------------------------
	TEST_METHOD(Deactivation_RemovesEffectByTag)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcEffectDefinition* BurningEffect = CreateTestEffect(ArcConditionTags::GetActiveTag(EArcConditionType::Burning));

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();
		FArcConditionMassEffectEntry& Entry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Burning);
		Entry.ActivationEffect = BurningEffect;

		// Run 1: activate — prev=inactive, current=active -> applies effect
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 30.f;
		}
		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);
		ASSERT_THAT(AreEqual(1, CountActiveEffects(Entity)));

		// Run 2: deactivate — prev=active, current=inactive -> removes effects by Active_Burning tag
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = false;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 0.f;
		}
		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

		ASSERT_THAT(AreEqual(0, CountActiveEffects(Entity)));
	}

	// ------------------------------------------------------------------
	// Burst_AppliesEffectAtHundred
	// ------------------------------------------------------------------
	TEST_METHOD(Burst_AppliesEffectAtHundred)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcEffectDefinition* BurstEffect = CreateTestEffect(ArcConditionTags::GetOverloadTag(EArcConditionType::Burning));

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();
		FArcConditionMassEffectEntry& Entry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Burning);
		Entry.BurstEffect = BurstEffect;

		// Set saturation=100 + overloaded; prev saturation was 0 so it crosses the 100 threshold
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 100.f;
			Frag->States[(int32)EArcConditionType::Burning].OverloadPhase = EArcConditionOverloadPhase::Overloaded;
		}

		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

		ASSERT_THAT(AreEqual(1, CountActiveEffects(Entity)));
	}

	// ------------------------------------------------------------------
	// Overload_AppliesEffectOnEnter
	// ------------------------------------------------------------------
	TEST_METHOD(Overload_AppliesEffectOnEnter)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcEffectDefinition* OverloadEffect = CreateTestEffect(ArcConditionTags::GetOverloadTag(EArcConditionType::Burning));

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();
		FArcConditionMassEffectEntry& Entry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Burning);
		Entry.OverloadEffect = OverloadEffect;

		// Transition: None -> Overloaded (prev state is None by default)
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 100.f;
			Frag->States[(int32)EArcConditionType::Burning].OverloadPhase = EArcConditionOverloadPhase::Overloaded;
		}

		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionOverloadChanged);

		ASSERT_THAT(AreEqual(1, CountActiveEffects(Entity)));
	}

	// ------------------------------------------------------------------
	// Overload_RemovesEffectOnEnd
	// ------------------------------------------------------------------
	TEST_METHOD(Overload_RemovesEffectOnEnd)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcEffectDefinition* OverloadEffect = CreateTestEffect(ArcConditionTags::GetOverloadTag(EArcConditionType::Burning));

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();
		FArcConditionMassEffectEntry& Entry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Burning);
		Entry.OverloadEffect = OverloadEffect;

		// Run 1: enter overload (None -> Overloaded)
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 100.f;
			Frag->States[(int32)EArcConditionType::Burning].OverloadPhase = EArcConditionOverloadPhase::Overloaded;
		}
		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionOverloadChanged);
		ASSERT_THAT(AreEqual(1, CountActiveEffects(Entity)));

		// Run 2: overload expires (Overloaded -> Burnout) — processor removes overload effects by tag
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].OverloadPhase = EArcConditionOverloadPhase::Burnout;
		}
		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionOverloadChanged);

		ASSERT_THAT(AreEqual(0, CountActiveEffects(Entity)));
	}

	// ------------------------------------------------------------------
	// NoConfig_NoCrash
	// ------------------------------------------------------------------
	TEST_METHOD(NoConfig_NoCrash)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		// Set MassConfig to nullptr — processor must not crash
		Subsystem->SetMassEffectsConfig(nullptr);

		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 30.f;
		}

		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

		ASSERT_THAT(AreEqual(0, CountActiveEffects(Entity)));
	}

	// ------------------------------------------------------------------
	// NullEffect_NoCrash
	// ------------------------------------------------------------------
	TEST_METHOD(NullEffect_NoCrash)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();
		// Add entry with all effects null
		FArcConditionMassEffectEntry& Entry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Burning);
		Entry.ActivationEffect = nullptr;
		Entry.BurstEffect = nullptr;
		Entry.OverloadEffect = nullptr;

		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 30.f;
		}

		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

		ASSERT_THAT(AreEqual(0, CountActiveEffects(Entity)));
	}

	// ------------------------------------------------------------------
	// MultipleConditions_IndependentEffects
	// ------------------------------------------------------------------
	TEST_METHOD(MultipleConditions_IndependentEffects)
	{
		FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

		UArcEffectDefinition* BurningEffect = CreateTestEffect(ArcConditionTags::GetActiveTag(EArcConditionType::Burning));
		UArcEffectDefinition* BleedingEffect = CreateTestEffect(ArcConditionTags::GetActiveTag(EArcConditionType::Bleeding));

		UArcConditionMassEffectsConfig* MassConfig = Subsystem->GetMassEffectsConfig();

		FArcConditionMassEffectEntry& BurningEntry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Burning);
		BurningEntry.ActivationEffect = BurningEffect;

		FArcConditionMassEffectEntry& BleedingEntry = MassConfig->ConditionEffects.FindOrAdd(EArcConditionType::Bleeding);
		BleedingEntry.ActivationEffect = BleedingEffect;

		// Activate burning + bleeding simultaneously
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = true;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 30.f;

			Frag->States[(int32)EArcConditionType::Bleeding].bActive = true;
			Frag->States[(int32)EArcConditionType::Bleeding].Saturation = 20.f;
		}
		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);
		ASSERT_THAT(AreEqual(2, CountActiveEffects(Entity)));

		// Deactivate only burning — bleeding effect must remain
		{
			FMassEntityView View(*EntityManager, Entity);
			FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
			Frag->States[(int32)EArcConditionType::Burning].bActive = false;
			Frag->States[(int32)EArcConditionType::Burning].Saturation = 0.f;
		}
		SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

		ASSERT_THAT(AreEqual(1, CountActiveEffects(Entity)));
	}
};
