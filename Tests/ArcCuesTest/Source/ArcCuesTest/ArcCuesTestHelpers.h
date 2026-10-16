// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcCues/ArcCueBehavior.h"
#include "ArcCues/ArcCueDefinition.h"
#include "ArcCues/ArcCueTypes.h"

#include "ArcCuesTestHelpers.generated.h"

class UWorld;
class UArcCueSubsystem;
struct FMassEntityManager;

// ---------------------------------------------------------------------------
// Probe behaviors — log-only, no per-instance state
//
// Each probe emits a uniquely-prefixed line on every lifecycle hook:
//
//   PROBE/<ProbeName>/EXEC   handle=<id> target=(<idx>:<serial>)
//   PROBE/<ProbeName>/APP    handle=<id> target=(<idx>:<serial>)
//   PROBE/<ProbeName>/REM    handle=<id>
//   PROBE/<ProbeName>/EVENT  handle=<id>
//   ATTACH-PROBE/<Name>/APP  handle=<id> target=(<idx>:<serial>)
//   ATTACH-PROBE/<Name>/REM  handle=<id>
//   ATTACH-PROBE/<Name>/LOST handle=<id> released=<0|1>
//   ATTACH-PROBE/<Name>/EVENT handle=<id>
//
// Tests assert on these lines via FAutomationTestBase::AddExpectedMessage().
// We intentionally do NOT carry counter pointers, TSharedPtrs, or any other
// per-instance state on the probes — Mass / FInstancedStruct / TArray copies
// use memcpy on USTRUCTs (WithCopy=false by default), which corrupts pointer
// payloads. An FName ProbeName is the only per-instance data; trivially
// copyable, immune to that whole class of bug.
// ---------------------------------------------------------------------------

USTRUCT(meta = (Hidden))
struct FArcCueBehaviorState_TestProbe : public FArcCueBehaviorState
{
	GENERATED_BODY()

	/** Spawned child entity for the AttachedTestProbe variant. */
	UPROPERTY()
	FMassEntityHandle SpawnedEntity;
};

USTRUCT(meta = (DisplayName = "Test Probe", Hidden))
struct FArcCueBehavior_TestProbe : public FArcCueBehavior
{
	GENERATED_BODY()

	/** Discriminates probes that share a definition tag (e.g. Stone vs Wood). */
	UPROPERTY()
	FName ProbeName = TEXT("Probe");

	/** When true, OnApplication returns a non-empty state (persistent path). */
	UPROPERTY()
	bool bPersistent = true;

	virtual void OnExecute(const FArcCueContext& Context) const override;
	virtual TInstancedStruct<FArcCueBehaviorState> OnApplication(const FArcCueContext& Context) const override;
	virtual void OnEvent(const FArcCueContext& Context, const FArcCueEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const override;
	virtual void OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const override;
};

USTRUCT(meta = (DisplayName = "Attached Test Probe", Hidden))
struct FArcCueBehavior_AttachedTestProbe : public FArcCueBehavior
{
	GENERATED_BODY()

	UPROPERTY()
	FName ProbeName = TEXT("Attach");

	UPROPERTY()
	FTransform LocalOffset = FTransform::Identity;

	virtual TInstancedStruct<FArcCueBehaviorState> OnApplication(const FArcCueContext& Context) const override;
	virtual void OnAttachmentLost(const FArcCueContext& Context, FMassEntityHandle AttachmentEntity, TInstancedStruct<FArcCueBehaviorState>& State) const override;
	virtual void OnEvent(const FArcCueContext& Context, const FArcCueEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const override;
	virtual void OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const override;
};

// ---------------------------------------------------------------------------
// Factory + utility helpers
// ---------------------------------------------------------------------------

namespace ArcCuesTest
{
	UArcCueDefinition* CreateTransientCueDef(
		FName Name,
		FGameplayTag CueTag,
		const FGameplayTagContainer& FilterTags,
		TArray<FInstancedStruct> Behaviors);

	/** Drops the GC root added by CreateTransientCueDef. Call from AFTER_EACH. */
	void ReleaseTransientCueDef(UArcCueDefinition* Def);

	/** Convenience: build a single-behavior probe definition. */
	UArcCueDefinition* CreateProbeDef(
		FName Name,
		FGameplayTag CueTag,
		const FGameplayTagContainer& FilterTags,
		FName ProbeName,
		bool bPersistent);

	FMassEntityManager& GetEntityManager(UWorld& World);

	UArcCueSubsystem& GetCueSubsystem(UWorld& World);

	/** Run UArcMassAttachmentProcessor once over the world's entity manager. */
	void RunAttachmentProcessor(UWorld& World);

	/** Run UArcCueLostTargetProcessor once. */
	void RunLostTargetProcessor(UWorld& World);

	/** Create a target entity carrying just FTransformFragment. */
	FMassEntityHandle CreateTargetEntity(FMassEntityManager& EntityManager, const FTransform& InitialTransform = FTransform::Identity);

	/** Apply a translation to a target entity's FTransformFragment. */
	void SetEntityTransform(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FTransform& NewTransform);
}
