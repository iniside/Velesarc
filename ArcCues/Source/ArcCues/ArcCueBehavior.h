// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcCueTypes.h"

#include "ArcCueBehavior.generated.h"

struct FStateTreeEvent;

// ---------------------------------------------------------------------------
// FArcCueBehaviorState
//
// Base for per-cue mutable state owned by a behavior on a persistent cue.
// Subclassed by behaviors that need to remember spawned resources (Niagara
// component handle, child entity handle, StateTree instance, etc.). Stored
// in TInstancedStruct<FArcCueBehaviorState> on the persistent cue instance.
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCCUES_API FArcCueBehaviorState
{
	GENERATED_BODY()

	virtual ~FArcCueBehaviorState() = default;
};

// ---------------------------------------------------------------------------
// FArcCueBehavior
//
// Base for composable cue behaviors. Mirrors engine GameplayCueNotify_Static
// (OnExecute / OnActive / WhileActive / OnRemove) and the Overwatch effect-
// event lifecycle (see GameDesign/research/ability-system-archetypes.md).
//
// All hooks are const; mutation lives in the per-cue state struct.
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCCUES_API FArcCueBehavior
{
	GENERATED_BODY()

	virtual ~FArcCueBehavior() = default;

	/** One-shot path. */
	virtual void OnExecute(const FArcCueContext& Context) const {}

	/** Persistent path entry. An empty initial state slot may be initialized by OnEvent later. */
	virtual TInstancedStruct<FArcCueBehaviorState> OnApplication(const FArcCueContext& Context) const { return {}; }

	virtual void OnEvent(const FArcCueContext& Context, const FArcCueEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const {}
	virtual void OnStateTreeEvent(const FArcCueContext& Context, const FStateTreeEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const {}
	virtual void OnAttachmentLost(const FArcCueContext& Context, FMassEntityHandle AttachmentEntity, TInstancedStruct<FArcCueBehaviorState>& State) const {}

	/** Per-tick hook. Only invoked for behaviors that returned non-empty state from OnApplication. */
	virtual void OnTick(const FArcCueContext& Context, FArcCueBehaviorState& State, float DeltaTime) const {}

	/** Mirrors OnApplication. Only invoked for behaviors that returned non-empty state. */
	virtual void OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const {}

	/** Whether OnTick should be called. Default false — cues are stateful but not all need ticking. */
	virtual bool WantsTick() const { return false; }
};
