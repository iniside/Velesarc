// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <atomic>

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"

#include "ArcLoreApi.h" // wraps lore.h (lore_event_t, lore_string_t, notification ABI)

// Kind of a lore push notification, mapped from the LORE_EVENT_NOTIFICATION_*
// tags. Plain POD enum (no reflection) — this type only crosses the lore
// worker thread -> game-thread MPSC queue, never a UObject/Slate boundary.
enum class EArcLoreNotificationKind : uint8
{
	BranchPushed,
	BranchCreated,
	BranchDeleted,
	ResourceLocked,
	ResourceUnlocked,
};

// One decoded push event, marshalled out of lore-owned memory on the worker
// thread (all strings deep-copied) and consumed on the game thread. POD —
// trivially movable; carries only the kind and the acting user's id (empty for
// branch_created/branch_deleted, which carry no user_id in the C ABI).
struct FArcLoreNotificationEvent
{
	EArcLoreNotificationKind Kind = EArcLoreNotificationKind::BranchPushed;
	FString UserId;
};

// Heap-owned context for a single repository's live notification stream.
//
// Threading model:
//   - Producers: lore-owned worker threads invoke OnLoreEvent, which enqueues
//     into EventQueue (MPSC = many producers). They touch NO UObject/Slate.
//   - Consumer: the game thread calls DrainInto (MPSC single-consumer contract:
//     DrainInto is the SOLE dequeuer).
//
// Lifetime invariant (the design rests on this):
//   The object and its EventQueue MUST outlive any in-flight callback. Stop()
//   calls the SYNC lore_notification_unsubscribe, which awaits lore's delivery
//   task, so no subscribe-config callback can fire after Stop() returns. The
//   owner therefore destroys this object only AFTER Stop() has returned.
//
// This is NOT a UObject (no UCLASS / .generated.h): it must be heap-owned with a
// stable address handed to lore as user_context, and it owns a TQueue + atomic
// that have no UObject lifetime semantics.
class FArcLoreNotificationSubscription
{
public:
	FArcLoreNotificationSubscription(const FString& InRepoRoot, const FString& InIdentity);

	// Non-copyable, non-movable: it owns a TQueue and a std::atomic, and lore
	// holds a raw pointer (user_context) to a fixed address. Any copy/move would
	// either dangle that pointer or duplicate the queue.
	FArcLoreNotificationSubscription(const FArcLoreNotificationSubscription&) = delete;
	FArcLoreNotificationSubscription& operator=(const FArcLoreNotificationSubscription&) = delete;
	FArcLoreNotificationSubscription(FArcLoreNotificationSubscription&&) = delete;
	FArcLoreNotificationSubscription& operator=(FArcLoreNotificationSubscription&&) = delete;

	// Subscribe via the non-blocking async ABI. Builds a LOCAL global-args scope
	// (lore copies the args before the call returns, per ArcLoreApi.h), enables
	// enqueue, and hands &OnLoreEvent + this as user_context to lore. Guarded by
	// ArcLoreApi::IsReady(). Game-thread callable; returns immediately.
	void Start();

	// Disable enqueue, then unsubscribe via the SYNC ABI (blocks until lore has
	// awaited its delivery task -> callbacks quiesced). After this returns no
	// subscribe-config callback can fire, so the owner may destroy this object.
	void Stop();

	// Drain every queued event into Sink on the game thread. SOLE consumer of
	// EventQueue (MPSC single-consumer contract) — nothing else may dequeue.
	void DrainInto(TFunctionRef<void(const FArcLoreNotificationEvent&)> Sink);

private:
	// C trampoline handed to lore. MUST be a free/static non-capturing function
	// (a lore callback is a plain C function pointer). Runs on a lore worker
	// thread; copies all needed strings immediately (valid only in-callback) and
	// only enqueues. Touches NO UObject/Slate.
	static void OnLoreEvent(const lore_event_t* Event, uint64 UserContext);

	// Many lore producer threads, one game-thread consumer.
	TQueue<FArcLoreNotificationEvent, EQueueMode::Mpsc> EventQueue;

	// Gates the lore-thread enqueue. Set true in Start() before subscribing, set
	// false in Stop() before the sync unsubscribe. seq_cst by default; the
	// load/store use acquire/release explicitly to pair the gate with the queue.
	std::atomic<bool> bEnqueueEnabled{ false };

	// Backing values for the per-call LOCAL FLoreGlobalArgsScope (the scope type
	// can't be held as a member — self-referential lore_string_t views).
	FString RepoRoot;
	FString Identity;
};
