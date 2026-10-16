// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Events/ArcAttributeEventPayloads.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TaskTest_Watched, "Test.EffectTask.Watched");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TaskTest_Other, "Test.EffectTask.Other");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TaskTest_Input, "Test.EffectTask.Input.A");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TaskTest_InputWanted, "Test.EffectTask.Input.B");

namespace ArcEffectTaskTestsPrivate
{
	void QueueAttributeChanged(UArcGameEventSubsystem& Bus, FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float OldValue, float NewValue)
	{
		FArcAttributeChangedPayload Payload;
		Payload.Attribute = Attribute;
		Payload.OldValue = OldValue;
		Payload.NewValue = NewValue;

		FArcGameEvent Event;
		Event.Tag = ArcGameEventTags::AttributeChanged.GetTag();
		Event.Instigator = Entity;
		Event.Target = Entity;
		Event.Magnitude = NewValue - OldValue;
		Event.Payload = FInstancedStruct::Make(Payload);
		Bus.Queue(MoveTemp(Event));
	}
}

// ============================================================================
// ArcEffectTask_Lifecycle
// ============================================================================

TEST_CLASS(ArcEffectTask_Lifecycle, "ArcMassAbilities.Effects.Tasks.Lifecycle")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

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
	}

	TEST_METHOD(Tasks_CopiedToActiveEffect_OnApply)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Effect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(IsTrue(Stack != nullptr));
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects[0].Tasks.Num()));
	}

	TEST_METHOD(Tasks_RemovedWhenEffectRemoved)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Effect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		FArcEffectRemovalQuery RemoveQuery;
		RemoveQuery.Definition = Effect;
		ArcEffects::RemoveEffects(*EntityManager, Entity, RemoveQuery);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(0, Stack->ActiveEffects.Num()));
	}

	TEST_METHOD(MultipleTasks_AllCopied)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcEffectTask_OnAttributeChanged Task1;
		Task1.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Effect->Tasks.Add(FInstancedStruct::Make(Task1));

		FArcEffectTask_OnAttributeChanged Task2;
		Task2.Attribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		Effect->Tasks.Add(FInstancedStruct::Make(Task2));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(2, Stack->ActiveEffects[0].Tasks.Num()));
	}

	TEST_METHOD(NoEffect_WhenInstant_NoTasks)
	{
		// Instant effects are not stored as active effects — tasks should not be created.
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Effect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(0, Stack->ActiveEffects.Num()));
	}
};

// ============================================================================
// ArcEffectTask_OnAttributeChanged
//
// NOTE: These tests verify that OnAttributeChanged is wired to the game bus.
// Most publish the attribute event by hand on ArcEvent.Attribute.Changed; the
// RealWrite tests drive a real write through the pending-ops processor, which
// publishes synchronously. Delivery to the task is one bus drain.
// ============================================================================

TEST_CLASS(ArcEffectTask_AttributeChanged, "ArcMassAbilities.Effects.Tasks.OnAttributeChanged")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
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

		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
		Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(Bus);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void Drain()
	{
		Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	TEST_METHOD(ReactsToAttributeChange_AppliesEffect)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		// Reaction effect: adds 10 armor
		UArcEffectDefinition* ReactionEffect = NewObject<UArcEffectDefinition>();
		ReactionEffect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution ArmorExec;
		ArmorExec.TargetAttribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		ArmorExec.Value = 10.f;
		ReactionEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ArmorExec)));

		// Buff with task: on health changed, apply reaction
		UArcEffectDefinition* BuffEffect = NewObject<UArcEffectDefinition>();
		BuffEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		TaskDef.EffectToApply = ReactionEffect;
		BuffEffect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		// Apply the buff (this wires the delegate via OnApplication)
		ArcEffects::TryApplyEffect(*EntityManager, Entity, BuffEffect, FMassEntityHandle());

		// Publish the attribute changed event by hand (simulates what
		// ArcAttributeAggregatorProcessor does after processing) and deliver it with one drain.
		ArcEffectTaskTestsPrivate::QueueAttributeChanged(*Bus, Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), 100.f, 80.f);
		Drain();
		DrainPendingOps(Entity);

		// Verify: armor should be 10 (reaction fired)
		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(10.f, Stats->Armor.BaseValue, 0.001f));
	}

	TEST_METHOD(DoesNotReactAfterEffectRemoved)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		// Reaction effect: adds 10 armor
		UArcEffectDefinition* ReactionEffect = NewObject<UArcEffectDefinition>();
		ReactionEffect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution ArmorExec;
		ArmorExec.TargetAttribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		ArmorExec.Value = 10.f;
		ReactionEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ArmorExec)));

		// Buff with task
		UArcEffectDefinition* BuffEffect = NewObject<UArcEffectDefinition>();
		BuffEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		TaskDef.EffectToApply = ReactionEffect;
		BuffEffect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		// Apply then remove the buff (OnRemoved should unsubscribe the delegate)
		ArcEffects::TryApplyEffect(*EntityManager, Entity, BuffEffect, FMassEntityHandle());
		FArcEffectRemovalQuery RemoveQuery;
		RemoveQuery.Definition = BuffEffect;
		ArcEffects::RemoveEffects(*EntityManager, Entity, RemoveQuery);

		// Publish + drain attribute changed — the subscription should already be gone
		ArcEffectTaskTestsPrivate::QueueAttributeChanged(*Bus, Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), 100.f, 80.f);
		Drain();

		// Verify: armor should still be 0 (reaction did NOT fire)
		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(0.f, Stats->Armor.BaseValue, 0.001f));
	}

	UArcEffectDefinition* MakeHealthWatcher()
	{
		UArcEffectDefinition* ReactionEffect = NewObject<UArcEffectDefinition>();
		ReactionEffect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution ArmorExec;
		ArmorExec.TargetAttribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		ArmorExec.Value = 10.f;
		ReactionEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ArmorExec)));

		UArcEffectDefinition* BuffEffect = NewObject<UArcEffectDefinition>();
		BuffEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		TaskDef.EffectToApply = ReactionEffect;
		BuffEffect->Tasks.Add(FInstancedStruct::Make(TaskDef));
		return BuffEffect;
	}

	void WriteAttribute(FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float Delta)
	{
		UArcEffectDefinition* Write = NewObject<UArcEffectDefinition>();
		Write->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution Exec;
		Exec.TargetAttribute = Attribute;
		Exec.Value = Delta;
		Write->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Write, FMassEntityHandle());
		DrainPendingOps(Entity);
	}

	TEST_METHOD(RealWrite_OfTheWatchedAttribute_ReachesTheTaskReaction)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		ArcEffects::TryApplyEffect(*EntityManager, Entity, MakeHealthWatcher(), FMassEntityHandle());

		WriteAttribute(Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), -10.f);
		Drain();
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(90.f, Stats->Health.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(10.f, Stats->Armor.BaseValue, 0.001f));
	}

	TEST_METHOD(RealWrite_OfAnotherAttribute_DoesNotReachTheTask)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		ArcEffects::TryApplyEffect(*EntityManager, Entity, MakeHealthWatcher(), FMassEntityHandle());

		WriteAttribute(Entity, FArcAttributeSet_TestStats::GetIntelligenceAttribute(), 5.f);
		Drain();
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(15.f, Stats->Intelligence.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(100.f, Stats->Health.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Stats->Armor.BaseValue, 0.001f));
	}

	TEST_METHOD(RemovedHost_WithTheEventAlreadyQueued_FiresNothingAndDoesNotCrash)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* ReactionEffect = NewObject<UArcEffectDefinition>();
		ReactionEffect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution ArmorExec;
		ArmorExec.TargetAttribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		ArmorExec.Value = 10.f;
		ReactionEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ArmorExec)));

		UArcEffectDefinition* BuffEffect = NewObject<UArcEffectDefinition>();
		BuffEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		TaskDef.EffectToApply = ReactionEffect;
		BuffEffect->Tasks.Add(FInstancedStruct::Make(TaskDef));
		ArcEffects::TryApplyEffect(*EntityManager, Entity, BuffEffect, FMassEntityHandle());

		ArcEffectTaskTestsPrivate::QueueAttributeChanged(*Bus, Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), 100.f, 80.f);
		FArcEffectRemovalQuery RemoveQuery;
		RemoveQuery.Definition = BuffEffect;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*EntityManager, Entity, RemoveQuery)));
		Drain();
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(0.f, Stats->Armor.BaseValue, 0.001f));
	}

	TEST_METHOD(NoEffectToApply_DoesNotCrash)
	{
		// Task with no EffectToApply should silently no-op on broadcast.
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* BuffEffect = NewObject<UArcEffectDefinition>();
		BuffEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		TaskDef.EffectToApply = nullptr;
		BuffEffect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, BuffEffect, FMassEntityHandle());

		// Should not crash
		ArcEffectTaskTestsPrivate::QueueAttributeChanged(*Bus, Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), 100.f, 80.f);
		Drain();

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(0.f, Stats->Armor.BaseValue, 0.001f));
	}
};

// ============================================================================
// ArcEventSubsystem_KeyedAttributeSubscription
//
// Tests Subscribe(Entity, Attribute) / Unsubscribe on ArcEvent.Attribute.Changed:
//   - Delegate fires only for the subscribed (entity, attribute) key
//   - Delegate does NOT fire for a different attribute on the same entity
//   - Delegate stops firing after explicit Unsubscribe
// ============================================================================

TEST_CLASS(ArcEventSubsystem_KeyedAttributeSubscription, "ArcMassAbilities.Events.KeyedAttributeSubscription")
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

	TEST_METHOD(DelegateFires_OnlyForSubscribedKey_AndUnbindPreventsSubsequentFire)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		FArcAttributeRef AttrA = FArcAttributeSet_TestStats::GetHealthAttribute();
		FArcAttributeRef AttrB = FArcAttributeSet_TestStats::GetArmorAttribute();

		int32 FireCount = 0;
		float ObservedNewValue = -1.f;

		FDelegateHandle Handle = Bus->Subscribe(Entity, AttrA,
			FArcGameEventDelegate::CreateLambda([&FireCount, &ObservedNewValue](const FArcGameEvent& Event)
			{
				++FireCount;
				const FArcAttributeChangedPayload* Payload = Event.Payload.GetPtr<FArcAttributeChangedPayload>();
				ObservedNewValue = Payload ? Payload->NewValue : -1.f;
			}));

		ArcEffectTaskTestsPrivate::QueueAttributeChanged(*Bus, Entity, AttrA, 0.f, 5.f);
		ArcEffectTaskTestsPrivate::QueueAttributeChanged(*Bus, Entity, AttrB, 0.f, 9.f);
		Drain();

		ASSERT_THAT(AreEqual(1, FireCount));
		ASSERT_THAT(IsNear(5.f, ObservedNewValue, 0.001f));

		Bus->Unsubscribe(Handle);

		ArcEffectTaskTestsPrivate::QueueAttributeChanged(*Bus, Entity, AttrA, 5.f, 7.f);
		Drain();

		ASSERT_THAT(AreEqual(1, FireCount));
	}
};

// ============================================================================
// ArcEffectTask_LambdaSafety
// ============================================================================

TEST_CLASS(ArcEffectTask_LambdaSafety, "ArcMassAbilities.Effects.Tasks.LambdaSafety")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
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

		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
		Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(Bus);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void Drain()
	{
		Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	TEST_METHOD(TaskDelegate_SurvivesArrayReallocation)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		// Reaction effect: adds 10 armor
		UArcEffectDefinition* ReactionEffect = NewObject<UArcEffectDefinition>();
		ReactionEffect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution ArmorExec;
		ArmorExec.TargetAttribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		ArmorExec.Value = 10.f;
		ReactionEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ArmorExec)));

		// Buff with OnAttributeChanged task
		UArcEffectDefinition* BuffEffect = NewObject<UArcEffectDefinition>();
		BuffEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		BuffEffect->StackingPolicy.StackType = EArcStackType::Instance;
		BuffEffect->StackingPolicy.MaxStacks = 100;
		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		TaskDef.EffectToApply = ReactionEffect;
		BuffEffect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		// Apply the buff first
		ArcEffects::TryApplyEffect(*EntityManager, Entity, BuffEffect, FMassEntityHandle());

		// Apply many more instances to force TArray reallocation
		UArcEffectDefinition* FillerEffect = NewObject<UArcEffectDefinition>();
		FillerEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FillerEffect->StackingPolicy.StackType = EArcStackType::Instance;
		FillerEffect->StackingPolicy.MaxStacks = 100;

		for (int32 i = 0; i < 32; ++i)
		{
			ArcEffects::TryApplyEffect(*EntityManager, Entity, FillerEffect, FMassEntityHandle());
		}

		// Publish + drain the attribute changed event — the original task must still be found and fire
		ArcEffectTaskTestsPrivate::QueueAttributeChanged(*Bus, Entity, FArcAttributeSet_TestStats::GetHealthAttribute(), 100.f, 80.f);
		Drain();
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(10.f, Stats->Armor.BaseValue, 0.001f));
	}
};

// ============================================================================
// ArcEffectTask_OnTagChanged: Added = owned count 0 -> >0, Removed = >0 -> 0 of RequiredTag on the
// Party (Target = host owner, Source = entity that applied the host), delivered by the bus drain after the change.
// ============================================================================

TEST_CLASS(ArcEffectTask_TagChanged, "ArcMassAbilities.Effects.Tasks.OnTagChanged")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
	UArcGameEventSubsystem* Bus = nullptr;
	FMassEntityHandle Entity;
	FMassEntityHandle SourceEntity;

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
		Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(Bus);

		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());
		Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		SourceEntity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
	}

	FArcActiveEffectHandle ApplyHost(EArcTagChangeTrigger When, EArcTagChangeParty Party = EArcTagChangeParty::Target,
		FMassEntityHandle AppliedBy = FMassEntityHandle())
	{
		UArcEffectDefinition* Reaction = NewObject<UArcEffectDefinition>();
		Reaction->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution ArmorExec;
		ArmorExec.TargetAttribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		ArmorExec.Value = 10.f;
		Reaction->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ArmorExec)));

		UArcEffectDefinition* Host = NewObject<UArcEffectDefinition>();
		Host->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnTagChanged Task;
		Task.Party = Party;
		Task.RequiredTag = TAG_TaskTest_Watched;
		Task.When = When;
		Task.EffectToApply = Reaction;
		Host->Tasks.Add(FInstancedStruct::Make(Task));
		return ArcEffects::TryApplyEffect(*EntityManager, Entity, Host, AppliedBy).Handle;
	}

	void AddTag(const FGameplayTag& Tag, FMassEntityHandle On)
	{
		ArcAbilities::AddOwnedTags(*EntityManager, On, FGameplayTagContainer(Tag));
	}

	void RemoveTag(const FGameplayTag& Tag, FMassEntityHandle On)
	{
		ArcAbilities::RemoveOwnedTags(*EntityManager, On, FGameplayTagContainer(Tag));
	}

	void AddTag(const FGameplayTag& Tag)
	{
		AddTag(Tag, Entity);
	}

	void RemoveTag(const FGameplayTag& Tag)
	{
		RemoveTag(Tag, Entity);
	}

	void Drain()
	{
		Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
	}

	float DrainAndReadArmor()
	{
		Drain();
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
		return ReadArmor();
	}

	int32 PendingOpCount() const
	{
		const FArcPendingAttributeOpsFragment* Pending = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcPendingAttributeOpsFragment>();
		return Pending ? Pending->PendingOps.Num() : -1;
	}

	float ReadArmor() const
	{
		const FArcAttributeSet_TestStats* Stats = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		return Stats ? Stats->Armor.BaseValue : -1.f;
	}

	TEST_METHOD(Added_FiresWhenTheTagIsGained_AfterTheDrain_NotOnAStackIncrease)
	{
		ASSERT_THAT(IsTrue(ApplyHost(EArcTagChangeTrigger::Added).IsValid()));

		const int32 OpsBeforeTheTag = PendingOpCount();
		ASSERT_THAT(IsTrue(OpsBeforeTheTag >= 0));
		AddTag(TAG_TaskTest_Watched);
		ASSERT_THAT(AreEqual(OpsBeforeTheTag, PendingOpCount()));
		ASSERT_THAT(IsNear(0.f, ReadArmor(), 0.001f));
		Drain();
		ASSERT_THAT(AreEqual(OpsBeforeTheTag + 1, PendingOpCount()));
		ASSERT_THAT(IsNear(10.f, DrainAndReadArmor(), 0.001f));

		AddTag(TAG_TaskTest_Watched);
		ASSERT_THAT(IsNear(10.f, DrainAndReadArmor(), 0.001f));

		RemoveTag(TAG_TaskTest_Watched);
		RemoveTag(TAG_TaskTest_Watched);
		ASSERT_THAT(IsNear(10.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(Removed_FiresOnlyWhenTheLastCountLeaves)
	{
		ASSERT_THAT(IsTrue(ApplyHost(EArcTagChangeTrigger::Removed).IsValid()));

		AddTag(TAG_TaskTest_Watched);
		AddTag(TAG_TaskTest_Watched);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));

		RemoveTag(TAG_TaskTest_Watched);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));

		RemoveTag(TAG_TaskTest_Watched);
		ASSERT_THAT(IsNear(10.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(Both_FiresOnGainAndOnLoss)
	{
		ASSERT_THAT(IsTrue(ApplyHost(EArcTagChangeTrigger::Both).IsValid()));

		AddTag(TAG_TaskTest_Watched);
		ASSERT_THAT(IsNear(10.f, DrainAndReadArmor(), 0.001f));

		RemoveTag(TAG_TaskTest_Watched);
		ASSERT_THAT(IsNear(20.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(UnwatchedTag_FiresNothing)
	{
		ASSERT_THAT(IsTrue(ApplyHost(EArcTagChangeTrigger::Both).IsValid()));

		AddTag(TAG_TaskTest_Other);
		RemoveTag(TAG_TaskTest_Other);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(RemovedHost_StopsReacting)
	{
		const FArcActiveEffectHandle Host = ApplyHost(EArcTagChangeTrigger::Added);
		ASSERT_THAT(IsTrue(Host.IsValid()));
		FArcEffectRemovalQuery Removal;
		Removal.Handle = Host;
		ArcEffects::RemoveEffects(*EntityManager, Entity, Removal);

		AddTag(TAG_TaskTest_Watched);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(RemovedHost_WithTheEventAlreadyQueued_FiresNothingAndDoesNotCrash)
	{
		const FArcActiveEffectHandle Host = ApplyHost(EArcTagChangeTrigger::Added);
		ASSERT_THAT(IsTrue(Host.IsValid()));

		AddTag(TAG_TaskTest_Watched);
		FArcEffectRemovalQuery Removal;
		Removal.Handle = Host;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*EntityManager, Entity, Removal)));

		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(PartyTarget_WatchesTheHostOwner_NotTheSource)
	{
		ASSERT_THAT(IsTrue(ApplyHost(EArcTagChangeTrigger::Added, EArcTagChangeParty::Target, SourceEntity).IsValid()));

		AddTag(TAG_TaskTest_Watched, SourceEntity);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));

		AddTag(TAG_TaskTest_Watched, Entity);
		ASSERT_THAT(IsNear(10.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(PartySource_TheSourcesTagChangeFiresTheEffectOnTheHostOwner)
	{
		ASSERT_THAT(IsTrue(ApplyHost(EArcTagChangeTrigger::Added, EArcTagChangeParty::Source, SourceEntity).IsValid()));

		AddTag(TAG_TaskTest_Watched, Entity);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));

		AddTag(TAG_TaskTest_Other, SourceEntity);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));

		AddTag(TAG_TaskTest_Watched, SourceEntity);
		ASSERT_THAT(IsNear(10.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(PartySource_Removed_FiresWhenTheSourceLosesTheTag)
	{
		ASSERT_THAT(IsTrue(ApplyHost(EArcTagChangeTrigger::Removed, EArcTagChangeParty::Source, SourceEntity).IsValid()));

		AddTag(TAG_TaskTest_Watched, SourceEntity);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));

		RemoveTag(TAG_TaskTest_Watched, SourceEntity);
		ASSERT_THAT(IsNear(10.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(PartySource_WithoutASourceEntity_SubscribesToNothing)
	{
		ASSERT_THAT(IsTrue(ApplyHost(EArcTagChangeTrigger::Both, EArcTagChangeParty::Source, FMassEntityHandle()).IsValid()));

		AddTag(TAG_TaskTest_Watched, Entity);
		RemoveTag(TAG_TaskTest_Watched, Entity);
		AddTag(TAG_TaskTest_Watched, SourceEntity);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(PartySource_RemovedHost_StopsReactingToTheSource)
	{
		const FArcActiveEffectHandle Host = ApplyHost(EArcTagChangeTrigger::Added, EArcTagChangeParty::Source, SourceEntity);
		ASSERT_THAT(IsTrue(Host.IsValid()));
		FArcEffectRemovalQuery Removal;
		Removal.Handle = Host;
		ArcEffects::RemoveEffects(*EntityManager, Entity, Removal);

		AddTag(TAG_TaskTest_Watched, SourceEntity);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));
	}
};

// ============================================================================
// ArcEffectTask_OnAbilityActivated: fires on ArcEvent.Ability.Activated of the host owner, after the bus drain;
// the activation goes through the production input processor.
// ============================================================================

TEST_CLASS(ArcEffectTask_AbilityActivated, "ArcMassAbilities.Effects.Tasks.OnAbilityActivated")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
	UArcGameEventSubsystem* Bus = nullptr;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogMass");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
		Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(Bus);

		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());

		FArcAttributeSet_TestStats Stats;
		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(Stats));
		Instances.Add(FInstancedStruct::Make(FArcAbilityCollectionFragment()));
		Instances.Add(FInstancedStruct::Make(FArcAbilityInputFragment()));
		Instances.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Instances.Add(FInstancedStruct::Make(FArcAbilityCooldownFragment()));
		Instances.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Instances.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Instances.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));
		Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Instances);
	}

	UArcEffectDefinition* MakeHost(UArcAbilityDefinition* Filter)
	{
		UArcEffectDefinition* Reaction = NewObject<UArcEffectDefinition>();
		Reaction->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution ArmorExec;
		ArmorExec.TargetAttribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		ArmorExec.Value = 10.f;
		Reaction->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ArmorExec)));

		UArcEffectDefinition* Host = NewObject<UArcEffectDefinition>();
		Host->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnAbilityActivated Task;
		Task.AbilityFilter = Filter;
		Task.EffectToApply = Reaction;
		Host->Tasks.Add(FInstancedStruct::Make(Task));
		return Host;
	}

	void Activate(UArcAbilityDefinition* Definition, const FGameplayTag& InputTag)
	{
		ArcAbilities::GrantAbility(*EntityManager, Entity, Definition, InputTag);
		ArcAbilities::PushInput(*EntityManager, Entity, InputTag);

		UArcAbilityInputProcessor* Processor = NewObject<UArcAbilityInputProcessor>();
		Processor->CallInitialize(&Spawner.GetWorld(), EntityManager->AsShared());
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);
		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, 0.016f);
		Processor->CallExecute(*EntityManager, ProcessingContext.GetExecutionContext());
	}

	float DrainAndReadArmor()
	{
		Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
		const FArcAttributeSet_TestStats* Stats = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		return Stats ? Stats->Armor.BaseValue : -1.f;
	}

	TEST_METHOD(AnyAbility_FiresWhenAnAbilityActivates_AfterTheDrain)
	{
		ASSERT_THAT(IsTrue(ArcEffects::TryApplyEffect(*EntityManager, Entity, MakeHost(nullptr), FMassEntityHandle()).Handle.IsValid()));

		Activate(NewObject<UArcAbilityDefinition>(), TAG_TaskTest_Input);
		ASSERT_THAT(IsNear(10.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(AbilityFilter_FiresOnlyForTheMatchingDefinition)
	{
		UArcAbilityDefinition* Wanted = NewObject<UArcAbilityDefinition>();
		UArcAbilityDefinition* Other = NewObject<UArcAbilityDefinition>();
		ASSERT_THAT(IsTrue(ArcEffects::TryApplyEffect(*EntityManager, Entity, MakeHost(Wanted), FMassEntityHandle()).Handle.IsValid()));

		Activate(Other, TAG_TaskTest_Input);
		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));

		Activate(Wanted, TAG_TaskTest_InputWanted);
		ASSERT_THAT(IsNear(10.f, DrainAndReadArmor(), 0.001f));
	}

	TEST_METHOD(RemovedHost_WithTheEventAlreadyQueued_FiresNothingAndDoesNotCrash)
	{
		const FArcEffectApplyResult Host = ArcEffects::TryApplyEffect(*EntityManager, Entity, MakeHost(nullptr), FMassEntityHandle());
		ASSERT_THAT(IsTrue(Host.Handle.IsValid()));

		Activate(NewObject<UArcAbilityDefinition>(), TAG_TaskTest_Input);
		FArcEffectRemovalQuery Removal;
		Removal.Handle = Host.Handle;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*EntityManager, Entity, Removal)));

		ASSERT_THAT(IsNear(0.f, DrainAndReadArmor(), 0.001f));
	}
};
