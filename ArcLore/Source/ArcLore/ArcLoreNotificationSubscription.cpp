// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreNotificationSubscription.h"

#include "HAL/Platform.h"
#include "ISourceControlModule.h" // LogSourceControl

// Unique, file-scoped namespace for free helpers (project rule: cpp free
// functions live in a unique file-scoped namespace).
namespace ArcLoreNotificationSubscriptionInternal
{
	// No-op callback for the SYNC unsubscribe. The unsubscribe config is distinct
	// from the subscribe config, so events delivered to it (SUBSCRIBED handshake
	// echoes / COMPLETE / END) need no handling — we only need unsubscribe to run
	// and block. Must be a free non-capturing C function (plain function pointer).
	static void UnsubscribeNoopCallback(const lore_event_t* /*Event*/, uint64_t /*UserContext*/)
	{
	}
}

FArcLoreNotificationSubscription::FArcLoreNotificationSubscription(const FString& InRepoRoot, const FString& InIdentity)
	: RepoRoot(InRepoRoot)
	, Identity(InIdentity)
{
}

void FArcLoreNotificationSubscription::OnLoreEvent(const lore_event_t* Event, uint64 UserContext)
{
	if (Event == nullptr || UserContext == 0)
	{
		return;
	}

	FArcLoreNotificationSubscription* Sub =
		reinterpret_cast<FArcLoreNotificationSubscription*>(static_cast<UPTRINT>(UserContext));

	// Gate: if enqueue is disabled (Stop() ran, or Start() not yet), drop. Acquire
	// pairs with the release store in Start()/Stop() so a callback that observes
	// the flag also observes a live queue.
	if (!Sub->bEnqueueEnabled.load(std::memory_order_acquire))
	{
		return;
	}

	const uint32_t Tag = lore_event_type(Event);
	switch (Tag)
	{
	case LORE_EVENT_NOTIFICATION_BRANCH_PUSHED:
	{
		FArcLoreNotificationEvent Ev;
		Ev.Kind = EArcLoreNotificationKind::BranchPushed;
		Ev.UserId = ArcLoreApi::LoreStringToFString(Event->notification_branch_pushed.user_id);
		Sub->EventQueue.Enqueue(MoveTemp(Ev));
		break;
	}

	case LORE_EVENT_NOTIFICATION_BRANCH_CREATED:
	{
		// branch_created carries no user_id in the C ABI -> UserId stays empty.
		FArcLoreNotificationEvent Ev;
		Ev.Kind = EArcLoreNotificationKind::BranchCreated;
		Sub->EventQueue.Enqueue(MoveTemp(Ev));
		break;
	}

	case LORE_EVENT_NOTIFICATION_BRANCH_DELETED:
	{
		// branch_deleted carries no user_id in the C ABI -> UserId stays empty.
		FArcLoreNotificationEvent Ev;
		Ev.Kind = EArcLoreNotificationKind::BranchDeleted;
		Sub->EventQueue.Enqueue(MoveTemp(Ev));
		break;
	}

	case LORE_EVENT_NOTIFICATION_RESOURCE_LOCKED:
	{
		FArcLoreNotificationEvent Ev;
		Ev.Kind = EArcLoreNotificationKind::ResourceLocked;
		Ev.UserId = ArcLoreApi::LoreStringToFString(Event->notification_resource_locked.user_id);
		Sub->EventQueue.Enqueue(MoveTemp(Ev));
		break;
	}

	case LORE_EVENT_NOTIFICATION_RESOURCE_UNLOCKED:
	{
		FArcLoreNotificationEvent Ev;
		Ev.Kind = EArcLoreNotificationKind::ResourceUnlocked;
		Ev.UserId = ArcLoreApi::LoreStringToFString(Event->notification_resource_unlocked.user_id);
		Sub->EventQueue.Enqueue(MoveTemp(Ev));
		break;
	}

	case LORE_EVENT_NOTIFICATION_SUBSCRIBED:
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: notification stream SUBSCRIBED.");
		break;

	case LORE_EVENT_NOTIFICATION_UNSUBSCRIBED:
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: notification stream UNSUBSCRIBED.");
		break;

	case LORE_EVENT_ERROR:
		UE_LOGF(LogSourceControl, Warning,"ArcLore: notification stream LORE_EVENT_ERROR: %ls",
			*ArcLoreApi::LoreStringToFString(Event->error.error_inner));
		break;

	case LORE_EVENT_COMPLETE:
		if (Event->complete.status != 0)
		{
			UE_LOGF(LogSourceControl, Warning,"ArcLore: notification stream LORE_EVENT_COMPLETE status=%d",
				Event->complete.status);
		}
		break;

	default:
		// Other control events (END, LOG, etc.) need no handling here.
		break;
	}
}

void FArcLoreNotificationSubscription::Start()
{
	if (!ArcLoreApi::IsReady())
	{
		return;
	}

	// LOCAL scope: lore copies the args before the async call returns
	// (ArcLoreApi.h:56-59), so the scope only needs to outlive this call.
	ArcLoreApi::FLoreGlobalArgsScope Scope(RepoRoot, Identity);

	// Enable enqueue BEFORE subscribing, so no early callback is dropped. Release
	// pairs with the acquire load in OnLoreEvent.
	bEnqueueEnabled.store(true, std::memory_order_release);

	lore_notification_subscribe_args_t Args;
	FMemory::Memzero(Args);

	lore_event_callback_config_t Config;
	Config.func = &FArcLoreNotificationSubscription::OnLoreEvent;
	Config.user_context = static_cast<uint64_t>(reinterpret_cast<UPTRINT>(this));

	UE_LOGF(LogSourceControl, Verbose,"ArcLore: subscribing to notification stream for repo '%ls'.", *RepoRoot);
	lore_notification_subscribe_async(&Scope.Args(), &Args, Config);
}

void FArcLoreNotificationSubscription::Stop()
{
	if (!ArcLoreApi::IsReady())
	{
		return;
	}

	// Disable enqueue first: any callback that fires after this load returns is a
	// no-op. Release pairs with the acquire load in OnLoreEvent.
	bEnqueueEnabled.store(false, std::memory_order_release);

	// LOCAL scope: same copy-before-return contract as Start().
	ArcLoreApi::FLoreGlobalArgsScope Scope(RepoRoot, Identity);

	lore_notification_unsubscribe_args_t Args;
	FMemory::Memzero(Args);

	// Throwaway no-op config for the unsubscribe op itself (distinct from the
	// subscribe config). We only care that the call blocks.
	lore_event_callback_config_t Config;
	Config.func = &ArcLoreNotificationSubscriptionInternal::UnsubscribeNoopCallback;
	Config.user_context = 0;

	// SYNC unsubscribe: blocks until lore awaits its delivery task
	// (lore-revision notification.rs:205-206) -> on return, no subscribe-config
	// callback can fire.
	//
	// INVARIANT: this object and EventQueue MUST outlive this call (a callback may
	// still be mid-flight until lore's await completes). The owner destroys this
	// object only AFTER Stop() returns.
	UE_LOGF(LogSourceControl, Verbose,"ArcLore: unsubscribing from notification stream for repo '%ls'.", *RepoRoot);
	lore_notification_unsubscribe(&Scope.Args(), &Args, Config);
}

void FArcLoreNotificationSubscription::DrainInto(TFunctionRef<void(const FArcLoreNotificationEvent&)> Sink)
{
	// SOLE consumer of EventQueue (MPSC single-consumer contract). Must run on
	// the game thread; nothing else may dequeue.
	check(IsInGameThread());

	FArcLoreNotificationEvent Ev;
	while (EventQueue.Dequeue(Ev))
	{
		Sink(Ev);
	}
}
