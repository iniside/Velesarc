// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Events/ArcEffectEventSubsystem.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "Effects/ArcActiveEffectHandle.h"

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
		TaskDef.Attribute = FArcTestStatsFragment::GetHealthAttribute();
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
		TaskDef.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Effect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::RemoveEffect(*EntityManager, Entity, Effect);

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
		Task1.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Effect->Tasks.Add(FInstancedStruct::Make(Task1));

		FArcEffectTask_OnAttributeChanged Task2;
		Task2.Attribute = FArcTestStatsFragment::GetArmorAttribute();
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
		TaskDef.Attribute = FArcTestStatsFragment::GetHealthAttribute();
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
// NOTE: These tests verify that OnAttributeChanged subscription is wired
// correctly via UArcEffectEventSubsystem. The delegate fires when
// BroadcastAttributeChanged is called, which happens from
// ArcAttributeAggregatorProcessor — a Mass processor that runs
// asynchronously via signals. In the test environment the processor does
// NOT run synchronously when TryApplyEffect is called.
//
// Therefore the ReactsToAttributeChange_AppliesEffect and
// DoesNotReactAfterEffectRemoved tests below drive the event subsystem
// directly by calling BroadcastAttributeChanged manually.
// ============================================================================

TEST_CLASS(ArcEffectTask_AttributeChanged, "ArcMassAbilities.Effects.Tasks.OnAttributeChanged")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;

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

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
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
		ArmorExec.TargetAttribute = FArcTestStatsFragment::GetArmorAttribute();
		ArmorExec.Value = 10.f;
		ReactionEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ArmorExec)));

		// Buff with task: on health changed, apply reaction
		UArcEffectDefinition* BuffEffect = NewObject<UArcEffectDefinition>();
		BuffEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		TaskDef.EffectToApply = ReactionEffect;
		BuffEffect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		// Apply the buff (this wires the delegate via OnApplication)
		ArcEffects::TryApplyEffect(*EntityManager, Entity, BuffEffect, FMassEntityHandle());

		// Manually broadcast the attribute changed event (simulates what
		// ArcAttributeAggregatorProcessor does after processing).
		UArcEffectEventSubsystem* EventSys = Spawner.GetWorld().GetSubsystem<UArcEffectEventSubsystem>();
		ASSERT_THAT(IsTrue(EventSys != nullptr));
		EventSys->BroadcastAttributeChanged(Entity, FArcTestStatsFragment::GetHealthAttribute(), 100.f, 80.f);
		DrainPendingOps(Entity);

		// Verify: armor should be 10 (reaction fired)
		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(10.f, Stats->Armor.BaseValue, 0.001f));
	}

	TEST_METHOD(DoesNotReactAfterEffectRemoved)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		// Reaction effect: adds 10 armor
		UArcEffectDefinition* ReactionEffect = NewObject<UArcEffectDefinition>();
		ReactionEffect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution ArmorExec;
		ArmorExec.TargetAttribute = FArcTestStatsFragment::GetArmorAttribute();
		ArmorExec.Value = 10.f;
		ReactionEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ArmorExec)));

		// Buff with task
		UArcEffectDefinition* BuffEffect = NewObject<UArcEffectDefinition>();
		BuffEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		TaskDef.EffectToApply = ReactionEffect;
		BuffEffect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		// Apply then remove the buff (OnRemoved should unsubscribe the delegate)
		ArcEffects::TryApplyEffect(*EntityManager, Entity, BuffEffect, FMassEntityHandle());
		ArcEffects::RemoveEffect(*EntityManager, Entity, BuffEffect);

		// Broadcast attribute changed — delegate should already be unsubscribed
		UArcEffectEventSubsystem* EventSys = Spawner.GetWorld().GetSubsystem<UArcEffectEventSubsystem>();
		ASSERT_THAT(IsTrue(EventSys != nullptr));
		EventSys->BroadcastAttributeChanged(Entity, FArcTestStatsFragment::GetHealthAttribute(), 100.f, 80.f);

		// Verify: armor should still be 0 (reaction did NOT fire)
		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(0.f, Stats->Armor.BaseValue, 0.001f));
	}

	TEST_METHOD(NoEffectToApply_DoesNotCrash)
	{
		// Task with no EffectToApply should silently no-op on broadcast.
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* BuffEffect = NewObject<UArcEffectDefinition>();
		BuffEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		TaskDef.EffectToApply = nullptr;
		BuffEffect->Tasks.Add(FInstancedStruct::Make(TaskDef));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, BuffEffect, FMassEntityHandle());

		UArcEffectEventSubsystem* EventSys = Spawner.GetWorld().GetSubsystem<UArcEffectEventSubsystem>();
		ASSERT_THAT(IsTrue(EventSys != nullptr));
		// Should not crash
		EventSys->BroadcastAttributeChanged(Entity, FArcTestStatsFragment::GetHealthAttribute(), 100.f, 80.f);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(0.f, Stats->Armor.BaseValue, 0.001f));
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

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
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
		ArmorExec.TargetAttribute = FArcTestStatsFragment::GetArmorAttribute();
		ArmorExec.Value = 10.f;
		ReactionEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ArmorExec)));

		// Buff with OnAttributeChanged task
		UArcEffectDefinition* BuffEffect = NewObject<UArcEffectDefinition>();
		BuffEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		BuffEffect->StackingPolicy.StackType = EArcStackType::Instance;
		BuffEffect->StackingPolicy.MaxStacks = 100;
		FArcEffectTask_OnAttributeChanged TaskDef;
		TaskDef.Attribute = FArcTestStatsFragment::GetHealthAttribute();
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

		// Fire the attribute changed event — the original task's delegate must still work
		UArcEffectEventSubsystem* EventSys = Spawner.GetWorld().GetSubsystem<UArcEffectEventSubsystem>();
		ASSERT_THAT(IsTrue(EventSys != nullptr));
		EventSys->BroadcastAttributeChanged(Entity, FArcTestStatsFragment::GetHealthAttribute(), 100.f, 80.f);
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(10.f, Stats->Armor.BaseValue, 0.001f));
	}
};
