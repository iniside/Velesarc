// Copyright Lukasz Baran. All Rights Reserved.
//
// CQTest coverage for On and While on the one event bus (Plan 7 Step 7): the "On stunned" and "On spell cast"
// authoring rows, the proc guard on ability activation, and While re-evaluating on tag and child-tag changes.
// Reward effects are the real Triggers/ED_Reward_* test assets; the host effects are built in code because they
// exist for one test each. Topology: standalone world, production processors, production bus drain.

#include "CQTest.h"
#include "ArcMechTestSupport.h"
#include "ArcStatusTestHarness.h"
#include "ArcStatusTestTypes.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Triggers/ArcTriggerOn.h"
#include "ArcMassMechanics/Triggers/ArcTriggerTypes.h"
#include "ArcMassMechanics/Triggers/ArcTriggerWhile.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcEffectEventPayloads.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventTags.h"
#include "Mass/EntityManager.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Processors/ArcAbilityStateTreeTickProcessor.h"
#include "Signals/ArcAbilitySignals.h"

namespace ArcTriggerBusTestsPrivate
{
	UArcEffectDefinition* LoadReward(const TCHAR* Name)
	{
		return ArcMechTest::LoadTestAsset<UArcEffectDefinition>(*FString::Printf(TEXT("Triggers/%s"), Name));
	}

	FInstancedStruct MakeApplyBurning(UArcEffectDefinition* Burning)
	{
		FArcTriggerEffect_ApplyEffect Apply;
		Apply.Effect = Burning;
		Apply.Recipient = EArcTriggerParty::Owner;
		Apply.Instigator = EArcTriggerParty::Owner;
		return FInstancedStruct::Make(Apply);
	}

	FInstancedStruct MakeOwnerTagCondition(const FGameplayTag& RequiredTag)
	{
		FArcTriggerCondition_Tags Condition;
		Condition.Party = EArcTriggerParty::Owner;
		Condition.Requirements.RequireTags.AddTag(RequiredTag);
		return FInstancedStruct::Make(Condition);
	}

	UArcEffectDefinition* MakeInfiniteHost(const FInstancedStruct& Task)
	{
		UArcEffectDefinition* Host = NewObject<UArcEffectDefinition>();
		Host->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		Host->Tasks.Add(Task);
		return Host;
	}

	FInstancedStruct MakeOnTask(const FGameplayTag& EventTag, const FGameplayTag& ChangedTag, EArcEventRole Role, const FInstancedStruct& Effect)
	{
		FArcEffectTask_TriggerOn Task;
		Task.EventTag = EventTag;
		Task.Tag = ChangedTag;
		Task.Role = Role;
		Task.Chance = 1.f;
		Task.Effects.Add(Effect);
		return FInstancedStruct::Make(Task);
	}

	int32 CountAppliedOf(const TArray<FArcGameEvent>& Events, const UArcEffectDefinition* Definition)
	{
		int32 Count = 0;
		for (const FArcGameEvent& Event : Events)
		{
			const FArcEffectAppliedPayload* Payload = Event.Payload.GetPtr<FArcEffectAppliedPayload>();
			const FArcEffectSpec* Spec = Payload ? Payload->Spec.GetPtr<FArcEffectSpec>() : nullptr;
			if (Event.Tag == ArcGameEventTags::EffectApplied.GetTag() && Spec && Spec->Definition == Definition)
			{
				++Count;
			}
		}
		return Count;
	}
}

TEST_CLASS(ArcTriggerBus_On, "ArcMassMechanics.Triggers.OnBus")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;
	FMassEntityHandle Owner;
	FMassEntityHandle Other;
	UArcEffectDefinition* BurningReward = nullptr;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Owner = Harness.CreateEntity();
		Other = Harness.CreateEntity();
		BurningReward = ArcTriggerBusTestsPrivate::LoadReward(TEXT("ED_Reward_Burning"));
		ASSERT_THAT(IsNotNull(BurningReward));
		Harness.CaptureEvents(ArcGameEventTags::EffectApplied.GetTag());
	}

	void Pump(int32 Frames = 3)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Harness.DrainPendingOps(Owner);
			Harness.DrainEvents();
		}
	}

	int32 Burning() const
	{
		return ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Owner, ArcMassMechanicsTags::Condition_Burning);
	}

	void AddOwnedTag(const FGameplayTag& Tag)
	{
		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Owner, FGameplayTagContainer(Tag));
	}

	FArcAbilityHandle GrantRunning(const FGameplayTag& AbilityTag, const FGameplayTag& InputTag)
	{
		UArcAbilityDefinition* Definition = NewObject<UArcAbilityDefinition>();
		Definition->StateTree = ArcStatusTest::MakeRunForeverTree(Harness.World);
		Definition->AbilityTags.AddTag(AbilityTag);
		return ArcAbilities::GrantAbility(*Harness.EntityManager, Owner, Definition, InputTag);
	}

	void RunAbilityProcessors()
	{
		UArcAbilityInputProcessor* InputProcessor = NewObject<UArcAbilityInputProcessor>();
		InputProcessor->CallInitialize(Harness.World, Harness.EntityManager->AsShared());
		Harness.SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Owner);
		UE::Mass::FProcessingContext InputContext(*Harness.EntityManager, 0.016f);
		InputProcessor->CallExecute(*Harness.EntityManager, InputContext.GetExecutionContext());

		UArcAbilityStateTreeTickProcessor* TickProcessor = NewObject<UArcAbilityStateTreeTickProcessor>();
		TickProcessor->CallInitialize(Harness.World, Harness.EntityManager->AsShared());
		Harness.SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Owner);
		UE::Mass::FProcessingContext TickContext(*Harness.EntityManager, 0.016f);
		TickProcessor->CallExecute(*Harness.EntityManager, TickContext.GetExecutionContext());
	}

	TEST_METHOD(OnStunned_TagAddedWithTagStun_FiresOnlyWhenStunIsAdded)
	{
		using namespace ArcTriggerBusTestsPrivate;
		UArcEffectDefinition* Host = MakeInfiniteHost(MakeOnTask(ArcGameEventTags::TagAdded.GetTag(), ArcMassMechanicsTags::State_Stun.GetTag(),
			EArcEventRole::Target, MakeApplyBurning(BurningReward)));
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, Host, Owner);
		Pump();

		AddOwnedTag(ArcMassMechanicsTags::State_Silence);
		AddOwnedTag(ArcStatusTestTags::Marker);
		Pump();
		ASSERT_THAT(AreEqual(0, Burning(), TEXT("other tags never reach the trigger")));

		AddOwnedTag(ArcMassMechanicsTags::State_Stun_Frozen);
		Pump();
		ASSERT_THAT(AreEqual(0, Burning(), TEXT("a child tag's edge is not the Stun edge")));

		AddOwnedTag(ArcMassMechanicsTags::State_Stun);
		Pump();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(1, CountAppliedOf(Harness.CapturedEvents, BurningReward)));
	}

	TEST_METHOD(OnSpellCast_ActivatedWithOwnerRequiringTheSpellTag_FiresOncePerSpellActivationOnly)
	{
		using namespace ArcTriggerBusTestsPrivate;
		FArcEffectTask_TriggerOn Task;
		Task.EventTag = ArcGameEventTags::AbilityActivated.GetTag();
		Task.Role = EArcEventRole::Any;
		Task.Chance = 1.f;
		Task.Conditions.Add(MakeOwnerTagCondition(ArcStatusTestTags::AbilitySpell));
		Task.Effects.Add(MakeApplyBurning(BurningReward));
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, MakeInfiniteHost(FInstancedStruct::Make(Task)), Owner);
		Pump();

		GrantRunning(ArcStatusTestTags::AbilityMelee, ArcStatusTestTags::InputB);
		GrantRunning(ArcStatusTestTags::AbilitySpell, ArcStatusTestTags::InputA);

		Harness.Activate(Owner, ArcStatusTestTags::InputB);
		Pump();
		ASSERT_THAT(AreEqual(0, Burning(), TEXT("a melee activation does not carry the spell tag")));
		ASSERT_THAT(AreEqual(0, CountAppliedOf(Harness.CapturedEvents, BurningReward)));

		Harness.Activate(Owner, ArcStatusTestTags::InputA);
		Pump();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(1, CountAppliedOf(Harness.CapturedEvents, BurningReward)));
	}

	TEST_METHOD(ProcGuard_AbilityCastByATrigger_DoesNotFireAnOnOnActivated)
	{
		using namespace ArcTriggerBusTestsPrivate;
		UArcEffectDefinition* CastHost = LoadReward(TEXT("ED_Host_CastAbility"));
		ASSERT_THAT(IsNotNull(CastHost));
		UArcEffectDefinition* SparkEffect = LoadReward(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(SparkEffect));
		const FArcAbilityHandle TriggerCast = GrantRunning(ArcStatusTestTags::AbilitySpell, ArcStatusTestTags::InputA);
		GrantRunning(ArcStatusTestTags::AbilityMelee, ArcStatusTestTags::InputB);

		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, CastHost, Owner);
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner,
			MakeInfiniteHost(MakeOnTask(ArcGameEventTags::AbilityActivated.GetTag(), FGameplayTag(), EArcEventRole::Any, MakeApplyBurning(BurningReward))), Owner);
		Pump();

		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, SparkEffect, Other);
		Pump();
		RunAbilityProcessors();
		ASSERT_THAT(IsTrue(ArcAbilities::IsAbilityActive(*Harness.EntityManager, Owner, TriggerCast)));
		Pump();
		ASSERT_THAT(AreEqual(0, Burning(), TEXT("the activation carries ArcMech.Trigger.Origin, so the On ignores it")));

		Harness.Activate(Owner, ArcStatusTestTags::InputB);
		Pump();
		ASSERT_THAT(AreEqual(1, Burning(), TEXT("a player activation still fires it")));
	}
};

TEST_CLASS(ArcTriggerBus_While, "ArcMassMechanics.Triggers.WhileBus")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;
	FMassEntityHandle Owner;
	UArcEffectDefinition* BurningReward = nullptr;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Owner = Harness.CreateEntity();
		BurningReward = ArcTriggerBusTestsPrivate::LoadReward(TEXT("ED_Reward_Burning"));
		ASSERT_THAT(IsNotNull(BurningReward));
	}

	// A While evaluates in a deferred command: the change is drained in one frame and the evaluation runs in the next.
	void Pump(int32 Frames = 3)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Harness.DrainPendingOps(Owner);
			Harness.DrainEvents();
		}
	}

	int32 Burning() const
	{
		return ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Owner, ArcMassMechanicsTags::Condition_Burning);
	}

	void AddOwnedTag(const FGameplayTag& Tag)
	{
		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Owner, FGameplayTagContainer(Tag));
	}

	void RemoveOwnedTag(const FGameplayTag& Tag)
	{
		ArcAbilities::RemoveOwnedTags(*Harness.EntityManager, Owner, FGameplayTagContainer(Tag));
	}

	void ApplyStunWhileHost()
	{
		using namespace ArcTriggerBusTestsPrivate;
		FArcEffectTask_TriggerWhile Task;
		Task.Conditions.Add(MakeOwnerTagCondition(ArcMassMechanicsTags::State_Stun.GetTag()));
		Task.Effects.Add(MakeApplyBurning(BurningReward));
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, MakeInfiniteHost(FInstancedStruct::Make(Task)), Owner);
		Pump();
	}

	TEST_METHOD(StunRequirement_ReevaluatesWhenAChildTagIsAddedAndRemoved)
	{
		ApplyStunWhileHost();
		ASSERT_THAT(AreEqual(0, Burning()));

		AddOwnedTag(ArcMassMechanicsTags::State_Silence);
		Pump();
		ASSERT_THAT(AreEqual(0, Burning(), TEXT("an unrelated tag leaves the condition false")));

		AddOwnedTag(ArcMassMechanicsTags::State_Stun_Frozen);
		Pump();
		ASSERT_THAT(AreEqual(1, Burning(), TEXT("ArcState.Stun.Frozen satisfies a Stun requirement and re-evaluates it")));

		RemoveOwnedTag(ArcMassMechanicsTags::State_Stun_Frozen);
		Pump();
		ASSERT_THAT(AreEqual(0, Burning()));
	}

	TEST_METHOD(StunRequirement_ReevaluatesWhenTheNamedTagItselfChanges)
	{
		ApplyStunWhileHost();

		AddOwnedTag(ArcMassMechanicsTags::State_Stun);
		Pump();
		ASSERT_THAT(AreEqual(1, Burning()));

		RemoveOwnedTag(ArcMassMechanicsTags::State_Stun);
		Pump();
		ASSERT_THAT(AreEqual(0, Burning()));
	}
};
