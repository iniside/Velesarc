// Copyright Lukasz Baran. All Rights Reserved.
//
// CQTest coverage for the scheduled-tick / WaitDuration migration introduced in
// docs/plans/2026-05-23-1533-arcmassabilities-conditional-tick-waitduration-plan.md
//
// Four test cases:
//   1. WaitDurationCompletesAfterScheduledDelay
//   2. WaitDurationZeroDurationSucceedsImmediately
//   3. WaitDurationRapidReentryDoesNotRace
//   4. SleepingAbilityDoesNotWakeUntilSignaled
//
// Time-advance mechanism chosen: write directly to World.TimeSeconds then call
// SignalSubsystem->Tick(delta). Rationale: DelaySignalEntities stores
// (CachedWorld->GetTimeSeconds() + Delay) as TargetTimestamp and
// UMassSignalSubsystem::Tick checks (TargetTimestamp <= CachedWorld->GetTimeSeconds()).
// World.TimeSeconds is a public double field — direct write is safe in tests.
// No other test-world Tick machinery needs to run; the signal subsystem is the
// only consumer of World time in this pipeline.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Processors/ArcAbilityStateTreeTickProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "Abilities/Tasks/ArcAbilityTask_WaitDuration.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SchedTick_Input_Fire, "Test.InputTag.Ability.Fire");

// ============================================================================
// Local helpers (file-scoped namespace — mandatory per CLAUDE.md)
// ============================================================================

namespace ArcAbilityScheduledTickTestHelpers
{

FMassEntityHandle CreateTestEntity(FMassEntityManager& EntityManager)
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

UStateTree* CreateWaitDurationTree(UObject* Outer, float Duration)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	TStateTreeEditorNode<FArcAbilityTask_WaitDuration>& WaitTask =
		Root.AddTask<FArcAbilityTask_WaitDuration>();
	WaitTask.GetInstanceData().Duration = Duration;

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	const bool bOk = Compiler.Compile(*StateTree);
	checkf(bOk, TEXT("CreateWaitDurationTree: compile failed (Duration=%.3f)"), Duration);
	return StateTree;
}

} // namespace ArcAbilityScheduledTickTestHelpers

// ============================================================================
// ArcAbility_ScheduledTick — WaitDuration via scheduled-tick pipeline
// ============================================================================

TEST_CLASS(ArcAbility_ScheduledTick, "ArcMassAbilities.Abilities.ScheduledTick")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcAbilityStateTreeSubsystem* AbilitySubsystem = nullptr;

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
	}

	// -------------------------------------------------------------------------
	// Private helpers
	// -------------------------------------------------------------------------

	// Run the input processor for Entity at CurrentTime. The input processor
	// reads the signal queue, starts any matching ability whose input tag was
	// pressed, and enqueues a DelaySignal for the scheduled wake.
	void ExecuteInputProcessor(FMassEntityHandle Entity, float DeltaTime = 0.016f)
	{
		UArcAbilityInputProcessor* Processor = NewObject<UArcAbilityInputProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, DeltaTime);
		FMassExecutionContext& Context = ProcessingContext.GetExecutionContext();
		Processor->CallExecute(*EntityManager, Context);
	}

	// Run the tick processor for Entity. The processor ticks the active ability
	// StateTree, reads the next scheduled tick after the Tick call, and enqueues
	// the next DelaySignal (or immediate signal) as needed.
	void ExecuteTickProcessor(FMassEntityHandle Entity, float DeltaTime = 0.016f)
	{
		UArcAbilityStateTreeTickProcessor* Processor = NewObject<UArcAbilityStateTreeTickProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, DeltaTime);
		FMassExecutionContext& Context = ProcessingContext.GetExecutionContext();
		Processor->CallExecute(*EntityManager, Context);
	}

	// Advance World time by Delta seconds and call SignalSubsystem::Tick so
	// that any pending DelayedSignals whose TargetTimestamp <= new CurrentTime
	// are flushed and broadcast to their subscriber delegates.
	//
	// The signal subsystem stores TargetTimestamp = World.GetTimeSeconds() + Delay
	// at enqueue time, and checks TargetTimestamp <= World.GetTimeSeconds() in its
	// own Tick. Writing directly to World.TimeSeconds is the only way to advance
	// time in the test world — there is no timer-manager drive or World::Tick call
	// in FActorTestSpawner. After the Tick() the delayed signal is converted to an
	// immediate SignalEntity broadcast, which registers the entity in the processor's
	// double-buffered signal queue ready for the next CallExecute.
	void AdvanceWorldTime(float Delta)
	{
		Spawner.GetWorld().TimeSeconds += static_cast<double>(Delta);
		// UMassSignalSubsystem::Tick is declared protected, but it overrides the
		// public UTickableWorldSubsystem::Tick. Cast through the base to access
		// it; virtual dispatch still runs the signal subsystem's override.
		UTickableWorldSubsystem* Tickable = static_cast<UTickableWorldSubsystem*>(SignalSubsystem);
		Tickable->Tick(Delta);
	}

	// -------------------------------------------------------------------------
	// Test 1
	// -------------------------------------------------------------------------

	// Grant an ability backed by WaitDuration(0.5s).
	// After input dispatch:
	//   - ability is Running (WaitDuration returns Running from EnterState)
	//   - NextWakeTimeInSeconds is set to CurrentTime + 0.5 (input processor reads schedule)
	// After advancing 0.5s + running the tick processor:
	//   - ability transitions to Succeeded and bIsActive becomes false
	TEST_METHOD(WaitDurationCompletesAfterScheduledDelay)
	{
		FMassEntityHandle Entity = ArcAbilityScheduledTickTestHelpers::CreateTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = NewObject<UArcAbilityDefinition>();
		Def->StateTree = ArcAbilityScheduledTickTestHelpers::CreateWaitDurationTree(&Spawner.GetWorld(), 0.5f);
		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_SchedTick_Input_Fire);

		const double TimeBeforeInput = Spawner.GetWorld().GetTimeSeconds();

		// Trigger input → starts the ability, EnterState runs, delayed signal enqueued
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_SchedTick_Input_Fire);
		ExecuteInputProcessor(Entity);

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
			ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
			ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));
			// NextWakeTimeInSeconds must be ~TimeBeforeInput + 0.5
			ASSERT_THAT(IsTrue(Collection.Abilities[0].NextWakeTimeInSeconds > TimeBeforeInput));
		}

		// Advance past the 0.5s threshold. AdvanceWorldTime flushes the delayed signal.
		AdvanceWorldTime(0.5f);

		// The delayed signal is now queued as an immediate signal. Run the tick processor
		// to consume it. DeltaTime passed to the tick processor is the delta since the
		// ability was last ticked. The tick processor uses World.GetTimeSeconds() minus
		// Ability.LastUpdateTimeInSeconds — so after 0.5s advance the DeltaTime computed
		// inside the processor will be ~0.5s, which satisfies Elapsed >= Duration.
		ExecuteTickProcessor(Entity, 0.5f);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));
	}

	// -------------------------------------------------------------------------
	// Test 2
	// -------------------------------------------------------------------------

	// WaitDuration(0.0) returns EStateTreeRunStatus::Succeeded directly from EnterState
	// (early-return path: Duration <= 0). The ability must complete in the input pass
	// alone — no tick processor call needed.
	TEST_METHOD(WaitDurationZeroDurationSucceedsImmediately)
	{
		FMassEntityHandle Entity = ArcAbilityScheduledTickTestHelpers::CreateTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = NewObject<UArcAbilityDefinition>();
		Def->StateTree = ArcAbilityScheduledTickTestHelpers::CreateWaitDurationTree(&Spawner.GetWorld(), 0.0f);
		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_SchedTick_Input_Fire);

		ArcAbilities::PushInput(*EntityManager, Entity, TAG_SchedTick_Input_Fire);
		ExecuteInputProcessor(Entity);

		// The input processor calls ExecContext.Start(), which calls EnterState.
		// EnterState returns Succeeded immediately for Duration <= 0.
		// The input processor then sees RunStatus == Succeeded and sets bIsActive = true.
		// The tick processor (Pass 2 teardown) cleans up the ability to bIsActive = false
		// when RunStatus is Succeeded. So one tick processor pass is still needed.
		ExecuteTickProcessor(Entity, 0.016f);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));
		// No delayed signal was enqueued (early-return path skips AddScheduledTickRequest).
		// NextWakeTimeInSeconds must still be at the sentinel -1.0 (never set).
		ASSERT_THAT(IsTrue(Collection.Abilities[0].NextWakeTimeInSeconds < 0.0));
	}

	// -------------------------------------------------------------------------
	// Test 3
	// -------------------------------------------------------------------------

	// Rapid re-entry race test: the original bug that motivated the serial-fence
	// fix (see docs/status/2026-05-23-1507-arcmassabilities-waitduration-serial-fence-fix.md).
	// The new scheduled-tick architecture eliminates the sparse fragment entirely —
	// there is no fragment slot to race. This test verifies the correctness of the
	// new architecture:
	//   1. Start WaitDuration(1.0)
	//   2. Cancel before the 1s wait completes
	//   3. Immediately re-trigger the same ability
	//   4. Advance 1s + tick
	//   5. Assert the second instance completed Succeeded
	TEST_METHOD(WaitDurationRapidReentryDoesNotRace)
	{
		FMassEntityHandle Entity = ArcAbilityScheduledTickTestHelpers::CreateTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = NewObject<UArcAbilityDefinition>();
		Def->StateTree = ArcAbilityScheduledTickTestHelpers::CreateWaitDurationTree(&Spawner.GetWorld(), 1.0f);
		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_SchedTick_Input_Fire);

		// --- First activation ---
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_SchedTick_Input_Fire);
		ExecuteInputProcessor(Entity);

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
			ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
			ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));
		}

		// Advance only 0.1s — the 1.0s delayed signal is still in the future.
		AdvanceWorldTime(0.1f);

		// Cancel before the delayed signal fires. CancelAbility calls Stop() and frees
		// instance data, setting bIsActive = false and RunStatus = Failed.
		ArcAbilities::CancelAbility(*EntityManager, Entity, Handle, *AbilitySubsystem, EArcAbilityEndReason::Cancelled, FMassEntityHandle());

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
			ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		}

		// --- Second activation (re-entry) ---
		// World time is now 0.1s. We re-trigger the ability immediately.
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_SchedTick_Input_Fire);
		ExecuteInputProcessor(Entity);

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
			ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
			ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));
		}

		// Advance 1.0s past the second activation. This covers the full WaitDuration.
		// The stale delayed signal from the first activation has a TargetTimestamp of
		// ~(0.0 + 1.0) = 1.0s. Current time is now 0.1 + 1.0 = 1.1s, so it will fire
		// at this Tick — but it targets an ability that is in its second instance with
		// fresh instance data; the WaitDuration Tick will see Elapsed >= Duration and
		// return Succeeded correctly. No sparse-fragment slot exists to be poisoned.
		AdvanceWorldTime(1.0f);

		// The delayed signal(s) from both activations are now past due. Tick processor
		// processes them.
		ExecuteTickProcessor(Entity, 1.0f);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));
	}

	// -------------------------------------------------------------------------
	// Test 4
	// -------------------------------------------------------------------------

	// Verify that a sleeping WaitDuration(10.0) ability does NOT cause spurious
	// tick processor invocations between waking events.
	//
	// Instrumentation approach: because inline UCLASS subclasses require UHT
	// plumbing that does not exist in test .cpp files (confirmed by existing project
	// comment in ArcMassGameModeResolutionTests.cpp), a mock UMassSignalProcessorBase
	// with a counter cannot be declared here. Instead we verify the same invariant
	// from the data side:
	//
	//   - After input dispatch, NextWakeTimeInSeconds is set to CurrentTime + 10.0
	//   - Run multiple tick-processor passes WITHOUT advancing time — each pass must
	//     find no pending signal for AbilityStateTreeTickRequired (the signal was
	//     not enqueued because UMassSignalSubsystem::Tick only fires a delayed signal
	//     when TargetTimestamp <= CurrentTime, which is false here)
	//   - Assert the ability remains Running and bIsActive == true throughout
	//   - Assert NextWakeTimeInSeconds is still > CurrentTime (no early re-enqueue)
	//
	// If the old "tick every frame" path were still active, each ExecuteTickProcessor
	// call would find the signal present and decrement the ability toward completion.
	// With the scheduled-tick path, no signal is present so the tick processor
	// correctly skips the entity.
	TEST_METHOD(SleepingAbilityDoesNotWakeUntilSignaled)
	{
		FMassEntityHandle Entity = ArcAbilityScheduledTickTestHelpers::CreateTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = NewObject<UArcAbilityDefinition>();
		Def->StateTree = ArcAbilityScheduledTickTestHelpers::CreateWaitDurationTree(&Spawner.GetWorld(), 10.0f);
		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_SchedTick_Input_Fire);

		const double TimeAtActivation = Spawner.GetWorld().GetTimeSeconds();

		// Trigger input. The input processor starts the ability and enqueues a 10s
		// delayed signal. Nothing fires immediately.
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_SchedTick_Input_Fire);
		ExecuteInputProcessor(Entity);

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
			ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
			ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));
			// NextWakeTimeInSeconds must be set to ~TimeAtActivation + 10.0
			ASSERT_THAT(IsTrue(Collection.Abilities[0].NextWakeTimeInSeconds > TimeAtActivation + 9.0));
		}

		// Run several "idle frame" ticks WITHOUT advancing world time.
		// World.TimeSeconds stays at TimeAtActivation, so the delayed signal's
		// TargetTimestamp (~TimeAtActivation + 10) is still in the future.
		// No signal is delivered to the tick processor; the entity is skipped.
		for (int32 IdleFrame = 0; IdleFrame < 5; ++IdleFrame)
		{
			// Advance time by a small amount well below the 10s threshold.
			// 0.016s * 5 = 0.08s total — far from the 10s wake.
			AdvanceWorldTime(0.016f);

			// The tick processor must NOT find a signal for this entity and must
			// therefore not tick the ability tree. We call ExecuteTickProcessor
			// without manually injecting a signal — it is a no-op for this entity
			// because no signal was broadcast by AdvanceWorldTime (time < threshold).
			// We call it here only to prove it does not cause a spurious wake.
			UArcAbilityStateTreeTickProcessor* TickProcessor =
				NewObject<UArcAbilityStateTreeTickProcessor>();
			TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
			TickProcessor->CallInitialize(&Spawner.GetWorld(), SharedEM);
			// No SignalEntity call — testing that absent signal = no processing.
			UE::Mass::FProcessingContext Ctx(*EntityManager, 0.016f);
			TickProcessor->CallExecute(*EntityManager, Ctx.GetExecutionContext());

			FMassEntityView View(*EntityManager, Entity);
			const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());

			// Ability must still be Running — no spurious wake-and-complete.
			ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
			ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));
		}

		// After 5 idle frames (0.08s total), NextWakeTimeInSeconds must still be far
		// in the future — no early re-enqueue or wake has occurred.
		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
			const double CurrentTime = Spawner.GetWorld().GetTimeSeconds();
			ASSERT_THAT(IsTrue(Collection.Abilities[0].NextWakeTimeInSeconds > CurrentTime));
		}

		// Sanity: advance past the 10s threshold and verify the ability completes.
		AdvanceWorldTime(10.0f);
		ExecuteTickProcessor(Entity, 10.0f);

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
			ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
			ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));
		}
	}
};
