// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeEvent.h"
#include "ArcKnowledgeTypes.h"
#include "AsyncMessageHandle.h"
#include "AsyncMessageId.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "StateTreeDelegate.h"
#include "ArcMass/ArcMassEntityHandleWrapper.h"
#include "ArcMassListenToKnowledgeEventTask.generated.h"

/**
 * Instance data for the knowledge event listener task.
 *
 * Parameters:
 *   MessageId — set to the broadcaster's GlobalMessageId (all events) or
 *               SpatialMessageId (only events near this entity). These IDs
 *               use the gameplay tags GameKnowledge.Event.GlobalMessage and
 *               GameKnowledge.Event.SpatialMessage respectively.
 *
 * Outputs are split into two groups:
 *   Event-derived (always populated) — data copied directly from the
 *     FArcKnowledgeChangedEvent payload: KnowledgeHandle, EventType, Tags, Location.
 *   Entry-derived (populated when bEntryAvailable is true) — looked up from
 *     UArcKnowledgeSubsystem using the handle: SourceEntity, Payload, Relevance,
 *     Timestamp, Lifetime, bClaimed, ClaimedBy. For Removed events the entry no
 *     longer exists, so bEntryAvailable will be false and these fields are stale.
 *
 * Dispatchers:
 *   Bind a listener state's transition to the dispatcher matching the event type
 *   you care about. Multiple dispatchers can be bound simultaneously — only the
 *   one matching the incoming event fires per callback.
 */
USTRUCT()
struct FArcMassListenToKnowledgeEventTaskInstanceData
{
	GENERATED_BODY()

	// --- Parameters ---

	/**
	 * Which async message ID to listen on.
	 * Use GlobalMessageId (TAG_GameKnowledge_Event_GlobalMessage) to receive all knowledge events,
	 * or SpatialMessageId (TAG_GameKnowledge_Event_SpatialMessage) to receive only events
	 * broadcast to nearby entities via their async message endpoints.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FAsyncMessageId MessageId = FAsyncMessageId::Invalid;

	// --- Outputs (event-derived, always populated) ---

	/**
	 * The knowledge entry handle from the event. Bind this to FArcMassClaimAdvertisementTask::AdvertisementHandle
	 * or FArcMassExecuteAdvertisementTask::AdvertisementHandle to act on the entry.
	 */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeHandle KnowledgeHandle;

	/** Which event type fired (Registered, Updated, Removed, AdvertisementPosted/Claimed/Completed). */
	UPROPERTY(EditAnywhere, Category = Output)
	EArcKnowledgeEventType EventType = EArcKnowledgeEventType::Registered;

	/** Tags from the knowledge entry at the time of the event. Useful for filtering in transitions. */
	UPROPERTY(EditAnywhere, Category = Output)
	FGameplayTagContainer Tags;

	/** World location of the knowledge entry. Zero for non-spatial entries. */
	UPROPERTY(EditAnywhere, Category = Output)
	FVector Location = FVector::ZeroVector;

	/**
	 * Whether the full entry was found in the subsystem when the event arrived.
	 * False for Removed events (entry already gone) and if the entry expired between broadcast and delivery.
	 * When false, entry-derived outputs below are default/stale — only event-derived outputs are valid.
	 */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bEntryAvailable = false;

	// --- Outputs (entry-derived, populated when bEntryAvailable) ---

	/** The entity that posted/owns the knowledge entry. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcMassEntityHandleWrapper SourceEntity;

	/** The entry's FInstancedStruct payload (domain-specific data, e.g. settlement info, resource data). */
	UPROPERTY(EditAnywhere, Category = Output)
	FInstancedStruct Payload;

	/** 0-1 relevance score assigned by the poster. Higher = more important. */
	UPROPERTY(EditAnywhere, Category = Output)
	float Relevance = 1.0f;

	/** World time when the entry was registered or last updated. */
	UPROPERTY(EditAnywhere, Category = Output)
	double Timestamp = 0.0;

	/** Time-to-live in seconds. 0 = entry never expires. */
	UPROPERTY(EditAnywhere, Category = Output)
	float Lifetime = 0.0f;

	/** Whether the entry is currently claimed by an entity (advertisement semantics). */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bClaimed = false;

	/** The entity that claimed this entry. Only valid when bClaimed is true. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcMassEntityHandleWrapper ClaimedBy;

	// --- Dispatchers (one per event type) ---
	// Bind a state transition to the dispatcher(s) you want to react to.

	/** Fires when a new knowledge entry is registered. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnRegistered;

	/** Fires when an existing knowledge entry's data changes. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnUpdated;

	/** Fires when a knowledge entry is removed from the subsystem. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnRemoved;

	/** Fires when a new advertisement is posted. React with Claim → Execute. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnAdvertisementPosted;

	/** Fires when an advertisement is claimed. Check ClaimedBy to see who took it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnAdvertisementClaimed;

	/** Fires when a claimed advertisement is completed. For persistent ads, Unclaim to recycle. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnAdvertisementCompleted;

	// --- Internal ---

	FAsyncMessageHandle BoundListenerHandle = FAsyncMessageHandle::Invalid;
};

/**
 * StateTree running task that listens for knowledge change events
 * on the entity's own async message endpoint.
 *
 * Binds in Tick (once the endpoint fragment exists), stores the event
 * in instance data, fires a delegate, and signals NewStateTreeTaskRequired
 * so the StateTree can react immediately.
 *
 * For global events, use the GlobalMessageId from the broadcaster config.
 * For spatial (per-entity) events, use the SpatialMessageId.
 *
 * --- Usage ---
 *
 * This task is meant to be paired with a listener state that binds to one of
 * its delegate dispatchers. When an event fires, the task populates all output
 * properties (KnowledgeHandle, Tags, Location, Payload, etc.) and broadcasts
 * the matching delegate. The StateTree can then transition to a handler state
 * that acts on the event data.
 *
 * --- Delegates and recommended follow-up tasks ---
 *
 * OnRegistered:
 *   A new knowledge entry was added to the subsystem. Useful for reacting to
 *   newly available information (e.g. a resource deposit appeared). Follow up
 *   with FArcMassQueryKnowledgeTask to score/filter, or read outputs directly.
 *
 * OnUpdated:
 *   An existing entry's data changed (location, tags, payload, relevance).
 *   Use this to re-evaluate decisions based on stale knowledge. Same follow-up
 *   as OnRegistered — query or read the updated outputs.
 *
 * OnRemoved:
 *   An entry was removed from the subsystem. bEntryAvailable will be false
 *   since the entry no longer exists. Use KnowledgeHandle/Tags/Location from
 *   event data to clean up any local state referencing the removed entry.
 *   No follow-up task needed — this is informational only.
 *
 * OnAdvertisementPosted:
 *   Someone posted an advertisement (a claimable job/task). To act on it:
 *   1. FArcMassClaimAdvertisementTask — claim it so no one else takes it
 *   2. FArcMassExecuteAdvertisementTask — run the advertisement's embedded
 *      StateTree instruction (if it carries FArcAdvertisementInstruction_StateTree)
 *
 * OnAdvertisementClaimed:
 *   An advertisement was claimed by an entity (could be this entity or another).
 *   The poster of the advertisement typically listens for this to know that
 *   someone accepted the job. If this entity is the poster and wants to act on
 *   the claim, use FArcMassExecuteAdvertisementTask with the KnowledgeHandle
 *   to run any poster-side reaction logic. Check ClaimedBy output to see who
 *   claimed it.
 *
 * OnAdvertisementCompleted:
 *   An advertisement was fulfilled and completed. The poster can use this to
 *   finalize state (e.g. mark a job as done, reward the claimer). Informational
 *   for most listeners — no specific follow-up task required. For persistent
 *   advertisements (bPersistent=true), use FArcMassUnclaimAdvertisementTask to
 *   recycle the advertisement so it becomes available again.
 */
USTRUCT(meta = (DisplayName = "Arc Listen To Knowledge Event", Category = "ArcKnowledge"))
struct ARCKNOWLEDGE_API FArcMassListenToKnowledgeEventTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassListenToKnowledgeEventTaskInstanceData;

	FArcMassListenToKnowledgeEventTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Event"); }
	virtual FColor GetIconColor() const override { return FColor(200, 160, 60); }
#endif
};
