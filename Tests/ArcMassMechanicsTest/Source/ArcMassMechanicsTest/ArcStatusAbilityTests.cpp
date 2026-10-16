// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMechTestSupport.h"
#include "ArcStatusTestHarness.h"
#include "ArcStatusTestTypes.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Statuses/ArcStatusComponents.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectFunctions.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Events/ArcGameEventTags.h"

namespace ArcStatusAbilityTestsPrivate
{
	UArcEffectDefinition* LoadStatus(const TCHAR* Name)
	{
		return ArcMechTest::LoadTestAsset<UArcEffectDefinition>(*FString::Printf(TEXT("Statuses/%s"), Name));
	}

	const FArcEffectComp_InterruptAbilities* FindInterrupt(const UArcEffectDefinition* Definition)
	{
		if (!Definition)
		{
			return nullptr;
		}
		for (const FInstancedStruct& Component : Definition->Components)
		{
			if (const FArcEffectComp_InterruptAbilities* Interrupt = Component.GetPtr<FArcEffectComp_InterruptAbilities>())
			{
				return Interrupt;
			}
		}
		return nullptr;
	}
}

TEST_CLASS(ArcStatus_InterruptAbilities, "ArcMassMechanics.Statuses.InterruptAbilities")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;
	UArcEffectDefinition* Stun = nullptr;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Stun = ArcStatusAbilityTestsPrivate::LoadStatus(TEXT("ED_Status_Stun"));
		ASSERT_THAT(IsNotNull(ArcStatusAbilityTestsPrivate::FindInterrupt(Stun)));
	}

	FArcAbilityHandle GrantRunning(FMassEntityHandle Entity, const FGameplayTag& AbilityTag, const FGameplayTag& InputTag, bool bActivate = true)
	{
		UArcAbilityDefinition* Definition = NewObject<UArcAbilityDefinition>();
		Definition->StateTree = ArcStatusTest::MakeRunForeverTree(Harness.World);
		if (AbilityTag.IsValid())
		{
			Definition->AbilityTags.AddTag(AbilityTag);
		}
		Definition->ActivationBlockedTags.AddTag(ArcMassMechanicsTags::State_Stun);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*Harness.EntityManager, Entity, Definition, InputTag);
		if (bActivate)
		{
			Harness.Activate(Entity, InputTag);
		}
		return Handle;
	}

	bool IsActive(FMassEntityHandle Entity, FArcAbilityHandle Handle) const
	{
		return ArcAbilities::IsAbilityActive(*Harness.EntityManager, Entity, Handle);
	}

	const FArcGameEvent* FindEnded(FArcAbilityHandle Handle) const
	{
		for (const FArcGameEvent& Event : Harness.CapturedEvents)
		{
			const FArcAbilityEndedPayload* Payload = Event.Payload.GetPtr<FArcAbilityEndedPayload>();
			if (Event.Tag == ArcGameEventTags::AbilityEnded.GetTag() && Payload && Payload->Handle == Handle)
			{
				return &Event;
			}
		}
		return nullptr;
	}

	TEST_METHOD(Stun_InterruptsRunningNonPassiveAbilities_WithInterruptedReason_KeepsPassive)
	{
		const FMassEntityHandle Source = Harness.CreateEntity();
		const FMassEntityHandle Target = Harness.CreateEntity();
		const FArcAbilityHandle Melee = GrantRunning(Target, ArcStatusTestTags::AbilityMelee, ArcStatusTestTags::InputA);
		const FArcAbilityHandle Passive = GrantRunning(Target, ArcStatusTestTags::AbilityPassive, ArcStatusTestTags::InputB);
		ASSERT_THAT(IsTrue(IsActive(Target, Melee)));
		ASSERT_THAT(IsTrue(IsActive(Target, Passive)));
		Harness.CaptureEvents(ArcGameEventTags::AbilityEnded.GetTag());

		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Stun, Source);
		Harness.DrainEvents();

		ASSERT_THAT(IsFalse(IsActive(Target, Melee)));
		ASSERT_THAT(IsTrue(IsActive(Target, Passive)));
		const FArcGameEvent* Ended = FindEnded(Melee);
		ASSERT_THAT(IsNotNull(Ended));
		ASSERT_THAT(IsTrue(Ended->Payload.Get<FArcAbilityEndedPayload>().Reason == EArcAbilityEndReason::Interrupted));
		ASSERT_THAT(IsTrue(Ended->Instigator == Source));
		ASSERT_THAT(IsTrue(Ended->Target == Target));
		ASSERT_THAT(IsNull(FindEnded(Passive)));
	}

	TEST_METHOD(Silence_InterruptsRunningSpellsOnly)
	{
		UArcEffectDefinition* Silence = ArcStatusAbilityTestsPrivate::LoadStatus(TEXT("ED_Status_Silence"));
		ASSERT_THAT(IsNotNull(ArcStatusAbilityTestsPrivate::FindInterrupt(Silence)));
		const FMassEntityHandle Target = Harness.CreateEntity();
		const FArcAbilityHandle Spell = GrantRunning(Target, ArcStatusTestTags::AbilitySpell, ArcStatusTestTags::InputA);
		const FArcAbilityHandle Melee = GrantRunning(Target, ArcStatusTestTags::AbilityMelee, ArcStatusTestTags::InputB);
		ASSERT_THAT(IsTrue(IsActive(Target, Spell)));
		ASSERT_THAT(IsTrue(IsActive(Target, Melee)));

		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Silence, FMassEntityHandle());

		ASSERT_THAT(IsFalse(IsActive(Target, Spell)));
		ASSERT_THAT(IsTrue(IsActive(Target, Melee)));
		ASSERT_THAT(AreEqual(1, ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Target, ArcMassMechanicsTags::State_Silence)));
	}

	// Real status lifecycle: the stun grants ArcState.Stun, interrupts the running ability and blocks a new activation until it is removed.
	TEST_METHOD(Stun_GrantsTheStateTag_BlocksActivation_AndRemovalLiftsBoth)
	{
		const FMassEntityHandle Target = Harness.CreateEntity();
		const FArcAbilityHandle Melee = GrantRunning(Target, ArcStatusTestTags::AbilityMelee, ArcStatusTestTags::InputA);
		ASSERT_THAT(IsTrue(IsActive(Target, Melee)));

		const FArcEffectApplyResult Stunned = ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Stun, FMassEntityHandle());
		ASSERT_THAT(IsTrue(Stunned.Handle.IsValid()));
		ASSERT_THAT(AreEqual(1, ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Target, ArcMassMechanicsTags::State_Stun)));
		ASSERT_THAT(IsFalse(IsActive(Target, Melee)));

		Harness.Activate(Target, ArcStatusTestTags::InputA);
		ASSERT_THAT(IsFalse(IsActive(Target, Melee)));

		FArcEffectRemovalQuery Removal;
		Removal.Handle = Stunned.Handle;
		ArcEffects::RemoveEffects(*Harness.EntityManager, Target, Removal);
		ASSERT_THAT(AreEqual(0, ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Target, ArcMassMechanicsTags::State_Stun)));

		Harness.Activate(Target, ArcStatusTestTags::InputA);
		ASSERT_THAT(IsTrue(IsActive(Target, Melee)));
	}

	TEST_METHOD(SelfApplied_SkipsSourceAbility_InterruptsOthers)
	{
		const FMassEntityHandle Caster = Harness.CreateEntity();
		const FArcAbilityHandle Casting = GrantRunning(Caster, ArcStatusTestTags::AbilitySpell, ArcStatusTestTags::InputA);
		const FArcAbilityHandle Other = GrantRunning(Caster, ArcStatusTestTags::AbilityMelee, ArcStatusTestTags::InputB);
		ASSERT_THAT(IsTrue(IsActive(Caster, Casting)));
		ASSERT_THAT(IsTrue(IsActive(Caster, Other)));

		FArcEffectContext Context;
		Context.SourceEntity = Caster;
		Context.SourceAbilityHandle = Casting;
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Caster, Stun, Context);

		ASSERT_THAT(IsTrue(IsActive(Caster, Casting)));
		ASSERT_THAT(IsFalse(IsActive(Caster, Other)));
	}

	TEST_METHOD(SourceAbilityHandleFromAnotherEntity_DoesNotShieldTargetAbility)
	{
		const FMassEntityHandle Source = Harness.CreateEntity();
		const FMassEntityHandle Target = Harness.CreateEntity();
		const FArcAbilityHandle Stunnable = GrantRunning(Target, ArcStatusTestTags::AbilityMelee, ArcStatusTestTags::InputA);

		FArcEffectContext Context;
		Context.SourceEntity = Source;
		Context.SourceAbilityHandle = Stunnable;
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Stun, Context);

		ASSERT_THAT(IsFalse(IsActive(Target, Stunnable)));
	}

	TEST_METHOD(InactiveAbility_IsNotEnded)
	{
		const FMassEntityHandle Target = Harness.CreateEntity();
		const FArcAbilityHandle Idle = GrantRunning(Target, ArcStatusTestTags::AbilityMelee, ArcStatusTestTags::InputA, false);
		ASSERT_THAT(IsFalse(IsActive(Target, Idle)));
		Harness.CaptureEvents(ArcGameEventTags::AbilityEnded.GetTag());

		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Stun, FMassEntityHandle());
		Harness.DrainEvents();

		ASSERT_THAT(IsNull(FindEnded(Idle)));
	}
};
