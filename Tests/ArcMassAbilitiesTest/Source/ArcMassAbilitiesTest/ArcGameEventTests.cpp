// Copyright Lukasz Baran. All Rights Reserved.
//
// CQTest coverage for the game event bus (UArcGameEventSubsystem) and the AMA lifecycle events it
// carries. Plan: docs/plans/2026-10-04-1408-arc-mechanics-plan1-ama-extensions-plan.md
// (Steps 2, 3, 4, 8; test list 16b).
//
// Topology: standalone world (FActorTestSpawner) with a real FMassEntityManager. The bus has no
// replication surface of its own; every event is queued and drained on the game thread. The drain is
// driven through the production entry point DrainOnPostActorTick.
//
// Known gap: FArcEffectSTTask_WaitEffectApplied cannot run through a compiled effect StateTree.
// The task declares TStateTreeExternalDataHandle members (EffectContextHandle, OwnerEntityHandle) but
// has no Link override, so both handles stay invalid and EnterState hits check(Handle.IsValid()) in
// FStateTreeExecutionContext::GetExternalData (StateTreeExecutionContext.h:717). Even once linked, the external-data callback in
// FArcEffectStateTreeContext::SetContextRequirements only serves Mass fragments and world subsystems,
// so those Required externals would fail the tree start. The bus-level
// Applied_RoleSubscriptions test pins the exact subscription the task makes (Applied, owner entity,
// Target role) and asserts the Instigator it reads SourceEntity from.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectComp_ChanceToApply.h"
#include "Effects/ArcEffectComp_GrantImmunity.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectDurationProcessor.h"
#include "Effects/ArcEffectFunctions.h"
#include "Engine/World.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Events/ArcEffectEventPayloads.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcImmunityFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "NativeGameplayTags.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StructUtils/InstancedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameEventTest_Alpha, "Test.GameEvent.Alpha");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameEventTest_AlphaLeaf, "Test.GameEvent.Alpha.Leaf");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameEventTest_Beta, "Test.GameEvent.Beta");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameEventTest_FireImmunity, "Test.GameEvent.Immunity.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameEventTest_RequiredState, "Test.GameEvent.State.Required");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameEventTest_InputFire, "Test.InputTag.Ability.Fire");

namespace ArcGameEventTestsPrivate
{

FMassEntityHandle CreateEffectEntity(FMassEntityManager& EntityManager)
{
	FArcAttributeSet_TestStats Stats;
	Stats.Health.BaseValue = 100.f;
	Stats.Health.CurrentValue = 100.f;

	UE::Mass::FEntityBuilder Builder(EntityManager);
	return Builder
		.Add<FArcAttributeSet_TestStats>(Stats)
		.Add<FArcEffectStackFragment>()
		.Add<FArcAggregatorFragment>()
		.Add<FArcOwnedTagsFragment>()
		.Add<FArcPendingAttributeOpsFragment>()
		.Add<FArcImmunityFragment>()
		.Commit();
}

FMassEntityHandle CreateAbilityEntity(FMassEntityManager& EntityManager)
{
	FArcAttributeSet_TestStats Stats;
	Stats.Health.BaseValue = 100.f;
	Stats.Health.CurrentValue = 100.f;

	TArray<FInstancedStruct> Instances;
	Instances.Add(FInstancedStruct::Make(Stats));
	Instances.Add(FInstancedStruct::Make(FArcAbilityCollectionFragment()));
	Instances.Add(FInstancedStruct::Make(FArcAbilityInputFragment()));
	Instances.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
	Instances.Add(FInstancedStruct::Make(FArcAbilityCooldownFragment()));
	Instances.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
	Instances.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
	Instances.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

	return ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(EntityManager, Instances);
}

UStateTree* CreateLongRunningStateTree(UObject* Outer)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);
	Root.AddTask<FArcTestExitFlagTask>();

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	check(Compiler.Compile(*StateTree));
	return StateTree;
}

FArcGameEventDelegate MakeRecorder(TArray<FArcGameEvent>& OutEvents)
{
	return FArcGameEventDelegate::CreateLambda([&OutEvents](const FArcGameEvent& Event)
	{
		OutEvents.Add(Event);
	});
}

FArcGameEventDelegate MakeCounter(int32& OutCount)
{
	return FArcGameEventDelegate::CreateLambda([&OutCount](const FArcGameEvent& /*Event*/)
	{
		++OutCount;
	});
}

int32 CountWithTag(const TArray<FArcGameEvent>& Events, FGameplayTag Tag)
{
	int32 Count = 0;
	for (const FArcGameEvent& Event : Events)
	{
		if (Event.Tag == Tag)
		{
			++Count;
		}
	}
	return Count;
}

const FArcGameEvent* FindEvent(const TArray<FArcGameEvent>& Events, FGameplayTag Tag, int32 Occurrence)
{
	int32 Seen = 0;
	for (const FArcGameEvent& Event : Events)
	{
		if (Event.Tag != Tag)
		{
			continue;
		}
		if (Seen == Occurrence)
		{
			return &Event;
		}
		++Seen;
	}
	return nullptr;
}

const UArcEffectDefinition* SpecDefinition(const FSharedStruct& Spec)
{
	const FArcEffectSpec* Resolved = Spec.GetPtr<FArcEffectSpec>();
	return Resolved ? Resolved->Definition.Get() : nullptr;
}

UArcEffectDefinition* MakeInstantExecutionEffect()
{
	UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
	Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

	FArcTestAddExecution Exec;
	Exec.TargetAttribute = FArcAttributeSet_TestStats::GetHealthAttribute();
	Exec.Value = -10.f;
	Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));
	return Effect;
}

UArcEffectDefinition* MakeInfiniteEffect()
{
	UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
	Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
	return Effect;
}

UArcEffectDefinition* MakeModifierOnlyDurationEffect(int32 MaxStacks)
{
	UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
	Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
	Effect->StackingPolicy.Duration = 10.f;
	Effect->StackingPolicy.StackType = EArcStackType::Counter;
	Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
	Effect->StackingPolicy.MaxStacks = MaxStacks;

	FArcEffectModifier Mod;
	Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
	Mod.Operation = EArcModifierOp::Add;
	Mod.Magnitude = FArcScalableFloat(10.f);
	Effect->Modifiers.Add(Mod);
	return Effect;
}

UArcEffectDefinition* MakeStoredPeriodicEffect(EArcPeriodicExecutionPolicy ExecPolicy)
{
	UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
	Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
	Effect->StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
	Effect->StackingPolicy.PeriodicExecPolicy = ExecPolicy;
	Effect->StackingPolicy.Duration = 10.f;
	Effect->StackingPolicy.Period = 2.f;

	FArcTestAddExecution Exec;
	Exec.TargetAttribute = FArcAttributeSet_TestStats::GetHealthAttribute();
	Exec.Value = -1.f;
	Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));
	return Effect;
}

} // namespace ArcGameEventTestsPrivate

// ============================================================================
// ArcGameEvent_Delivery
// ============================================================================

TEST_CLASS(ArcGameEvent_Delivery, "ArcMassAbilities.GameEventBus.Delivery")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UArcGameEventSubsystem* Bus = nullptr;

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
		Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(Bus);
	}

	void Drain()
	{
		Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
	}

	void QueueEvent(FGameplayTag Tag, FMassEntityHandle Instigator, FMassEntityHandle Target, float Magnitude = 0.f)
	{
		FArcGameEvent Event;
		Event.Tag = Tag;
		Event.Instigator = Instigator;
		Event.Target = Target;
		Event.Magnitude = Magnitude;
		Bus->Queue(MoveTemp(Event));
	}

	FMassEntityHandle CreateEntity()
	{
		return ArcGameEventTestsPrivate::CreateEffectEntity(*EntityManager);
	}

	TEST_METHOD(NothingDeliveredBeforeDrain_DeliveredExactlyOnceAfter)
	{
		int32 Count = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(Count));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		ASSERT_THAT(AreEqual(0, Count, TEXT("event must not be delivered before the drain")));

		Drain();
		ASSERT_THAT(AreEqual(1, Count, TEXT("event must be delivered by the drain")));

		Drain();
		ASSERT_THAT(AreEqual(1, Count, TEXT("a delivered event must not be redelivered by the next drain")));
	}

	TEST_METHOD(PostActorTickBinding_DeliversQueuedEventsOncePerBroadcast)
	{
		int32 Count = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(Count));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		ASSERT_THAT(AreEqual(0, Count, TEXT("event must not be delivered before the post-actor-tick broadcast")));

		FWorldDelegates::OnWorldPostActorTick.Broadcast(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
		ASSERT_THAT(AreEqual(1, Count, TEXT("the production post-actor-tick binding must drain the queue")));

		FWorldDelegates::OnWorldPostActorTick.Broadcast(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
		ASSERT_THAT(AreEqual(1, Count, TEXT("a delivered event must not be redelivered by the next broadcast")));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		FWorldDelegates::OnWorldPostActorTick.Broadcast(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
		ASSERT_THAT(AreEqual(2, Count));
	}

	TEST_METHOD(EventFieldsAndOrderArePreserved)
	{
		const FMassEntityHandle Instigator = CreateEntity();
		const FMassEntityHandle Target = CreateEntity();

		TArray<FArcGameEvent> Events;
		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeRecorder(Events));

		FArcTestSourcePayload Payload;
		Payload.DamageMultiplier = 3.f;
		FArcGameEvent First;
		First.Tag = TAG_GameEventTest_Alpha;
		First.Instigator = Instigator;
		First.Target = Target;
		First.Magnitude = 1.f;
		First.Payload = FInstancedStruct::Make(Payload);
		Bus->Queue(MoveTemp(First));
		QueueEvent(TAG_GameEventTest_Alpha, Instigator, Target, 2.f);
		QueueEvent(TAG_GameEventTest_Alpha, Instigator, Target, 3.f);

		Drain();

		ASSERT_THAT(AreEqual(3, Events.Num()));
		ASSERT_THAT(IsNear(1.f, Events[0].Magnitude, 0.001f));
		ASSERT_THAT(IsNear(2.f, Events[1].Magnitude, 0.001f));
		ASSERT_THAT(IsNear(3.f, Events[2].Magnitude, 0.001f));
		ASSERT_THAT(IsTrue(Events[0].Instigator == Instigator, TEXT("instigator preserved")));
		ASSERT_THAT(IsTrue(Events[0].Target == Target, TEXT("target preserved")));

		const FArcTestSourcePayload* Delivered = Events[0].Payload.GetPtr<FArcTestSourcePayload>();
		ASSERT_THAT(IsNotNull(Delivered, TEXT("payload preserved")));
		ASSERT_THAT(IsNear(3.f, Delivered->DamageMultiplier, 0.001f));
	}

	TEST_METHOD(GlobalSubscription_ReceivesOnlyMatchingTag)
	{
		int32 AlphaCount = 0;
		int32 BetaCount = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(AlphaCount));
		Bus->Subscribe(TAG_GameEventTest_Beta, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(BetaCount));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();

		ASSERT_THAT(AreEqual(1, AlphaCount));
		ASSERT_THAT(AreEqual(0, BetaCount, TEXT("unrelated tag must not receive the event")));
	}

	TEST_METHOD(GlobalSubscription_ReceivesEventOncePerEventRegardlessOfEntities)
	{
		const FMassEntityHandle Instigator = CreateEntity();
		const FMassEntityHandle Target = CreateEntity();

		int32 GlobalCount = 0;
		int32 InstigatorCount = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(GlobalCount));
		Bus->Subscribe(TAG_GameEventTest_Alpha, Instigator, EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(InstigatorCount));

		QueueEvent(TAG_GameEventTest_Alpha, Instigator, Target);
		Drain();

		ASSERT_THAT(AreEqual(1, GlobalCount, TEXT("global subscription must fire once, not once per participant")));
		ASSERT_THAT(AreEqual(1, InstigatorCount));
	}

	TEST_METHOD(ExactMatch_Global_ParentAndChildSubscriptionsNeverReceiveEachOther)
	{
		int32 ParentSubCount = 0;
		int32 ChildSubCount = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(ParentSubCount));
		Bus->Subscribe(TAG_GameEventTest_AlphaLeaf, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(ChildSubCount));

		QueueEvent(TAG_GameEventTest_AlphaLeaf, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(0, ParentSubCount, TEXT("a parent-tag subscription must not receive the child-tag event")));
		ASSERT_THAT(AreEqual(1, ChildSubCount));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(1, ParentSubCount));
		ASSERT_THAT(AreEqual(1, ChildSubCount, TEXT("a child-tag subscription must not receive the parent-tag event")));
	}

	TEST_METHOD(ExactMatch_PerEntity_ParentAndChildSubscriptionsNeverReceiveEachOther)
	{
		const FMassEntityHandle Entity = CreateEntity();

		int32 ParentSubCount = 0;
		int32 ChildSubCount = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, Entity, EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(ParentSubCount));
		Bus->Subscribe(TAG_GameEventTest_AlphaLeaf, Entity, EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(ChildSubCount));

		QueueEvent(TAG_GameEventTest_AlphaLeaf, Entity, FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(0, ParentSubCount, TEXT("a parent-tag subscription must not receive the child-tag event")));
		ASSERT_THAT(AreEqual(1, ChildSubCount));

		QueueEvent(TAG_GameEventTest_Alpha, Entity, FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(1, ParentSubCount));
		ASSERT_THAT(AreEqual(1, ChildSubCount, TEXT("a child-tag subscription must not receive the parent-tag event")));
	}

	TEST_METHOD(ExactMatch_UnrelatedSiblingTagDoesNotMatch)
	{
		const FMassEntityHandle Entity = CreateEntity();

		int32 GlobalCount = 0;
		int32 EntityCount = 0;
		Bus->Subscribe(TAG_GameEventTest_AlphaLeaf, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(GlobalCount));
		Bus->Subscribe(TAG_GameEventTest_AlphaLeaf, Entity, EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(EntityCount));

		QueueEvent(TAG_GameEventTest_Beta, Entity, FMassEntityHandle());
		Drain();

		ASSERT_THAT(AreEqual(0, GlobalCount));
		ASSERT_THAT(AreEqual(0, EntityCount));
	}

	TEST_METHOD(PerEntity_RoleFiltering)
	{
		const FMassEntityHandle A = CreateEntity();
		const FMassEntityHandle B = CreateEntity();
		const FMassEntityHandle C = CreateEntity();

		int32 AInstigator = 0;
		int32 ATarget = 0;
		int32 AAny = 0;
		int32 BInstigator = 0;
		int32 BTarget = 0;
		int32 BAny = 0;
		int32 CAny = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, A, EArcEventRole::Instigator, ArcGameEventTestsPrivate::MakeCounter(AInstigator));
		Bus->Subscribe(TAG_GameEventTest_Alpha, A, EArcEventRole::Target, ArcGameEventTestsPrivate::MakeCounter(ATarget));
		Bus->Subscribe(TAG_GameEventTest_Alpha, A, EArcEventRole::Any, ArcGameEventTestsPrivate::MakeCounter(AAny));
		Bus->Subscribe(TAG_GameEventTest_Alpha, B, EArcEventRole::Instigator, ArcGameEventTestsPrivate::MakeCounter(BInstigator));
		Bus->Subscribe(TAG_GameEventTest_Alpha, B, EArcEventRole::Target, ArcGameEventTestsPrivate::MakeCounter(BTarget));
		Bus->Subscribe(TAG_GameEventTest_Alpha, B, EArcEventRole::Any, ArcGameEventTestsPrivate::MakeCounter(BAny));
		Bus->Subscribe(TAG_GameEventTest_Alpha, C, EArcEventRole::Any, ArcGameEventTestsPrivate::MakeCounter(CAny));

		QueueEvent(TAG_GameEventTest_Alpha, A, B);
		Drain();

		ASSERT_THAT(AreEqual(1, AInstigator, TEXT("Instigator role on the instigator")));
		ASSERT_THAT(AreEqual(0, ATarget, TEXT("Target role on the instigator must not fire")));
		ASSERT_THAT(AreEqual(1, AAny, TEXT("Any role on the instigator")));
		ASSERT_THAT(AreEqual(0, BInstigator, TEXT("Instigator role on the target must not fire")));
		ASSERT_THAT(AreEqual(1, BTarget, TEXT("Target role on the target")));
		ASSERT_THAT(AreEqual(1, BAny, TEXT("Any role on the target")));
		ASSERT_THAT(AreEqual(0, CAny, TEXT("an entity that is neither instigator nor target must not fire")));
	}

	TEST_METHOD(PerEntity_SelfTargetedEvent_DeliveredOncePerSubscription)
	{
		const FMassEntityHandle A = CreateEntity();

		int32 AInstigator = 0;
		int32 ATarget = 0;
		int32 AAny = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, A, EArcEventRole::Instigator, ArcGameEventTestsPrivate::MakeCounter(AInstigator));
		Bus->Subscribe(TAG_GameEventTest_Alpha, A, EArcEventRole::Target, ArcGameEventTestsPrivate::MakeCounter(ATarget));
		Bus->Subscribe(TAG_GameEventTest_Alpha, A, EArcEventRole::Any, ArcGameEventTestsPrivate::MakeCounter(AAny));

		QueueEvent(TAG_GameEventTest_Alpha, A, A);
		Drain();

		ASSERT_THAT(AreEqual(1, AInstigator));
		ASSERT_THAT(AreEqual(1, ATarget));
		ASSERT_THAT(AreEqual(1, AAny, TEXT("a self-targeted event must not be delivered twice to one subscription")));
	}

	TEST_METHOD(EventQueuedInsideCallback_DeliveredOnNextDrainNotCurrent)
	{
		int32 Count = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			FArcGameEventDelegate::CreateLambda([this, &Count](const FArcGameEvent& /*Event*/)
			{
				++Count;
				if (Count == 1)
				{
					QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
				}
			}));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());

		Drain();
		ASSERT_THAT(AreEqual(1, Count, TEXT("an event queued during the drain must not be delivered in the same drain")));

		Drain();
		ASSERT_THAT(AreEqual(2, Count, TEXT("the re-queued event must arrive on the next drain")));

		Drain();
		ASSERT_THAT(AreEqual(2, Count));
	}

	TEST_METHOD(Unsubscribe_StopsDelivery)
	{
		int32 Count = 0;
		const FDelegateHandle Handle = Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(Count));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(1, Count));

		Bus->Unsubscribe(Handle);
		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(1, Count));
	}

	TEST_METHOD(UnsubscribeOtherSubscriptionDuringDispatch_IsSafeAndSkipsIt)
	{
		int32 FirstCount = 0;
		int32 SecondCount = 0;
		FDelegateHandle SecondHandle;

		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			FArcGameEventDelegate::CreateLambda([this, &FirstCount, &SecondHandle](const FArcGameEvent& /*Event*/)
			{
				++FirstCount;
				Bus->Unsubscribe(SecondHandle);
			}));
		SecondHandle = Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(SecondCount));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(1, FirstCount));
		ASSERT_THAT(AreEqual(0, SecondCount, TEXT("a subscription removed mid-dispatch must not be called afterwards")));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(2, FirstCount));
		ASSERT_THAT(AreEqual(0, SecondCount));
	}

	TEST_METHOD(UnsubscribeSelfDuringDispatch_IsSafeAndStopsFurtherDelivery)
	{
		int32 Count = 0;
		FDelegateHandle Handle;
		Handle = Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			FArcGameEventDelegate::CreateLambda([this, &Count, &Handle](const FArcGameEvent& /*Event*/)
			{
				++Count;
				Bus->Unsubscribe(Handle);
			}));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(1, Count));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(1, Count));
	}

	TEST_METHOD(UnsubscribePerEntitySubscriptionDuringDispatch_IsSafe)
	{
		const FMassEntityHandle Entity = CreateEntity();

		int32 Count = 0;
		FDelegateHandle Handle;
		Handle = Bus->Subscribe(TAG_GameEventTest_Alpha, Entity, EArcEventRole::Any,
			FArcGameEventDelegate::CreateLambda([this, &Count, &Handle](const FArcGameEvent& /*Event*/)
			{
				++Count;
				Bus->Unsubscribe(Handle);
			}));

		QueueEvent(TAG_GameEventTest_Alpha, Entity, FMassEntityHandle());
		Drain();
		QueueEvent(TAG_GameEventTest_Alpha, Entity, FMassEntityHandle());
		Drain();

		ASSERT_THAT(AreEqual(1, Count));
	}

	TEST_METHOD(SubscribeDuringDispatch_NewSubscriptionReceivesOnlyLaterEvents)
	{
		int32 LateCount = 0;
		bool bSubscribed = false;
		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			FArcGameEventDelegate::CreateLambda([this, &LateCount, &bSubscribed](const FArcGameEvent& /*Event*/)
			{
				if (!bSubscribed)
				{
					bSubscribed = true;
					Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
						ArcGameEventTestsPrivate::MakeCounter(LateCount));
				}
			}));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(0, LateCount, TEXT("a subscription added mid-dispatch must not see the in-flight event")));

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(1, LateCount, TEXT("the new subscription must receive later events")));
	}

	TEST_METHOD(DestroyedEntity_NeverTouchedSubscription_EvictedBySweep)
	{
		const FMassEntityHandle Doomed = CreateEntity();

		int32 DoomedCount = 0;
		int32 ControlCount = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, Doomed, EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(DoomedCount));
		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(ControlCount));

		EntityManager->DestroyEntity(Doomed);
		ASSERT_THAT(IsFalse(EntityManager->IsEntityValid(Doomed)));

		Drain();

		QueueEvent(TAG_GameEventTest_Alpha, Doomed, FMassEntityHandle());
		Drain();

		ASSERT_THAT(AreEqual(1, ControlCount, TEXT("the event itself must have been drained")));
		ASSERT_THAT(AreEqual(0, DoomedCount, TEXT("a destroyed entity's subscription must have been evicted")));
	}

	TEST_METHOD(DestroyedEntity_TouchedSubscription_EvictedAndLiveEntityUnaffected)
	{
		const FMassEntityHandle Doomed = CreateEntity();
		const FMassEntityHandle Survivor = CreateEntity();

		int32 DoomedCount = 0;
		int32 SurvivorCount = 0;
		Bus->Subscribe(TAG_GameEventTest_Alpha, Doomed, EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(DoomedCount));
		Bus->Subscribe(TAG_GameEventTest_Alpha, Survivor, EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(SurvivorCount));

		QueueEvent(TAG_GameEventTest_Alpha, Doomed, Survivor);
		Drain();
		ASSERT_THAT(AreEqual(1, DoomedCount));
		ASSERT_THAT(AreEqual(1, SurvivorCount));

		EntityManager->DestroyEntity(Doomed);

		QueueEvent(TAG_GameEventTest_Alpha, Doomed, Survivor);
		Drain();
		const int32 DoomedCountAfterDestroyDrain = DoomedCount;
		ASSERT_THAT(AreEqual(2, SurvivorCount, TEXT("a live entity's subscription must survive eviction of another")));

		QueueEvent(TAG_GameEventTest_Alpha, Doomed, Survivor);
		Drain();
		ASSERT_THAT(AreEqual(DoomedCountAfterDestroyDrain, DoomedCount, TEXT("no callback after the destroyed entity was evicted")));
		ASSERT_THAT(AreEqual(3, SurvivorCount));
	}

	TEST_METHOD(UnsubscribeStaleHandleAfterEviction_IsSafe)
	{
		const FMassEntityHandle Doomed = CreateEntity();

		int32 ControlCount = 0;
		const FDelegateHandle DoomedHandle = Bus->Subscribe(TAG_GameEventTest_Alpha, Doomed, EArcEventRole::Any,
			FArcGameEventDelegate::CreateLambda([](const FArcGameEvent& /*Event*/) {}));
		Bus->Subscribe(TAG_GameEventTest_Alpha, FMassEntityHandle(), EArcEventRole::Any,
			ArcGameEventTestsPrivate::MakeCounter(ControlCount));

		EntityManager->DestroyEntity(Doomed);
		Drain();

		Bus->Unsubscribe(DoomedHandle);

		QueueEvent(TAG_GameEventTest_Alpha, FMassEntityHandle(), FMassEntityHandle());
		Drain();
		ASSERT_THAT(AreEqual(1, ControlCount, TEXT("the bus must keep working after unsubscribing an evicted handle")));
	}
};

// ============================================================================
// ArcGameEvent_EffectLifecycle
// ============================================================================

TEST_CLASS(ArcGameEvent_EffectLifecycle, "ArcMassAbilities.GameEventBus.EffectLifecycle")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UArcGameEventSubsystem* Bus = nullptr;
	UArcEffectDurationProcessor* DurationProcessor = nullptr;
	TArray<FArcGameEvent> Events;
	FMassEntityHandle Source;
	FMassEntityHandle Target;

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
		Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(Bus);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		DurationProcessor = NewObject<UArcEffectDurationProcessor>();
		DurationProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		Source = ArcGameEventTestsPrivate::CreateEffectEntity(*EntityManager);
		Target = ArcGameEventTestsPrivate::CreateEffectEntity(*EntityManager);

		const FGameplayTag LifecycleTags[4] = {
			ArcGameEventTags::EffectApplied.GetTag(), ArcGameEventTags::EffectExecuted.GetTag(),
			ArcGameEventTags::EffectRemoved.GetTag(), ArcGameEventTags::EffectRejected.GetTag()
		};
		for (const FGameplayTag& LifecycleTag : LifecycleTags)
		{
			Bus->Subscribe(LifecycleTag, FMassEntityHandle(), EArcEventRole::Any, ArcGameEventTestsPrivate::MakeRecorder(Events));
		}
	}

	void Drain()
	{
		Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
	}

	void TickDuration(float DeltaTime)
	{
		FMassProcessingContext ProcessingContext(*EntityManager, DeltaTime);
		UE::Mass::Executor::Run(*DurationProcessor, ProcessingContext);
	}

	FArcEffectApplyResult Apply(UArcEffectDefinition* Effect)
	{
		return ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, Source);
	}

	int32 CountOf(const FNativeGameplayTag& Tag) const
	{
		return ArcGameEventTestsPrivate::CountWithTag(Events, Tag.GetTag());
	}

	const FArcGameEvent* EventOf(const FNativeGameplayTag& Tag, int32 Occurrence) const
	{
		return ArcGameEventTestsPrivate::FindEvent(Events, Tag.GetTag(), Occurrence);
	}

	TEST_METHOD(Applied_InstantEffect_CarriesSourceTargetDefinitionAndStackCount)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeInstantExecutionEffect();

		const FArcEffectApplyResult Result = Apply(Effect);
		ASSERT_THAT(IsTrue(Result.bApplied));
		ASSERT_THAT(AreEqual(0, Events.Num(), TEXT("nothing may be delivered before the drain")));

		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectApplied)));
		const FArcGameEvent* Applied = EventOf(ArcGameEventTags::EffectApplied, 0);
		ASSERT_THAT(IsNotNull(Applied));
		ASSERT_THAT(IsTrue(Applied->Instigator == Source, TEXT("Instigator carries the effect source")));
		ASSERT_THAT(IsTrue(Applied->Target == Target, TEXT("Target carries the receiving entity")));

		const FArcEffectAppliedPayload* Payload = Applied->Payload.GetPtr<FArcEffectAppliedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(ArcGameEventTestsPrivate::SpecDefinition(Payload->Spec) == Effect));
		ASSERT_THAT(AreEqual(1, Payload->StackCount));
		ASSERT_THAT(IsFalse(Payload->Handle.IsValid(), TEXT("an instant effect has no active handle")));
		ASSERT_THAT(AreEqual(0, CountOf(ArcGameEventTags::EffectRejected)));
	}

	TEST_METHOD(Applied_DurationEffect_CarriesSourceHandleAndStackCountOne)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeModifierOnlyDurationEffect(3);

		const FArcEffectApplyResult Result = Apply(Effect);
		ASSERT_THAT(IsTrue(Result.Handle.IsValid()));

		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectApplied)));
		const FArcGameEvent* Applied = EventOf(ArcGameEventTags::EffectApplied, 0);
		ASSERT_THAT(IsNotNull(Applied));
		ASSERT_THAT(IsTrue(Applied->Instigator == Source));
		ASSERT_THAT(IsTrue(Applied->Target == Target));

		const FArcEffectAppliedPayload* Payload = Applied->Payload.GetPtr<FArcEffectAppliedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Handle == Result.Handle, TEXT("payload handle must be the stored effect's handle")));
		ASSERT_THAT(IsTrue(ArcGameEventTestsPrivate::SpecDefinition(Payload->Spec) == Effect));
		ASSERT_THAT(AreEqual(1, Payload->StackCount));
	}

	TEST_METHOD(Applied_StackIncrementAndRefreshAtMax_CarryStackCountAndSameHandle)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeModifierOnlyDurationEffect(2);

		const FArcEffectApplyResult First = Apply(Effect);
		const FArcEffectApplyResult Second = Apply(Effect);
		const FArcEffectApplyResult Third = Apply(Effect);
		ASSERT_THAT(IsTrue(First.Handle.IsValid()));
		ASSERT_THAT(IsTrue(Second.Handle == First.Handle));
		ASSERT_THAT(IsTrue(Third.Handle == First.Handle));

		Drain();

		ASSERT_THAT(AreEqual(3, CountOf(ArcGameEventTags::EffectApplied), TEXT("first apply, increment and refresh at max each publish Applied")));

		const int32 ExpectedStackCounts[3] = {1, 2, 2};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FArcGameEvent* Applied = EventOf(ArcGameEventTags::EffectApplied, Index);
			ASSERT_THAT(IsNotNull(Applied));
			ASSERT_THAT(IsTrue(Applied->Instigator == Source));
			ASSERT_THAT(IsTrue(Applied->Target == Target));

			const FArcEffectAppliedPayload* Payload = Applied->Payload.GetPtr<FArcEffectAppliedPayload>();
			ASSERT_THAT(IsNotNull(Payload));
			ASSERT_THAT(AreEqual(ExpectedStackCounts[Index], Payload->StackCount));
			ASSERT_THAT(IsTrue(Payload->Handle == First.Handle));
		}
	}

	TEST_METHOD(Applied_RoleSubscriptions_TargetRoleOnReceiverSeesSourceAsInstigator)
	{
		int32 TargetRoleOnTarget = 0;
		int32 InstigatorRoleOnSource = 0;
		int32 TargetRoleOnSource = 0;
		int32 InstigatorRoleOnTarget = 0;
		FMassEntityHandle ObservedSource;

		Bus->Subscribe(ArcGameEventTags::EffectApplied.GetTag(), Target, EArcEventRole::Target,
			FArcGameEventDelegate::CreateLambda([&TargetRoleOnTarget, &ObservedSource](const FArcGameEvent& Event)
			{
				++TargetRoleOnTarget;
				ObservedSource = Event.Instigator;
			}));
		Bus->Subscribe(ArcGameEventTags::EffectApplied.GetTag(), Source, EArcEventRole::Instigator,
			ArcGameEventTestsPrivate::MakeCounter(InstigatorRoleOnSource));
		Bus->Subscribe(ArcGameEventTags::EffectApplied.GetTag(), Source, EArcEventRole::Target,
			ArcGameEventTestsPrivate::MakeCounter(TargetRoleOnSource));
		Bus->Subscribe(ArcGameEventTags::EffectApplied.GetTag(), Target, EArcEventRole::Instigator,
			ArcGameEventTestsPrivate::MakeCounter(InstigatorRoleOnTarget));

		ASSERT_THAT(IsTrue(Apply(ArcGameEventTestsPrivate::MakeInstantExecutionEffect()).bApplied));
		Drain();

		ASSERT_THAT(AreEqual(1, TargetRoleOnTarget));
		ASSERT_THAT(IsTrue(ObservedSource == Source, TEXT("the SourceEntity a Target-role subscriber reads is Event.Instigator")));
		ASSERT_THAT(AreEqual(1, InstigatorRoleOnSource));
		ASSERT_THAT(AreEqual(0, TargetRoleOnSource));
		ASSERT_THAT(AreEqual(0, InstigatorRoleOnTarget));
	}

	TEST_METHOD(Executed_InstantEffect_FiresOnceWithSourceAndTarget)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeInstantExecutionEffect();

		ASSERT_THAT(IsTrue(Apply(Effect).bApplied));
		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectExecuted)));
		const FArcGameEvent* Executed = EventOf(ArcGameEventTags::EffectExecuted, 0);
		ASSERT_THAT(IsNotNull(Executed));
		ASSERT_THAT(IsTrue(Executed->Instigator == Source));
		ASSERT_THAT(IsTrue(Executed->Target == Target));

		const FArcEffectExecutedPayload* Payload = Executed->Payload.GetPtr<FArcEffectExecutedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(ArcGameEventTestsPrivate::SpecDefinition(Payload->Spec) == Effect));
	}

	TEST_METHOD(Executed_ModifierOnlyDurationEffect_DoesNotFire)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeModifierOnlyDurationEffect(1);

		ASSERT_THAT(IsTrue(Apply(Effect).Handle.IsValid()));
		TickDuration(1.f);
		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectApplied)));
		ASSERT_THAT(AreEqual(0, CountOf(ArcGameEventTags::EffectExecuted), TEXT("a duration effect with no executions never executes")));
	}

	TEST_METHOD(Executed_PeriodicPeriodOnly_NotAtApplyThenOncePerTick)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeStoredPeriodicEffect(EArcPeriodicExecutionPolicy::PeriodOnly);
		const FArcEffectApplyResult Result = Apply(Effect);
		ASSERT_THAT(IsTrue(Result.Handle.IsValid()));

		Drain();
		ASSERT_THAT(AreEqual(0, CountOf(ArcGameEventTags::EffectExecuted), TEXT("PeriodOnly must not execute at application")));

		TickDuration(2.1f);
		Drain();
		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectExecuted)));

		const FArcGameEvent* Executed = EventOf(ArcGameEventTags::EffectExecuted, 0);
		ASSERT_THAT(IsNotNull(Executed));
		ASSERT_THAT(IsTrue(Executed->Instigator == Source));
		ASSERT_THAT(IsTrue(Executed->Target == Target));
		const FArcEffectExecutedPayload* Payload = Executed->Payload.GetPtr<FArcEffectExecutedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Handle == Result.Handle));
		ASSERT_THAT(IsTrue(ArcGameEventTestsPrivate::SpecDefinition(Payload->Spec) == Effect));

		TickDuration(2.f);
		Drain();
		ASSERT_THAT(AreEqual(2, CountOf(ArcGameEventTags::EffectExecuted)));
	}

	TEST_METHOD(Executed_PeriodicPeriodAndApplication_FiresAtApplyAndOnEachTick)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeStoredPeriodicEffect(EArcPeriodicExecutionPolicy::PeriodAndApplication);
		ASSERT_THAT(IsTrue(Apply(Effect).Handle.IsValid()));

		Drain();
		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectExecuted), TEXT("PeriodAndApplication executes at application")));

		TickDuration(2.1f);
		Drain();
		ASSERT_THAT(AreEqual(2, CountOf(ArcGameEventTags::EffectExecuted)));
	}

	TEST_METHOD(Removed_ExplicitRemoval_CarriesRemovedReasonHandleDefinitionAndSource)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeInfiniteEffect();
		const FArcEffectApplyResult Result = Apply(Effect);
		ASSERT_THAT(IsTrue(Result.Handle.IsValid()));

		FArcEffectRemovalQuery Query;
		Query.Definition = Effect;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*EntityManager, Target, Query)));

		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectRemoved)));
		const FArcGameEvent* Removed = EventOf(ArcGameEventTags::EffectRemoved, 0);
		ASSERT_THAT(IsNotNull(Removed));
		ASSERT_THAT(IsTrue(Removed->Instigator == Source, TEXT("Instigator carries the original effect source")));
		ASSERT_THAT(IsTrue(Removed->Target == Target));

		const FArcEffectRemovedPayload* Payload = Removed->Payload.GetPtr<FArcEffectRemovedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcEffectRemovalReason::Removed));
		ASSERT_THAT(IsTrue(Payload->Handle == Result.Handle));
		ASSERT_THAT(IsTrue(ArcGameEventTestsPrivate::SpecDefinition(Payload->Spec) == Effect));
	}

	TEST_METHOD(Removed_QueryReasonIsForwardedToEvent)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeInfiniteEffect();
		ASSERT_THAT(IsTrue(Apply(Effect).Handle.IsValid()));

		FArcEffectRemovalQuery Query;
		Query.Definition = Effect;
		Query.Reason = EArcEffectRemovalReason::EntityDestroyed;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*EntityManager, Target, Query)));

		Drain();

		const FArcGameEvent* Removed = EventOf(ArcGameEventTags::EffectRemoved, 0);
		ASSERT_THAT(IsNotNull(Removed));
		const FArcEffectRemovedPayload* Payload = Removed->Payload.GetPtr<FArcEffectRemovedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcEffectRemovalReason::EntityDestroyed));
	}

	TEST_METHOD(Removed_Expiry_CarriesExpiredReason)
	{
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 1.f;

		const FArcEffectApplyResult Result = Apply(Effect);
		ASSERT_THAT(IsTrue(Result.Handle.IsValid()));

		Drain();
		ASSERT_THAT(AreEqual(0, CountOf(ArcGameEventTags::EffectRemoved)));

		TickDuration(2.f);
		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectRemoved)));
		const FArcGameEvent* Removed = EventOf(ArcGameEventTags::EffectRemoved, 0);
		ASSERT_THAT(IsNotNull(Removed));
		ASSERT_THAT(IsTrue(Removed->Instigator == Source));
		ASSERT_THAT(IsTrue(Removed->Target == Target));
		const FArcEffectRemovedPayload* Payload = Removed->Payload.GetPtr<FArcEffectRemovedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcEffectRemovalReason::Expired));
		ASSERT_THAT(IsTrue(Payload->Handle == Result.Handle));
	}

	TEST_METHOD(Removed_CounterReplace_CarriesReplacedReasonForOldHandle)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeInfiniteEffect();
		Effect->StackingPolicy.StackType = EArcStackType::Counter;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Replace;
		Effect->StackingPolicy.MaxStacks = 1;

		const FArcEffectApplyResult First = Apply(Effect);
		const FArcEffectApplyResult Second = Apply(Effect);
		ASSERT_THAT(IsTrue(First.Handle.IsValid()));
		ASSERT_THAT(IsTrue(Second.Handle.IsValid()));
		ASSERT_THAT(IsTrue(First.Handle != Second.Handle));

		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectRemoved)));
		const FArcGameEvent* Removed = EventOf(ArcGameEventTags::EffectRemoved, 0);
		ASSERT_THAT(IsNotNull(Removed));
		const FArcEffectRemovedPayload* Payload = Removed->Payload.GetPtr<FArcEffectRemovedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcEffectRemovalReason::Replaced));
		ASSERT_THAT(IsTrue(Payload->Handle == First.Handle, TEXT("the replaced effect is the first one")));
		ASSERT_THAT(AreEqual(2, CountOf(ArcGameEventTags::EffectApplied)));
	}

	TEST_METHOD(Removed_InstanceOverflowRemoveOldest_CarriesOverflowReasonForOldestHandle)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeInfiniteEffect();
		Effect->StackingPolicy.StackType = EArcStackType::Instance;
		Effect->StackingPolicy.OverflowPolicy = EArcStackOverflowPolicy::RemoveOldest;
		Effect->StackingPolicy.MaxStacks = 1;

		const FArcEffectApplyResult First = Apply(Effect);
		const FArcEffectApplyResult Second = Apply(Effect);
		ASSERT_THAT(IsTrue(First.Handle.IsValid()));
		ASSERT_THAT(IsTrue(Second.Handle.IsValid()));

		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectRemoved)));
		const FArcGameEvent* Removed = EventOf(ArcGameEventTags::EffectRemoved, 0);
		ASSERT_THAT(IsNotNull(Removed));
		const FArcEffectRemovedPayload* Payload = Removed->Payload.GetPtr<FArcEffectRemovedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcEffectRemovalReason::Overflow));
		ASSERT_THAT(IsTrue(Payload->Handle == First.Handle));
	}

	TEST_METHOD(Rejected_Immunity_CarriesImmunityReasonDefinitionAndSource)
	{
		UArcEffectDefinition* ImmunityGrant = ArcGameEventTestsPrivate::MakeInfiniteEffect();
		FArcEffectComp_GrantImmunity GrantComponent;
		GrantComponent.GrantedImmunityTags.AddTag(TAG_GameEventTest_FireImmunity);
		ImmunityGrant->Components.Add(FInstancedStruct::Make(GrantComponent));
		ASSERT_THAT(IsTrue(Apply(ImmunityGrant).Handle.IsValid()));

		UArcEffectDefinition* Blocked = ArcGameEventTestsPrivate::MakeInstantExecutionEffect();
		FArcEffectComp_EffectTags IdentityComponent;
		IdentityComponent.OwnedTags.AddTag(TAG_GameEventTest_FireImmunity);
		Blocked->Components.Add(FInstancedStruct::Make(IdentityComponent));

		ASSERT_THAT(IsFalse(Apply(Blocked).bApplied));
		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectRejected)));
		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectApplied), TEXT("only the immunity grant was applied")));
		ASSERT_THAT(AreEqual(0, CountOf(ArcGameEventTags::EffectExecuted)));

		const FArcGameEvent* Rejected = EventOf(ArcGameEventTags::EffectRejected, 0);
		ASSERT_THAT(IsNotNull(Rejected));
		ASSERT_THAT(IsTrue(Rejected->Instigator == Source));
		ASSERT_THAT(IsTrue(Rejected->Target == Target));
		const FArcEffectRejectedPayload* Payload = Rejected->Payload.GetPtr<FArcEffectRejectedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == ArcGameEventTags::EffectRejectedImmunity.GetTag()));
		ASSERT_THAT(IsTrue(ArcGameEventTestsPrivate::SpecDefinition(Payload->Spec) == Blocked));
	}

	TEST_METHOD(Rejected_GateVeto_CarriesGateReason)
	{
		UArcEffectDefinition* Gated = ArcGameEventTestsPrivate::MakeInstantExecutionEffect();
		FArcEffectComp_TagFilter FilterComponent;
		FilterComponent.RequirementFilter.RequireTags.AddTag(TAG_GameEventTest_RequiredState);
		Gated->Components.Add(FInstancedStruct::Make(FilterComponent));

		ASSERT_THAT(IsFalse(Apply(Gated).bApplied));
		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectRejected)));
		ASSERT_THAT(AreEqual(0, CountOf(ArcGameEventTags::EffectApplied)));

		const FArcGameEvent* Rejected = EventOf(ArcGameEventTags::EffectRejected, 0);
		ASSERT_THAT(IsNotNull(Rejected));
		ASSERT_THAT(IsTrue(Rejected->Instigator == Source));
		ASSERT_THAT(IsTrue(Rejected->Target == Target));
		const FArcEffectRejectedPayload* Payload = Rejected->Payload.GetPtr<FArcEffectRejectedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == ArcGameEventTags::EffectRejectedGate.GetTag()));
		ASSERT_THAT(IsTrue(ArcGameEventTestsPrivate::SpecDefinition(Payload->Spec) == Gated));
	}

	TEST_METHOD(Rejected_ChanceZero_CarriesChanceReason)
	{
		UArcEffectDefinition* Chancy = ArcGameEventTestsPrivate::MakeInstantExecutionEffect();
		FArcEffectComp_ChanceToApply ChanceComponent;
		ChanceComponent.Chance = 0.f;
		Chancy->Components.Add(FInstancedStruct::Make(ChanceComponent));

		ASSERT_THAT(IsFalse(Apply(Chancy).bApplied));
		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectRejected)));
		const FArcGameEvent* Rejected = EventOf(ArcGameEventTags::EffectRejected, 0);
		ASSERT_THAT(IsNotNull(Rejected));
		const FArcEffectRejectedPayload* Payload = Rejected->Payload.GetPtr<FArcEffectRejectedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == ArcGameEventTags::EffectRejectedChance.GetTag()));
	}

	TEST_METHOD(Rejected_DenyNew_CarriesDenyNewReason)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeInfiniteEffect();
		Effect->StackingPolicy.StackType = EArcStackType::Counter;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::DenyNew;
		Effect->StackingPolicy.MaxStacks = 1;

		ASSERT_THAT(IsTrue(Apply(Effect).Handle.IsValid()));
		const FArcEffectApplyResult Denied = Apply(Effect);
		ASSERT_THAT(IsFalse(Denied.bApplied));

		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectRejected)));
		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectApplied), TEXT("the denied application publishes no Applied")));

		const FArcGameEvent* Rejected = EventOf(ArcGameEventTags::EffectRejected, 0);
		ASSERT_THAT(IsNotNull(Rejected));
		ASSERT_THAT(IsTrue(Rejected->Instigator == Source));
		ASSERT_THAT(IsTrue(Rejected->Target == Target));
		const FArcEffectRejectedPayload* Payload = Rejected->Payload.GetPtr<FArcEffectRejectedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == ArcGameEventTags::EffectRejectedDenyNew.GetTag()));
		ASSERT_THAT(IsTrue(ArcGameEventTestsPrivate::SpecDefinition(Payload->Spec) == Effect));
	}

	TEST_METHOD(Rejected_InstanceOverflowDeny_CarriesOverflowReason)
	{
		UArcEffectDefinition* Effect = ArcGameEventTestsPrivate::MakeInfiniteEffect();
		Effect->StackingPolicy.StackType = EArcStackType::Instance;
		Effect->StackingPolicy.OverflowPolicy = EArcStackOverflowPolicy::Deny;
		Effect->StackingPolicy.MaxStacks = 1;

		ASSERT_THAT(IsTrue(Apply(Effect).Handle.IsValid()));
		ASSERT_THAT(IsFalse(Apply(Effect).bApplied));

		Drain();

		ASSERT_THAT(AreEqual(1, CountOf(ArcGameEventTags::EffectRejected)));
		const FArcGameEvent* Rejected = EventOf(ArcGameEventTags::EffectRejected, 0);
		ASSERT_THAT(IsNotNull(Rejected));
		const FArcEffectRejectedPayload* Payload = Rejected->Payload.GetPtr<FArcEffectRejectedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == ArcGameEventTags::EffectRejectedOverflow.GetTag()));
	}
};

// ============================================================================
// ArcGameEvent_AbilityEnded
// ============================================================================

TEST_CLASS(ArcGameEvent_AbilityEnded, "ArcMassAbilities.GameEventBus.AbilityEnded")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcAbilityStateTreeSubsystem* AbilitySubsystem = nullptr;
	UArcGameEventSubsystem* Bus = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogStateTree");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogMass");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
		AbilitySubsystem = Spawner.GetWorld().GetSubsystem<UArcAbilityStateTreeSubsystem>();
		check(AbilitySubsystem);
		Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(Bus);
	}

	void Drain()
	{
		Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
	}

	void ExecuteInputProcessor(FMassEntityHandle Entity)
	{
		UArcAbilityInputProcessor* Processor = NewObject<UArcAbilityInputProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, 0.016f);
		FMassExecutionContext& Context = ProcessingContext.GetExecutionContext();
		Processor->CallExecute(*EntityManager, Context);
	}

	UArcAbilityDefinition* MakeLongRunningAbility()
	{
		UArcAbilityDefinition* Def = NewObject<UArcAbilityDefinition>();
		Def->StateTree = ArcGameEventTestsPrivate::CreateLongRunningStateTree(&Spawner.GetWorld());
		return Def;
	}

	bool IsAbilityActive(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment* Collection = View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
		return Collection != nullptr && Collection->Abilities.Num() > 0 && Collection->Abilities[0].bIsActive;
	}

	TEST_METHOD(CancelAbility_Interrupted_PublishesEndedWithInstigatorTargetAndPayload)
	{
		const FMassEntityHandle Owner = ArcGameEventTestsPrivate::CreateAbilityEntity(*EntityManager);
		const FMassEntityHandle Interrupter = ArcGameEventTestsPrivate::CreateEffectEntity(*EntityManager);
		UArcAbilityDefinition* Def = MakeLongRunningAbility();

		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Owner, Def, TAG_GameEventTest_InputFire);
		ArcAbilities::PushInput(*EntityManager, Owner, TAG_GameEventTest_InputFire);
		ExecuteInputProcessor(Owner);
		ASSERT_THAT(IsTrue(IsAbilityActive(Owner), TEXT("ability must be running before the cancel")));
		Drain();

		TArray<FArcGameEvent> GlobalEvents;
		int32 OwnerTargetRole = 0;
		int32 InterrupterInstigatorRole = 0;
		int32 InterrupterTargetRole = 0;
		const FGameplayTag EndedTag = ArcGameEventTags::AbilityEnded.GetTag();
		Bus->Subscribe(EndedTag, FMassEntityHandle(), EArcEventRole::Any, ArcGameEventTestsPrivate::MakeRecorder(GlobalEvents));
		Bus->Subscribe(EndedTag, Owner, EArcEventRole::Target, ArcGameEventTestsPrivate::MakeCounter(OwnerTargetRole));
		Bus->Subscribe(EndedTag, Interrupter, EArcEventRole::Instigator, ArcGameEventTestsPrivate::MakeCounter(InterrupterInstigatorRole));
		Bus->Subscribe(EndedTag, Interrupter, EArcEventRole::Target, ArcGameEventTestsPrivate::MakeCounter(InterrupterTargetRole));

		FArcTestExitFlagTask::ExitStateCount = 0;
		ArcAbilities::CancelAbility(*EntityManager, Owner, Handle, *AbilitySubsystem, EArcAbilityEndReason::Interrupted, Interrupter);

		ASSERT_THAT(AreEqual(0, GlobalEvents.Num(), TEXT("nothing may be delivered before the drain")));
		Drain();

		ASSERT_THAT(AreEqual(1, GlobalEvents.Num()));
		const FArcGameEvent& Ended = GlobalEvents[0];
		ASSERT_THAT(IsTrue(Ended.Tag == EndedTag));
		ASSERT_THAT(IsTrue(Ended.Instigator == Interrupter, TEXT("Instigator is the interrupting entity")));
		ASSERT_THAT(IsTrue(Ended.Target == Owner, TEXT("Target is the ability owner")));

		const FArcAbilityEndedPayload* Payload = Ended.Payload.GetPtr<FArcAbilityEndedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcAbilityEndReason::Interrupted));
		ASSERT_THAT(IsTrue(Payload->Handle == Handle));
		ASSERT_THAT(IsTrue(Payload->Definition.Get() == Def));

		ASSERT_THAT(AreEqual(1, OwnerTargetRole));
		ASSERT_THAT(AreEqual(1, InterrupterInstigatorRole));
		ASSERT_THAT(AreEqual(0, InterrupterTargetRole));
	}

	TEST_METHOD(CancelAbility_CancelledWithoutInstigator_PublishesEndedWithTheOwnerAsInstigator)
	{
		const FMassEntityHandle Owner = ArcGameEventTestsPrivate::CreateAbilityEntity(*EntityManager);
		UArcAbilityDefinition* Def = MakeLongRunningAbility();

		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Owner, Def, TAG_GameEventTest_InputFire);
		ArcAbilities::PushInput(*EntityManager, Owner, TAG_GameEventTest_InputFire);
		ExecuteInputProcessor(Owner);
		ASSERT_THAT(IsTrue(IsAbilityActive(Owner)));
		Drain();

		TArray<FArcGameEvent> GlobalEvents;
		int32 OwnerTargetRole = 0;
		int32 OwnerInstigatorRole = 0;
		const FGameplayTag EndedTag = ArcGameEventTags::AbilityEnded.GetTag();
		Bus->Subscribe(EndedTag, FMassEntityHandle(), EArcEventRole::Any, ArcGameEventTestsPrivate::MakeRecorder(GlobalEvents));
		Bus->Subscribe(EndedTag, Owner, EArcEventRole::Target, ArcGameEventTestsPrivate::MakeCounter(OwnerTargetRole));
		Bus->Subscribe(EndedTag, Owner, EArcEventRole::Instigator, ArcGameEventTestsPrivate::MakeCounter(OwnerInstigatorRole));

		ArcAbilities::CancelAbility(*EntityManager, Owner, Handle, *AbilitySubsystem, EArcAbilityEndReason::Cancelled, FMassEntityHandle());
		Drain();

		ASSERT_THAT(AreEqual(1, GlobalEvents.Num()));
		ASSERT_THAT(IsTrue(GlobalEvents[0].Instigator == Owner, TEXT("with no instigator supplied the owner is the instigator")));
		ASSERT_THAT(IsTrue(GlobalEvents[0].Target == Owner));

		const FArcAbilityEndedPayload* Payload = GlobalEvents[0].Payload.GetPtr<FArcAbilityEndedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == EArcAbilityEndReason::Cancelled));
		ASSERT_THAT(IsTrue(Payload->Handle == Handle));

		ASSERT_THAT(AreEqual(1, OwnerTargetRole));
		ASSERT_THAT(AreEqual(1, OwnerInstigatorRole));
	}
};
