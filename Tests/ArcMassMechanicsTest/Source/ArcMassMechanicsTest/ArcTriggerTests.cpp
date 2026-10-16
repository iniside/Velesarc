// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassMechanicsTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcStatusTestHarness.h"
#include "ArcStatusTestTypes.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Triggers/ArcTriggerIf.h"
#include "ArcMassMechanics/Triggers/ArcTriggerOn.h"
#include "ArcMassMechanics/Triggers/ArcTriggerTypes.h"
#include "ArcMassMechanics/Triggers/ArcTriggerWhile.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "AssetRegistry/AssetData.h"
#include "Attributes/ArcInstantModifier.h"
#include "Components/ActorTestSpawner.h"
#include "EditorValidatorBase.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Events/ArcEffectEventPayloads.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "Misc/DataValidation.h"
#include "NativeGameplayTags.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Processors/ArcAbilityStateTreeTickProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "UObject/Package.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TriggerTest_Watch, "Test.MassMechanics.Trigger.Watch");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TriggerTest_Unwatched, "Test.MassMechanics.Trigger.Unwatched");

namespace ArcTriggerTestsPrivate
{
	FGameplayTagContainer Tags(std::initializer_list<FGameplayTag> InTags)
	{
		FGameplayTagContainer Container;
		for (const FGameplayTag& Tag : InTags)
		{
			Container.AddTag(Tag);
		}
		return Container;
	}

	UArcEffectDefinition* LoadTrigger(const TCHAR* Name)
	{
		return ArcMechTest::LoadTestAsset<UArcEffectDefinition>(*FString::Printf(TEXT("Triggers/%s"), Name));
	}

	FArcEffectTask_TriggerOn MakeOn(EArcEventRole Role, const FGameplayTag& EventTag, float Chance = 1.f)
	{
		FArcEffectTask_TriggerOn Task;
		Task.Role = Role;
		Task.EventTag = EventTag;
		Task.Chance = Chance;
		return Task;
	}

	FArcIfModifier MakeModifier(EArcIfChannel Channel, float Value)
	{
		FArcIfModifier Modifier;
		Modifier.Channel = Channel;
		Modifier.Value = Value;
		return Modifier;
	}

	FArcEffectTask_TriggerIf MakeIf(EArcEventRole Role, std::initializer_list<FArcIfModifier> Modifiers)
	{
		FArcEffectTask_TriggerIf Task;
		Task.Role = Role;
		Task.Modifiers = Modifiers;
		return Task;
	}

	FInstancedStruct MakeApply(UArcEffectDefinition* Effect)
	{
		FArcTriggerEffect_ApplyEffect Apply;
		Apply.Effect = Effect;
		Apply.Recipient = EArcTriggerParty::Owner;
		Apply.Instigator = EArcTriggerParty::Owner;
		return FInstancedStruct::Make(Apply);
	}

	UArcEffectDefinition* MakeHost(const FInstancedStruct& Task)
	{
		UArcEffectDefinition* Host = NewObject<UArcEffectDefinition>();
		Host->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		Host->Tasks.Add(Task);
		return Host;
	}

	int32 GetStackCount(const ArcStatusTest::FStatusHarness& Harness, FMassEntityHandle Entity, const UArcEffectDefinition* Definition)
	{
		const FArcActiveEffect* Active = Harness.FindActiveByDefinition(Entity, Definition);
		const FArcEffectSpec* Spec = Active ? Active->Spec.GetPtr<FArcEffectSpec>() : nullptr;
		return Spec ? Spec->Context.StackCount : 0;
	}

	void RemoveByHandle(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcActiveEffectHandle Handle)
	{
		FArcEffectRemovalQuery Query;
		Query.Handle = Handle;
		Query.Reason = EArcEffectRemovalReason::Removed;
		ArcEffects::RemoveEffects(EntityManager, Entity, Query);
	}

	template <typename TPayload>
	int32 CountEventsFor(const TArray<FArcGameEvent>& Events, const FGameplayTag& Tag, const UArcEffectDefinition* Definition)
	{
		int32 Count = 0;
		for (const FArcGameEvent& Event : Events)
		{
			const TPayload* Payload = Event.Payload.GetPtr<TPayload>();
			const FArcEffectSpec* Spec = Payload ? Payload->Spec.template GetPtr<FArcEffectSpec>() : nullptr;
			if (Event.Tag == Tag && Spec && Spec->Definition == Definition)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 Burning(ArcStatusTest::FStatusHarness& Harness, FMassEntityHandle Entity)
	{
		return ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Entity, ArcMassMechanicsTags::Condition_Burning);
	}

	int32 Bleeding(ArcStatusTest::FStatusHarness& Harness, FMassEntityHandle Entity)
	{
		return ArcMechTest::GetOwnedTagCount(*Harness.EntityManager, Entity, ArcMassMechanicsTags::Condition_Bleeding);
	}

	// Rewards are real statuses (one instance, a new application replaces the old one), so a test counting firings removes them between fires.
	bool ClearRewards(ArcStatusTest::FStatusHarness& Harness, FMassEntityHandle Entity)
	{
		UArcEffectDefinition* BurningReward = LoadTrigger(TEXT("ED_Reward_Burning"));
		UArcEffectDefinition* BleedingReward = LoadTrigger(TEXT("ED_Reward_Bleeding"));
		if (!BurningReward || !BleedingReward)
		{
			return false;
		}
		for (const UArcEffectDefinition* Reward : {BurningReward, BleedingReward})
		{
			FArcEffectRemovalQuery Query;
			Query.Definition = Reward;
			Query.Reason = EArcEffectRemovalReason::Removed;
			ArcEffects::RemoveEffects(*Harness.EntityManager, Entity, Query);
		}
		Harness.DrainPendingOps(Entity);
		Harness.DrainEvents();
		return Burning(Harness, Entity) == 0 && Bleeding(Harness, Entity) == 0;
	}
}

TEST_CLASS(ArcTrigger_On, "ArcMassMechanics.Triggers.On")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;
	FMassEntityHandle Owner;
	FMassEntityHandle Other;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Owner = Harness.CreateEntity();
		Other = Harness.CreateEntity();
	}

	void Pump(int32 Frames = 1)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Harness.DrainPendingOps(Owner);
			Harness.DrainEvents();
		}
	}

	FArcActiveEffectHandle ApplyHost(UArcEffectDefinition* Host, FMassEntityHandle Entity)
	{
		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Harness.EntityManager, Entity, Host, Entity);
		Pump();
		return Result.Handle;
	}

	void Spark(UArcEffectDefinition* Effect, FMassEntityHandle Target, FMassEntityHandle Source)
	{
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Effect, Source);
		Pump();
	}

	int32 Burning(FMassEntityHandle Entity)
	{
		return ArcTriggerTestsPrivate::Burning(Harness, Entity);
	}

	TEST_METHOD(RoleInstigator_FiresOnlyWhenOwnerIsTheEventInstigator)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* SparkEffect = LoadTrigger(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(SparkEffect));
		UArcEffectDefinition* Host = LoadTrigger(TEXT("ED_Host_On_Instigator"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host, Owner);

		Spark(SparkEffect, Owner, Other);
		ASSERT_THAT(AreEqual(0, Burning(Owner)));

		Spark(SparkEffect, Other, Owner);
		ASSERT_THAT(AreEqual(1, Burning(Owner)));
	}

	TEST_METHOD(RoleTarget_FiresOnlyWhenOwnerIsTheEventTarget)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* SparkEffect = LoadTrigger(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(SparkEffect));
		UArcEffectDefinition* Host = LoadTrigger(TEXT("ED_Host_On_Chance1"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host, Owner);

		Spark(SparkEffect, Other, Owner);
		ASSERT_THAT(AreEqual(0, Burning(Owner)));

		Spark(SparkEffect, Owner, Other);
		ASSERT_THAT(AreEqual(1, Burning(Owner)));
	}

	TEST_METHOD(RoleAny_FiresForBothSides)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* SparkEffect = LoadTrigger(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(SparkEffect));
		UArcEffectDefinition* Host = LoadTrigger(TEXT("ED_Host_On_Any"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host, Owner);

		Spark(SparkEffect, Other, Owner);
		ASSERT_THAT(AreEqual(1, Burning(Owner)));
		ASSERT_THAT(IsTrue(ClearRewards(Harness, Owner)));

		Spark(SparkEffect, Owner, Other);
		ASSERT_THAT(AreEqual(1, Burning(Owner)));
	}

	TEST_METHOD(EventTag_OtherTagsDoNotFire)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* Host = LoadTrigger(TEXT("ED_Host_On_EventMagnitude"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host, Owner);

		// Magnitude 15 passes the host's EventMagnitude >= 10 condition, so only the tag can block it.
		FArcGameEvent Unwatched;
		Unwatched.Tag = TAG_TriggerTest_Unwatched;
		Unwatched.Instigator = Other;
		Unwatched.Target = Owner;
		Unwatched.Magnitude = 15.f;
		Harness.EventBus->Queue(MoveTemp(Unwatched));
		Pump();
		ASSERT_THAT(AreEqual(0, Burning(Owner)));

		FArcGameEvent Watched;
		Watched.Tag = TAG_TriggerTest_Watch;
		Watched.Instigator = Other;
		Watched.Target = Owner;
		Watched.Magnitude = 15.f;
		Harness.EventBus->Queue(MoveTemp(Watched));
		Pump();
		ASSERT_THAT(AreEqual(1, Burning(Owner)));
	}

	TEST_METHOD(TagsCondition_ReadsTheOtherPartysOwnedTags)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* SparkEffect = LoadTrigger(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(SparkEffect));
		UArcEffectDefinition* Host = LoadTrigger(TEXT("ED_Host_On_OtherTags"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host, Owner);

		Spark(SparkEffect, Owner, Other);
		ASSERT_THAT(AreEqual(0, Burning(Owner)));

		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Other, Tags({ArcStatusTestTags::Marker}));
		Spark(SparkEffect, Owner, Other);
		ASSERT_THAT(AreEqual(1, Burning(Owner)));
	}

	TEST_METHOD(EventMagnitudeCondition_ComparesTheEventMagnitude)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* Host = LoadTrigger(TEXT("ED_Host_On_EventMagnitude"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host, Owner);

		FArcGameEvent Weak;
		Weak.Tag = TAG_TriggerTest_Watch;
		Weak.Instigator = Other;
		Weak.Target = Owner;
		Weak.Magnitude = 5.f;
		Harness.EventBus->Queue(MoveTemp(Weak));
		Pump();
		ASSERT_THAT(AreEqual(0, Burning(Owner)));

		FArcGameEvent Strong;
		Strong.Tag = TAG_TriggerTest_Watch;
		Strong.Instigator = Other;
		Strong.Target = Owner;
		Strong.Magnitude = 15.f;
		Harness.EventBus->Queue(MoveTemp(Strong));
		Pump();
		ASSERT_THAT(AreEqual(1, Burning(Owner)));
	}

	TEST_METHOD(EffectTagsCondition_ReadsTheEventEffectDefinitionTags)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* Host = LoadTrigger(TEXT("ED_Host_On_EffectTags"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host, Owner);

		UArcEffectDefinition* PlainSpark = LoadTrigger(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(PlainSpark));
		Spark(PlainSpark, Owner, Other);
		ASSERT_THAT(AreEqual(0, Burning(Owner)));

		UArcEffectDefinition* SparkMarker = LoadTrigger(TEXT("ED_Spark_Marker"));
		ASSERT_THAT(IsNotNull(SparkMarker));
		Spark(SparkMarker, Owner, Other);
		ASSERT_THAT(AreEqual(1, Burning(Owner)));
	}

	TEST_METHOD(AbilityCondition_MatchesTheEndedAbilityDefinition)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcAbilityDefinition* Matching = NewObject<UArcAbilityDefinition>();
		Matching->StateTree = ArcStatusTest::MakeRunForeverTree(Harness.World);
		UArcAbilityDefinition* Unrelated = NewObject<UArcAbilityDefinition>();
		Unrelated->StateTree = ArcStatusTest::MakeRunForeverTree(Harness.World);

		const FArcAbilityHandle MatchingHandle = ArcAbilities::GrantAbility(*Harness.EntityManager, Owner, Matching, ArcStatusTestTags::InputA);
		const FArcAbilityHandle UnrelatedHandle = ArcAbilities::GrantAbility(*Harness.EntityManager, Owner, Unrelated, ArcStatusTestTags::InputB);
		Harness.Activate(Owner, ArcStatusTestTags::InputA);
		Harness.Activate(Owner, ArcStatusTestTags::InputB);
		ASSERT_THAT(IsTrue(ArcAbilities::IsAbilityActive(*Harness.EntityManager, Owner, MatchingHandle)));
		ASSERT_THAT(IsTrue(ArcAbilities::IsAbilityActive(*Harness.EntityManager, Owner, UnrelatedHandle)));

		UArcEffectDefinition* Reward = LoadTrigger(TEXT("ED_Reward_Burning"));
		ASSERT_THAT(IsNotNull(Reward));
		FArcEffectTask_TriggerOn Trigger = MakeOn(EArcEventRole::Target, ArcGameEventTags::AbilityEnded);
		FArcTriggerCondition_Ability AbilityCondition;
		AbilityCondition.Ability = Matching;
		Trigger.Conditions.Add(FInstancedStruct::Make(AbilityCondition));
		Trigger.Effects.Add(MakeApply(Reward));
		ApplyHost(MakeHost(FInstancedStruct::Make(Trigger)), Owner);

		ArcAbilities::CancelAbility(*Harness.EntityManager, Owner, UnrelatedHandle, *Harness.AbilitySubsystem, EArcAbilityEndReason::Cancelled, Other);
		Pump();
		ASSERT_THAT(AreEqual(0, Burning(Owner)));

		ArcAbilities::CancelAbility(*Harness.EntityManager, Owner, MatchingHandle, *Harness.AbilitySubsystem, EArcAbilityEndReason::Cancelled, Other);
		Pump();
		ASSERT_THAT(AreEqual(1, Burning(Owner)));
	}

	// Chance 0 fails the validator, so both hosts stay code-built (decision 7); they differ only in Chance and both see the same event.
	TEST_METHOD(ChanceZero_NeverFires)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* Reward = LoadTrigger(TEXT("ED_Reward_Burning"));
		ASSERT_THAT(IsNotNull(Reward));
		UArcEffectDefinition* SparkEffect = LoadTrigger(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(SparkEffect));

		FArcEffectTask_TriggerOn Never = MakeOn(EArcEventRole::Any, ArcGameEventTags::EffectExecuted, 0.f);
		Never.Effects.Add(MakeApply(Reward));
		ApplyHost(MakeHost(FInstancedStruct::Make(Never)), Owner);

		FArcEffectTask_TriggerOn Always = MakeOn(EArcEventRole::Any, ArcGameEventTags::EffectExecuted, 1.f);
		Always.Effects.Add(MakeApply(Reward));
		ApplyHost(MakeHost(FInstancedStruct::Make(Always)), Other);

		for (int32 Index = 0; Index < 5; ++Index)
		{
			Spark(SparkEffect, Owner, Other);
			ASSERT_THAT(AreEqual(0, Burning(Owner)));
			ASSERT_THAT(AreEqual(1, Burning(Other)));
			ASSERT_THAT(IsTrue(ClearRewards(Harness, Other)));
		}
	}

	TEST_METHOD(ChanceOne_AlwaysFires)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* SparkEffect = LoadTrigger(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(SparkEffect));
		UArcEffectDefinition* Host = LoadTrigger(TEXT("ED_Host_On_Chance1"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host, Owner);

		for (int32 Index = 0; Index < 5; ++Index)
		{
			Spark(SparkEffect, Owner, Other);
			ASSERT_THAT(AreEqual(1, Burning(Owner)));
			ASSERT_THAT(IsTrue(ClearRewards(Harness, Owner)));
		}
	}
};

TEST_CLASS(ArcTrigger_Charges, "ArcMassMechanics.Triggers.Charges")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;
	FMassEntityHandle Owner;
	FMassEntityHandle Other;
	UArcEffectDefinition* SparkEffect = nullptr;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Owner = Harness.CreateEntity();
		Other = Harness.CreateEntity();
		SparkEffect = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(SparkEffect));
	}

	void Pump()
	{
		Harness.DrainPendingOps(Owner);
		Harness.DrainEvents();
	}

	FArcEffectApplyResult ApplyHost(UArcEffectDefinition* Host)
	{
		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, Host, Owner);
		Pump();
		return Result;
	}

	void Spark()
	{
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, SparkEffect, Other);
		Pump();
	}

	int32 Burning()
	{
		return ArcTriggerTestsPrivate::Burning(Harness, Owner);
	}

	int32 Bleeding()
	{
		return ArcTriggerTestsPrivate::Bleeding(Harness, Owner);
	}

	int32 Count(const UArcEffectDefinition* Definition) const
	{
		return ArcMechTest::CountActiveEffects(*Harness.EntityManager, Owner, Definition);
	}

	bool Clear()
	{
		return ArcTriggerTestsPrivate::ClearRewards(Harness, Owner);
	}

	TEST_METHOD(Unlimited_FiresEveryTime)
	{
		UArcEffectDefinition* Host = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_Host_On_Chance1"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host);

		for (int32 Index = 0; Index < 4; ++Index)
		{
			Spark();
			ASSERT_THAT(AreEqual(1, Burning()));
			ASSERT_THAT(IsTrue(Clear()));
		}
		ASSERT_THAT(AreEqual(1, Count(Host)));
	}

	TEST_METHOD(TriggerCharges_StopAfterN_AndRemoveTheHostWhenSpent)
	{
		UArcEffectDefinition* Host = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_Host_On_Charges"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host);

		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(1, Count(Host)));
		ASSERT_THAT(IsTrue(Clear()));

		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(0, Count(Host)));
		ASSERT_THAT(IsTrue(Clear()));

		Spark();
		ASSERT_THAT(AreEqual(0, Burning()));
	}

	TEST_METHOD(TriggerCharges_KeepTheHostWhenRemovalDisabled)
	{
		UArcEffectDefinition* Host = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_Host_On_Charges_Keep"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host);

		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(IsTrue(Clear()));
		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(IsTrue(Clear()));
		Spark();
		ASSERT_THAT(AreEqual(0, Burning()));
		ASSERT_THAT(AreEqual(1, Count(Host)));
	}

	TEST_METHOD(TriggerCharges_OnlyTheTaskWithRemovalOnEndsTheHost)
	{
		UArcEffectDefinition* Host = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_Host_On_Charges_Mixed"));
		ASSERT_THAT(IsNotNull(Host));
		ApplyHost(Host);

		Spark();
		ASSERT_THAT(AreEqual(1, Bleeding()));
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(1, Count(Host)));
		ASSERT_THAT(IsTrue(Clear()));

		Spark();
		ASSERT_THAT(AreEqual(0, Bleeding()));
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(1, Count(Host)));
		ASSERT_THAT(IsTrue(Clear()));

		Spark();
		ASSERT_THAT(AreEqual(0, Bleeding()));
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(0, Count(Host)));
		ASSERT_THAT(IsTrue(Clear()));

		Spark();
		ASSERT_THAT(AreEqual(0, Bleeding()));
		ASSERT_THAT(AreEqual(0, Burning()));
	}

	TEST_METHOD(HostStack_ConsumesOneStackPerFiring_AndRemovesTheHostAtZero)
	{
		UArcEffectDefinition* Host = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_Host_On_HostStack"));
		ASSERT_THAT(IsNotNull(Host));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, Host, Owner);
		}
		Pump();
		ASSERT_THAT(AreEqual(3, ArcTriggerTestsPrivate::GetStackCount(Harness, Owner, Host)));

		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(2, ArcTriggerTestsPrivate::GetStackCount(Harness, Owner, Host)));
		ASSERT_THAT(IsTrue(Clear()));

		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(1, ArcTriggerTestsPrivate::GetStackCount(Harness, Owner, Host)));
		ASSERT_THAT(IsTrue(Clear()));

		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(0, Count(Host)));
		ASSERT_THAT(IsTrue(Clear()));

		Spark();
		ASSERT_THAT(AreEqual(0, Burning()));
	}

	TEST_METHOD(RemovedHost_Unsubscribes_AndReapplyStartsWithFreshCharges)
	{
		UArcEffectDefinition* Host = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_Host_On_Charges"));
		ASSERT_THAT(IsNotNull(Host));
		const FArcEffectApplyResult First = ApplyHost(Host);

		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(IsTrue(Clear()));

		ArcTriggerTestsPrivate::RemoveByHandle(*Harness.EntityManager, Owner, First.Handle);
		Pump();
		ASSERT_THAT(AreEqual(0, Count(Host)));

		Spark();
		Spark();
		ASSERT_THAT(AreEqual(0, Burning()));

		ApplyHost(Host);

		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(IsTrue(Clear()));
		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(IsTrue(Clear()));
		Spark();
		ASSERT_THAT(AreEqual(0, Burning()));
	}

	TEST_METHOD(RemoveWithHost_RemovesOnlyFlaggedOnEffects_WhenTheHostEnds)
	{
		UArcEffectDefinition* Host = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_Host_On_RemoveWithHost"));
		ASSERT_THAT(IsNotNull(Host));
		const FArcEffectApplyResult Applied = ApplyHost(Host);

		Spark();
		ASSERT_THAT(AreEqual(1, Burning()));
		ASSERT_THAT(AreEqual(1, Bleeding()));

		ArcTriggerTestsPrivate::RemoveByHandle(*Harness.EntityManager, Owner, Applied.Handle);
		Pump();
		ASSERT_THAT(AreEqual(0, Burning()));
		ASSERT_THAT(AreEqual(1, Bleeding()));
	}
};

TEST_CLASS(ArcTrigger_While, "ArcMassMechanics.Triggers.While")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;
	FMassEntityHandle Owner;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Owner = Harness.CreateEntity();
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

	FArcEffectApplyResult ApplyHost(const TCHAR* Name)
	{
		UArcEffectDefinition* Host = ArcTriggerTestsPrivate::LoadTrigger(Name);
		if (!Host)
		{
			return FArcEffectApplyResult();
		}
		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, Host, Owner);
		Pump();
		return Result;
	}

	int32 Burning()
	{
		return ArcTriggerTestsPrivate::Burning(Harness, Owner);
	}

	void SetMana(float Value)
	{
		ArcAttributes::EnqueueInstantModifier(*Harness.EntityManager, Owner, FArcMechTest_CostSet::GetManaAttribute(),
			EArcModifierOp::Override, Value, EArcAttributeOpType::Execute);
		Pump();
	}

	TEST_METHOD(TagCondition_AppliesWhenGranted_AndRemovesWhenRevoked)
	{
		ASSERT_THAT(IsTrue(ApplyHost(TEXT("ED_Host_While")).Handle.IsValid()));
		ASSERT_THAT(AreEqual(0, Burning()));

		UArcEffectDefinition* Grant = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_GrantTag_Marker"));
		ASSERT_THAT(IsNotNull(Grant));
		const FArcEffectApplyResult Granted = ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, Grant, Owner);
		Pump();
		ASSERT_THAT(AreEqual(1, Burning()));

		Pump();
		ASSERT_THAT(AreEqual(1, Burning()));

		ArcTriggerTestsPrivate::RemoveByHandle(*Harness.EntityManager, Owner, Granted.Handle);
		Pump();
		ASSERT_THAT(AreEqual(0, Burning()));

		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, Grant, Owner);
		Pump();
		ASSERT_THAT(AreEqual(1, Burning()));
	}

	TEST_METHOD(TagCondition_DirectOwnedTagChange_SwitchesTheEffect)
	{
		ASSERT_THAT(IsTrue(ApplyHost(TEXT("ED_Host_While")).Handle.IsValid()));
		ASSERT_THAT(AreEqual(0, Burning()));

		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Owner, ArcTriggerTestsPrivate::Tags({ArcStatusTestTags::Marker}));
		Pump();
		ASSERT_THAT(AreEqual(1, Burning()));

		ArcAbilities::RemoveOwnedTags(*Harness.EntityManager, Owner, ArcTriggerTestsPrivate::Tags({ArcStatusTestTags::Marker}));
		Pump();
		ASSERT_THAT(AreEqual(0, Burning()));
	}

	TEST_METHOD(TagCondition_AlreadyTrue_AppliesOnHostApplication)
	{
		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Owner, ArcTriggerTestsPrivate::Tags({ArcStatusTestTags::Marker}));
		ASSERT_THAT(IsTrue(ApplyHost(TEXT("ED_Host_While")).Handle.IsValid()));
		ASSERT_THAT(AreEqual(1, Burning()));
	}

	TEST_METHOD(HostRemoval_RemovesTheEffectsTheWhileApplied)
	{
		ArcAbilities::AddOwnedTags(*Harness.EntityManager, Owner, ArcTriggerTestsPrivate::Tags({ArcStatusTestTags::Marker}));
		const FArcEffectApplyResult Host = ApplyHost(TEXT("ED_Host_While"));
		ASSERT_THAT(IsTrue(Host.Handle.IsValid()));
		ASSERT_THAT(AreEqual(1, Burning()));

		ArcTriggerTestsPrivate::RemoveByHandle(*Harness.EntityManager, Owner, Host.Handle);
		Pump();
		ASSERT_THAT(AreEqual(0, Burning()));
	}

	TEST_METHOD(ThresholdCondition_SwitchesOnBelow_AndOffAfterAHeal)
	{
		ASSERT_THAT(IsTrue(ApplyHost(TEXT("ED_Host_While_AttributeThreshold")).Handle.IsValid()));
		ASSERT_THAT(IsNear(100.f, Harness.GetMana(Owner), 0.001f));
		ASSERT_THAT(AreEqual(0, Burning()));

		SetMana(40.f);
		ASSERT_THAT(IsNear(40.f, Harness.GetMana(Owner), 0.001f));
		ASSERT_THAT(AreEqual(1, Burning()));

		SetMana(80.f);
		ASSERT_THAT(IsNear(80.f, Harness.GetMana(Owner), 0.001f));
		ASSERT_THAT(AreEqual(0, Burning()));

		SetMana(10.f);
		ASSERT_THAT(AreEqual(1, Burning()));
	}

	// The host applies an effect that grants the tag its own condition forbids; every evaluation is deferred, so it flips once per frame.
	TEST_METHOD(EffectFlippingItsOwnCondition_TogglesAtMostOncePerFrame)
	{
		UArcEffectDefinition* Grant = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_GrantTag_Marker"));
		ASSERT_THAT(IsNotNull(Grant));
		Harness.CaptureEvents(ArcGameEventTags::EffectApplied.GetTag());
		ASSERT_THAT(IsTrue(ApplyHost(TEXT("ED_Host_While_SelfFlip")).Handle.IsValid()));

		int32 AppliedBefore = ArcTriggerTestsPrivate::CountEventsFor<FArcEffectAppliedPayload>(Harness.CapturedEvents,
			ArcGameEventTags::EffectApplied.GetTag(), Grant);
		int32 AppliedInLoop = 0;
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			Pump(1);
			const int32 AppliedNow = ArcTriggerTestsPrivate::CountEventsFor<FArcEffectAppliedPayload>(Harness.CapturedEvents,
				ArcGameEventTags::EffectApplied.GetTag(), Grant);
			ASSERT_THAT(IsTrue(AppliedNow - AppliedBefore <= 1));
			ASSERT_THAT(IsTrue(ArcMechTest::CountActiveEffects(*Harness.EntityManager, Owner, Grant) <= 1));
			AppliedInLoop += AppliedNow - AppliedBefore;
			AppliedBefore = AppliedNow;
		}
		ASSERT_THAT(IsTrue(AppliedInLoop >= 2));
	}
};

TEST_CLASS(ArcTrigger_CastAbility, "ArcMassMechanics.Triggers.CastAbility")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;
	FMassEntityHandle Owner;
	FMassEntityHandle Other;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Owner = Harness.CreateEntity();
		Other = Harness.CreateEntity();
	}

	void RunAbilityProcessors(FMassEntityHandle Entity)
	{
		UArcAbilityInputProcessor* InputProcessor = NewObject<UArcAbilityInputProcessor>();
		InputProcessor->CallInitialize(Harness.World, Harness.EntityManager->AsShared());
		Harness.SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);
		UE::Mass::FProcessingContext InputContext(*Harness.EntityManager, 0.016f);
		InputProcessor->CallExecute(*Harness.EntityManager, InputContext.GetExecutionContext());

		UArcAbilityStateTreeTickProcessor* TickProcessor = NewObject<UArcAbilityStateTreeTickProcessor>();
		TickProcessor->CallInitialize(Harness.World, Harness.EntityManager->AsShared());
		Harness.SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
		UE::Mass::FProcessingContext TickContext(*Harness.EntityManager, 0.016f);
		TickProcessor->CallExecute(*Harness.EntityManager, TickContext.GetExecutionContext());
	}

	TEST_METHOD(PushesAndReleasesTheInput_ActivatingTheGrantedAbility)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcAbilityDefinition* Definition = NewObject<UArcAbilityDefinition>();
		Definition->StateTree = ArcStatusTest::MakeRunForeverTree(Harness.World);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*Harness.EntityManager, Owner, Definition, ArcStatusTestTags::InputA);

		UArcEffectDefinition* HostCastAbility = LoadTrigger(TEXT("ED_Host_CastAbility"));
		ASSERT_THAT(IsNotNull(HostCastAbility));
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, HostCastAbility, Owner);
		Harness.DrainPendingOps(Owner);
		Harness.DrainEvents();
		ASSERT_THAT(IsFalse(ArcAbilities::IsAbilityActive(*Harness.EntityManager, Owner, Handle)));

		UArcEffectDefinition* CastSpark = LoadTrigger(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(CastSpark));
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, CastSpark, Other);
		Harness.DrainPendingOps(Owner);
		Harness.DrainEvents();

		const FArcAbilityInputFragment* Input = FMassEntityView(*Harness.EntityManager, Owner).GetSparseFragmentDataPtr<FArcAbilityInputFragment>();
		ASSERT_THAT(IsNotNull(Input));
		ASSERT_THAT(IsTrue(Input->PressedThisFrame.Contains(ArcStatusTestTags::InputA.GetTag())));
		ASSERT_THAT(IsFalse(Input->HeldInputs.HasTagExact(ArcStatusTestTags::InputA)));

		RunAbilityProcessors(Owner);
		ASSERT_THAT(IsTrue(ArcAbilities::IsAbilityActive(*Harness.EntityManager, Owner, Handle)));
	}
};

TEST_CLASS(ArcTrigger_ProcsNeverTriggerProcs, "ArcMassMechanics.Triggers.ProcsNeverTriggerProcs")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;
	FMassEntityHandle Owner;
	FMassEntityHandle Other;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
		Owner = Harness.CreateEntity();
		Other = Harness.CreateEntity();
	}

	void Pump(int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Harness.DrainPendingOps(Owner);
			Harness.DrainEvents();
		}
	}

	TEST_METHOD(SingleTrigger_ReactingToItsOwnAppliedEffect_FiresOnce)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* HostOnApplied = LoadTrigger(TEXT("ED_Host_On_Applied"));
		ASSERT_THAT(IsNotNull(HostOnApplied));
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, HostOnApplied, Owner);
		Pump(1);

		UArcEffectDefinition* Bleed = LoadTrigger(TEXT("ED_Reward_Bleeding"));
		ASSERT_THAT(IsNotNull(Bleed));
		UArcEffectDefinition* BurningReward = LoadTrigger(TEXT("ED_Reward_Burning"));
		ASSERT_THAT(IsNotNull(BurningReward));
		const FGameplayTag Applied = ArcGameEventTags::EffectApplied.GetTag();
		Harness.CaptureEvents(Applied);
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, Bleed, Other);
		Pump(6);

		ASSERT_THAT(AreEqual(1, Bleeding(Harness, Owner)));
		ASSERT_THAT(AreEqual(1, Burning(Harness, Owner)));
		ASSERT_THAT(AreEqual(1, CountEventsFor<FArcEffectAppliedPayload>(Harness.CapturedEvents, Applied, BurningReward)));
	}

	TEST_METHOD(TwoTriggers_ReactingToInstantExecutedEvents_FireOnceEach)
	{
		using namespace ArcTriggerTestsPrivate;
		UArcEffectDefinition* Ping = LoadTrigger(TEXT("ED_Spark_Marker"));
		ASSERT_THAT(IsNotNull(Ping));
		UArcEffectDefinition* Pong = LoadTrigger(TEXT("ED_Ping"));
		ASSERT_THAT(IsNotNull(Pong));
		UArcEffectDefinition* SparkEffect = LoadTrigger(TEXT("ED_Spark"));
		ASSERT_THAT(IsNotNull(SparkEffect));

		UArcEffectDefinition* HostOnPingPong = LoadTrigger(TEXT("ED_Host_On_PingPong"));
		ASSERT_THAT(IsNotNull(HostOnPingPong));
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, HostOnPingPong, Owner);
		Pump(1);

		const FGameplayTag Executed = ArcGameEventTags::EffectExecuted.GetTag();
		Harness.CaptureEvents(Executed);
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Owner, SparkEffect, Other);
		Pump(6);

		ASSERT_THAT(AreEqual(1, CountEventsFor<FArcEffectExecutedPayload>(Harness.CapturedEvents, Executed, SparkEffect)));
		ASSERT_THAT(AreEqual(1, CountEventsFor<FArcEffectExecutedPayload>(Harness.CapturedEvents, Executed, Ping)));
		ASSERT_THAT(AreEqual(1, CountEventsFor<FArcEffectExecutedPayload>(Harness.CapturedEvents, Executed, Pong)));
		ASSERT_THAT(AreEqual(3, Harness.CountCaptured(Executed)));
	}
};

TEST_CLASS(ArcTrigger_Validator, "ArcMassMechanics.Triggers.Validator")
{
	TOptional<ArcMechTest::FVerboseLogScope> LogScope;
	UEditorValidatorBase* Validator = nullptr;
	UArcEffectDefinition* Reward = nullptr;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		UClass* ValidatorClass = FindObject<UClass>(nullptr, TEXT("/Script/ArcMassMechanicsEditor.ArcTriggerValidator"));
		ASSERT_THAT(IsNotNull(ValidatorClass));
		Validator = NewObject<UEditorValidatorBase>(GetTransientPackage(), ValidatorClass);
		Reward = ArcTriggerTestsPrivate::LoadTrigger(TEXT("ED_Reward_Burning"));
		ASSERT_THAT(IsNotNull(Reward));
	}

	bool FailsWith(const FInstancedStruct& Task, const TCHAR* Fragment, EArcEffectDuration Duration = EArcEffectDuration::Infinite)
	{
		UArcEffectDefinition* Host = ArcTriggerTestsPrivate::MakeHost(Task);
		Host->StackingPolicy.DurationType = Duration;
		FDataValidationContext Context;
		const EDataValidationResult Result = Validator->ValidateLoadedAsset(FAssetData(), Host, Context);
		if (Result != EDataValidationResult::Invalid)
		{
			return false;
		}
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			if (Issue.Message.ToString().Contains(Fragment))
			{
				return true;
			}
		}
		return false;
	}

	// Loads a host and returns its only task when it is of type T, so a Valid result can never come from a host without that task.
	template <typename T>
	const T* LoadOnlyTask(const TCHAR* HostName, UArcEffectDefinition*& OutHost)
	{
		OutHost = ArcTriggerTestsPrivate::LoadTrigger(HostName);
		if (!OutHost || OutHost->Tasks.Num() != 1)
		{
			return nullptr;
		}
		return OutHost->Tasks[0].GetPtr<T>();
	}

	EDataValidationResult Validate(UArcEffectDefinition* Host)
	{
		FDataValidationContext Context;
		return Validator->ValidateLoadedAsset(FAssetData(), Host, Context);
	}

	FArcEffectTask_TriggerOn MakeValidOn()
	{
		FArcEffectTask_TriggerOn Trigger = ArcTriggerTestsPrivate::MakeOn(EArcEventRole::Target, ArcGameEventTags::EffectExecuted);
		Trigger.Effects.Add(ArcTriggerTestsPrivate::MakeApply(Reward));
		return Trigger;
	}

	TEST_METHOD(ValidOn_Passes)
	{
		UArcEffectDefinition* Host = nullptr;
		const FArcEffectTask_TriggerOn* On = LoadOnlyTask<FArcEffectTask_TriggerOn>(TEXT("ED_Host_On_Chance1"), Host);
		ASSERT_THAT(IsNotNull(On));
		ASSERT_THAT(IsTrue(On->EventTag.IsValid()));
		ASSERT_THAT(IsNear(1.f, On->Chance, 0.0001f));
		ASSERT_THAT(AreEqual(1, On->Effects.Num()));
		ASSERT_THAT(IsTrue(Validate(Host) == EDataValidationResult::Valid));
	}

	TEST_METHOD(ChanceBelowOneOnOn_Passes)
	{
		UArcEffectDefinition* Host = nullptr;
		const FArcEffectTask_TriggerOn* On = LoadOnlyTask<FArcEffectTask_TriggerOn>(TEXT("ED_Host_On_ChanceHalf"), Host);
		ASSERT_THAT(IsNotNull(On));
		ASSERT_THAT(IsTrue(On->Chance > 0.f && On->Chance < 1.f));
		ASSERT_THAT(IsTrue(Validate(Host) == EDataValidationResult::Valid));
	}

	TEST_METHOD(OnTagChannelWithoutTag_Fails)
	{
		for (const FGameplayTag& Channel : {ArcGameEventTags::TagAdded.GetTag(), ArcGameEventTags::TagRemoved.GetTag()})
		{
			FArcEffectTask_TriggerOn Trigger = ArcTriggerTestsPrivate::MakeOn(EArcEventRole::Target, Channel);
			Trigger.Effects.Add(ArcTriggerTestsPrivate::MakeApply(Reward));
			ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(Trigger), TEXT("needs Tag"))));
		}
	}

	TEST_METHOD(OnTagChannelWithTag_Passes)
	{
		for (const FGameplayTag& Channel : {ArcGameEventTags::TagAdded.GetTag(), ArcGameEventTags::TagRemoved.GetTag()})
		{
			FArcEffectTask_TriggerOn Trigger = ArcTriggerTestsPrivate::MakeOn(EArcEventRole::Target, Channel);
			Trigger.Tag = ArcMassMechanicsTags::State_Stun;
			Trigger.Effects.Add(ArcTriggerTestsPrivate::MakeApply(Reward));
			ASSERT_THAT(IsTrue(Validate(ArcTriggerTestsPrivate::MakeHost(FInstancedStruct::Make(Trigger))) == EDataValidationResult::Valid));
		}
	}

	TEST_METHOD(OnTagOnAnotherChannel_Fails)
	{
		FArcEffectTask_TriggerOn Trigger = MakeValidOn();
		Trigger.Tag = ArcMassMechanicsTags::State_Stun;
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(Trigger), TEXT("Tag is read only"))));
	}

	TEST_METHOD(OnChanceZero_Fails)
	{
		FArcEffectTask_TriggerOn Trigger = MakeValidOn();
		Trigger.Chance = 0.f;
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(Trigger), TEXT("needs an authored Chance in (0, 1]"))));
	}

	TEST_METHOD(WhileHost_Passes)
	{
		UArcEffectDefinition* Host = nullptr;
		const FArcEffectTask_TriggerWhile* While = LoadOnlyTask<FArcEffectTask_TriggerWhile>(TEXT("ED_Host_While"), Host);
		ASSERT_THAT(IsNotNull(While));
		ASSERT_THAT(AreEqual(1, While->Conditions.Num()));
		ASSERT_THAT(IsTrue(Validate(Host) == EDataValidationResult::Valid));
	}

	TEST_METHOD(WhileThresholdHost_Passes)
	{
		UArcEffectDefinition* Host = nullptr;
		const FArcEffectTask_TriggerWhile* While = LoadOnlyTask<FArcEffectTask_TriggerWhile>(TEXT("ED_Host_While_AttributeThreshold"), Host);
		ASSERT_THAT(IsNotNull(While));
		ASSERT_THAT(AreEqual(1, While->Conditions.Num()));
		ASSERT_THAT(IsNotNull(While->Conditions[0].GetPtr<FArcTriggerCondition_AttributeThreshold>()));
		ASSERT_THAT(IsTrue(Validate(Host) == EDataValidationResult::Valid));
	}

	TEST_METHOD(WhileWithoutConditions_Fails)
	{
		FArcEffectTask_TriggerWhile While;
		While.Effects.Add(ArcTriggerTestsPrivate::MakeApply(Reward));
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(While), TEXT("A While trigger needs at least one condition"))));
	}

	TEST_METHOD(IfWithOnlyModifiers_Passes)
	{
		UArcEffectDefinition* Host = nullptr;
		const FArcEffectTask_TriggerIf* If = LoadOnlyTask<FArcEffectTask_TriggerIf>(TEXT("ED_Host_If_DoneMultiplier"), Host);
		ASSERT_THAT(IsNotNull(If));
		ASSERT_THAT(AreEqual(1, If->Modifiers.Num()));
		ASSERT_THAT(AreEqual(0, If->Effects.Num()));
		ASSERT_THAT(IsTrue(Validate(Host) == EDataValidationResult::Valid));
	}

	TEST_METHOD(InstantHost_Fails)
	{
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(MakeValidOn()), TEXT("only runs on a stored effect"), EArcEffectDuration::Instant)));
	}

	TEST_METHOD(EmptyConditionEntry_Fails)
	{
		FArcEffectTask_TriggerOn Trigger = MakeValidOn();
		Trigger.Conditions.Add(FInstancedStruct());
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(Trigger), TEXT("A condition entry is empty"))));
	}

	TEST_METHOD(EmptyEffectEntry_Fails)
	{
		FArcEffectTask_TriggerOn Trigger = MakeValidOn();
		Trigger.Effects.Add(FInstancedStruct());
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(Trigger), TEXT("An effect entry is empty"))));
	}

	TEST_METHOD(IfWithoutModifiersOrEffects_Fails)
	{
		const FArcEffectTask_TriggerIf EmptyIf = ArcTriggerTestsPrivate::MakeIf(EArcEventRole::Instigator, {});
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(EmptyIf), TEXT("needs Modifiers or Effects"))));
	}

	TEST_METHOD(DoneOnTarget_Fails)
	{
		UArcEffectDefinition* Host = nullptr;
		const FArcEffectTask_TriggerIf* If = LoadOnlyTask<FArcEffectTask_TriggerIf>(TEXT("ED_Host_If_DoneMultiplier"), Host);
		ASSERT_THAT(IsNotNull(If));
		ASSERT_THAT(IsTrue(If->Role == EArcEventRole::Instigator));
		ASSERT_THAT(IsTrue(Validate(Host) == EDataValidationResult::Valid));

		const FArcEffectTask_TriggerIf Trigger = ArcTriggerTestsPrivate::MakeIf(EArcEventRole::Target,
			{ArcTriggerTestsPrivate::MakeModifier(EArcIfChannel::DamageDoneMultiplier, 0.5f)});
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(Trigger), TEXT("A Done channel needs the owner to be the hit's source"))));
	}

	TEST_METHOD(TakenOnInstigator_Fails)
	{
		UArcEffectDefinition* Host = nullptr;
		const FArcEffectTask_TriggerIf* If = LoadOnlyTask<FArcEffectTask_TriggerIf>(TEXT("ED_Host_If_TakenFlatMultiplier"), Host);
		ASSERT_THAT(IsNotNull(If));
		ASSERT_THAT(IsTrue(If->Role == EArcEventRole::Target));
		ASSERT_THAT(IsTrue(Validate(Host) == EDataValidationResult::Valid));

		const FArcEffectTask_TriggerIf Trigger = ArcTriggerTestsPrivate::MakeIf(EArcEventRole::Instigator,
			{ArcTriggerTestsPrivate::MakeModifier(EArcIfChannel::DamageTakenFlat, 5.f)});
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(Trigger), TEXT("A Taken channel needs the owner to be the struck target"))));
	}

	TEST_METHOD(MultiplierBelowMinusOne_Fails)
	{
		UArcEffectDefinition* Host = nullptr;
		const FArcEffectTask_TriggerIf* If = LoadOnlyTask<FArcEffectTask_TriggerIf>(TEXT("ED_Host_If_TakenFlatMultiplier"), Host);
		ASSERT_THAT(IsNotNull(If));
		ASSERT_THAT(IsTrue(If->Modifiers.ContainsByPredicate([](const FArcIfModifier& Modifier)
		{
			return Modifier.Channel == EArcIfChannel::DamageTakenMultiplier && Modifier.Value < 0.f && Modifier.Value >= -1.f;
		})));
		ASSERT_THAT(IsTrue(Validate(Host) == EDataValidationResult::Valid));

		const FArcEffectTask_TriggerIf Trigger = ArcTriggerTestsPrivate::MakeIf(EArcEventRole::Target,
			{ArcTriggerTestsPrivate::MakeModifier(EArcIfChannel::DamageTakenMultiplier, -1.5f)});
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(Trigger), TEXT("below -1 would flip the sign"))));
	}

	TEST_METHOD(CritChanceNeedsRoll_Fails)
	{
		UArcEffectDefinition* Host = nullptr;
		const FArcEffectTask_TriggerIf* If = LoadOnlyTask<FArcEffectTask_TriggerIf>(TEXT("ED_Host_If_HitResult"), Host);
		ASSERT_THAT(IsNotNull(If));
		ASSERT_THAT(AreEqual(1, If->Conditions.Num()));
		const FArcTriggerCondition_HitResult* HitResult = If->Conditions[0].GetPtr<FArcTriggerCondition_HitResult>();
		ASSERT_THAT(IsNotNull(HitResult));
		ASSERT_THAT(IsTrue(HitResult->Crit == EArcTriggerCritFilter::Any && HitResult->Zones.IsEmpty()));
		ASSERT_THAT(IsTrue(If->Modifiers.ContainsByPredicate([](const FArcIfModifier& Modifier) { return Modifier.Channel == EArcIfChannel::CritChance; })));
		ASSERT_THAT(IsTrue(Validate(Host) == EDataValidationResult::Valid));

		FArcTriggerCondition_HitResult CritFilter;
		CritFilter.Crit = EArcTriggerCritFilter::Crit;
		FArcEffectTask_TriggerIf Trigger = ArcTriggerTestsPrivate::MakeIf(EArcEventRole::Instigator,
			{ArcTriggerTestsPrivate::MakeModifier(EArcIfChannel::CritChance, 0.25f)});
		Trigger.Conditions.Add(FInstancedStruct::Make(CritFilter));
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(Trigger), TEXT("read before the crit and zone rolls"))));
	}

	TEST_METHOD(ThresholdOnOn_Fails)
	{
		FArcTriggerCondition_AttributeThreshold Threshold;
		Threshold.Attribute = FArcMechTest_CostSet::GetManaAttribute();
		FArcEffectTask_TriggerOn Trigger = MakeValidOn();
		Trigger.Conditions.Add(FInstancedStruct::Make(Threshold));
		ASSERT_THAT(IsTrue(FailsWith(FInstancedStruct::Make(Trigger), TEXT("cannot use a stat threshold"))));
	}
};
