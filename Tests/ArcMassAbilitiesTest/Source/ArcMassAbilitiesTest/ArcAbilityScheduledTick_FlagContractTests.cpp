// Copyright Lukasz Baran. All Rights Reserved.
//
// Unit tests asserting the scheduled-tick flag-combination contract on all six
// migrated tasks.
//
// These tests are purely constructive — no entity manager, no spawner, no world.
// Each test constructs the task struct on the stack and asserts the trio:
//   bShouldCallTick, bConsideredForScheduling, DrivesScheduledTick().
//
// Migrated tasks (drive scheduled tick):
//   FArcAbilityTask_WaitDuration          — tick=true,  sched=false, drives=true
//   FArcAbilityTask_PlayMontage           — tick=true,  sched=false, drives=true
//   FArcAbilityTask_PlayMontageManual     — tick=true,  sched=true,  drives=false (every-frame for OnNotify crossings)
//   FArcAbilityTask_WaitEnhancedInputGated— tick=false, sched=false, drives=true
//   FArcAbilityTask_InputPressed          — tick=false, sched=false, drives=true
//   FArcAbilityTask_InputReleased         — tick=false, sched=false, drives=true

#include "CQTest.h"
#include "Abilities/Tasks/ArcAbilityTask_WaitDuration.h"
#include "Abilities/Tasks/ArcAbilityTask_PlayMontage.h"
#include "Abilities/Tasks/ArcAbilityTask_WaitEnhancedInputGated.h"
#include "Abilities/Tasks/ArcAbilityTask_InputPressed.h"
#include "Abilities/Tasks/ArcAbilityTask_InputReleased.h"
#include "Abilities/Tasks/ArcAbilityTask_InputReleasedOnce.h"
#include "Tasks/ArcAbilityTask_PlayMontageManual.h"

// ============================================================================
// ArcAbility_ScheduledTickFlagContract — pure construction-and-assert tests
// ============================================================================

TEST_CLASS(ArcAbility_ScheduledTickFlagContract, "ArcMassAbilities.Abilities.ScheduledTickFlagContract")
{
	// -------------------------------------------------------------------------
	// WaitDuration: drives scheduled tick via AddScheduledTickRequest in
	// EnterState. bShouldCallTick=true so Tick is invoked, bConsideredForScheduling
	// =false so the per-state aggregation does not force MakeEveryFrames.
	// -------------------------------------------------------------------------
	TEST_METHOD(WaitDuration_FlagsMatchContract)
	{
		FArcAbilityTask_WaitDuration Task;
		ASSERT_THAT(IsTrue(static_cast<bool>(Task.bShouldCallTick)));
		ASSERT_THAT(IsFalse(static_cast<bool>(Task.bConsideredForScheduling)));
		ASSERT_THAT(IsTrue(Task.DrivesScheduledTick()));
	}

	// -------------------------------------------------------------------------
	// PlayMontage: identical flag contract to WaitDuration — uses a
	// FScheduledTickHandle in its instance data for the natural-end wake.
	// -------------------------------------------------------------------------
	TEST_METHOD(PlayMontage_FlagsMatchContract)
	{
		FArcAbilityTask_PlayMontage Task;
		ASSERT_THAT(IsTrue(static_cast<bool>(Task.bShouldCallTick)));
		ASSERT_THAT(IsFalse(static_cast<bool>(Task.bConsideredForScheduling)));
		ASSERT_THAT(IsTrue(Task.DrivesScheduledTick()));
	}

	// -------------------------------------------------------------------------
	// PlayMontageManual (ArcAbilityKit): every-frame tick, no scheduled wake.
	// Notify-broadcast crossings require sub-cycle Tick resolution, so the task
	// keeps bConsideredForScheduling=true and does NOT override DrivesScheduledTick.
	// -------------------------------------------------------------------------
	TEST_METHOD(PlayMontageManual_FlagsMatchContract)
	{
		FArcAbilityTask_PlayMontageManual Task;
		ASSERT_THAT(IsTrue(static_cast<bool>(Task.bShouldCallTick)));
		ASSERT_THAT(IsTrue(static_cast<bool>(Task.bConsideredForScheduling)));
		ASSERT_THAT(IsFalse(Task.DrivesScheduledTick()));
	}

	// -------------------------------------------------------------------------
	// WaitEnhancedInputGated: purely event-driven — no Tick override at all.
	// bShouldCallTick=false prevents the per-state aggregation from requesting
	// a tick slot. bConsideredForScheduling=false and DrivesScheduledTick=true
	// signal that the task manages its own wake via EnhancedInput callbacks.
	// -------------------------------------------------------------------------
	TEST_METHOD(WaitEnhancedInputGated_FlagsMatchContract)
	{
		FArcAbilityTask_WaitEnhancedInputGated Task;
		ASSERT_THAT(IsFalse(static_cast<bool>(Task.bShouldCallTick)));
		ASSERT_THAT(IsFalse(static_cast<bool>(Task.bConsideredForScheduling)));
		ASSERT_THAT(IsTrue(Task.DrivesScheduledTick()));
	}

	// -------------------------------------------------------------------------
	// InputPressed: signal-listener task — same flag contract as
	// WaitEnhancedInputGated. Subscribes to AbilityActiveInputReceived in
	// EnterState; no Tick, no scheduled wake of its own.
	// -------------------------------------------------------------------------
	TEST_METHOD(InputPressed_FlagsMatchContract)
	{
		FArcAbilityTask_InputPressed Task;
		ASSERT_THAT(IsFalse(static_cast<bool>(Task.bShouldCallTick)));
		ASSERT_THAT(IsFalse(static_cast<bool>(Task.bConsideredForScheduling)));
		ASSERT_THAT(IsTrue(Task.DrivesScheduledTick()));
	}

	// -------------------------------------------------------------------------
	// InputReleased: signal-listener task — same flag contract as InputPressed.
	// Subscribes to AbilityActiveInputReceived in EnterState; two-phase lambda
	// accumulates press then fires on release. No Tick, no scheduled wake.
	// -------------------------------------------------------------------------
	TEST_METHOD(InputReleased_FlagsMatchContract)
	{
		FArcAbilityTask_InputReleased Task;
		ASSERT_THAT(IsFalse(static_cast<bool>(Task.bShouldCallTick)));
		ASSERT_THAT(IsFalse(static_cast<bool>(Task.bConsideredForScheduling)));
		ASSERT_THAT(IsTrue(Task.DrivesScheduledTick()));
	}

	// -------------------------------------------------------------------------
	// InputReleasedOnce: single-stage variant of InputReleased — fires on the
	// first release after EnterState with no prior-press requirement. Same flag
	// contract: no Tick, no scheduled wake of its own.
	// -------------------------------------------------------------------------
	TEST_METHOD(InputReleasedOnce_FlagsMatchContract)
	{
		FArcAbilityTask_InputReleasedOnce Task;
		ASSERT_THAT(IsFalse(static_cast<bool>(Task.bShouldCallTick)));
		ASSERT_THAT(IsFalse(static_cast<bool>(Task.bConsideredForScheduling)));
		ASSERT_THAT(IsTrue(Task.DrivesScheduledTick()));
	}

};
