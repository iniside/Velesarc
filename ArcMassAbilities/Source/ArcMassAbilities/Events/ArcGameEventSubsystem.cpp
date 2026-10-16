// Copyright Lukasz Baran. All Rights Reserved.

#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcEventSubsystemCore.h"
#include "Events/ArcGameEventTags.h"
#include "Events/ArcAttributeEventPayloads.h"
#include "Events/ArcTagEventPayloads.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityManager.h"
#include "Misc/ScopeLock.h"

namespace ArcGameEventSubsystemPrivate
{
	constexpr int32 SweepBudgetPerDrain = 16;

	bool IsTagChannel(FGameplayTag Tag)
	{
		return Tag == ArcGameEventTags::TagAdded.GetTag() || Tag == ArcGameEventTags::TagRemoved.GetTag();
	}
}

void UArcGameEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PostActorTickHandle = ArcEventSubsystemCore::BindPostActorTickDrain(this);
}

void UArcGameEventSubsystem::Deinitialize()
{
	ArcEventSubsystemCore::UnbindPostActorTickDrain(PostActorTickHandle);

	{
		FScopeLock Lock(&QueueLock);
		QueuedEvents.Empty();
		CoalescedAttributeEvents.Empty();
	}

	PerEntitySubscriptions.Empty();
	GlobalSubscriptions.Empty();
	HandleLocations.Empty();
	PendingSubscriptions.Empty();
	PendingRemovals.Empty();
	TouchedEntities.Empty();
	SweepEntities.Empty();

	Super::Deinitialize();
}

void UArcGameEventSubsystem::Queue(FArcGameEvent&& Event)
{
	FScopeLock Lock(&QueueLock);

	if (Event.Tag == ArcGameEventTags::AttributeChanged.GetTag())
	{
		if (const FArcAttributeChangedPayload* Incoming = Event.Payload.GetPtr<FArcAttributeChangedPayload>())
		{
			const FAttributeKey Key{Event.Target, Incoming->Attribute};
			if (const int32* QueuedIndex = CoalescedAttributeEvents.Find(Key))
			{
				FArcGameEvent& Queued = QueuedEvents[*QueuedIndex];
				FArcAttributeChangedPayload& Merged = Queued.Payload.GetMutable<FArcAttributeChangedPayload>();
				Merged.NewValue = Incoming->NewValue;
				Queued.Magnitude = Merged.NewValue - Merged.OldValue;
				Queued.SourceTags = MoveTemp(Event.SourceTags);
				Queued.TargetTags = MoveTemp(Event.TargetTags);
				return;
			}
			CoalescedAttributeEvents.Add(Key, QueuedEvents.Num());
		}
	}

	QueuedEvents.Add(MoveTemp(Event));
}

FDelegateHandle UArcGameEventSubsystem::Subscribe(FGameplayTag Tag, FMassEntityHandle Entity, EArcEventRole Role, FArcGameEventDelegate Delegate)
{
	FChannelKey Key;
	Key.Tag = Tag;
	return AddNewSubscription(MoveTemp(Key), Entity, Role, MoveTemp(Delegate));
}

FDelegateHandle UArcGameEventSubsystem::Subscribe(FMassEntityHandle Entity, const FArcAttributeRef& Attribute, FArcGameEventDelegate Delegate)
{
	if (!ensureMsgf(Entity.IsSet(), TEXT("Attribute subscriptions are per entity")))
	{
		return FDelegateHandle();
	}

	FChannelKey Key;
	Key.Tag = ArcGameEventTags::AttributeChanged;
	Key.Attribute = Attribute;
	return AddNewSubscription(MoveTemp(Key), Entity, EArcEventRole::Any, MoveTemp(Delegate));
}

FDelegateHandle UArcGameEventSubsystem::Subscribe(FGameplayTag Tag, FMassEntityHandle Entity, FGameplayTag ChangedTag, FArcGameEventDelegate Delegate)
{
	if (!ensureMsgf(Entity.IsSet() && ArcGameEventSubsystemPrivate::IsTagChannel(Tag) && ChangedTag.IsValid(),
		TEXT("Tag change subscriptions need an entity, ArcEvent.Tag.Added or ArcEvent.Tag.Removed, and a tag")))
	{
		return FDelegateHandle();
	}

	FChannelKey Key;
	Key.Tag = Tag;
	Key.ChangedTag = ChangedTag;
	return AddNewSubscription(MoveTemp(Key), Entity, EArcEventRole::Any, MoveTemp(Delegate));
}

FDelegateHandle UArcGameEventSubsystem::AddNewSubscription(FChannelKey&& Key, FMassEntityHandle Entity, EArcEventRole Role, FArcGameEventDelegate&& Delegate)
{
	check(IsInGameThread());

	FSubscription Subscription;
	Subscription.Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
	Subscription.Key = MoveTemp(Key);
	Subscription.Entity = Entity;
	Subscription.Role = Role;
	Subscription.Delegate = MoveTemp(Delegate);

	const FDelegateHandle Handle = Subscription.Handle;
	HandleLocations.Add(Handle, FSubscriptionLocation{Entity, Subscription.Key});

	if (bDispatching)
	{
		PendingSubscriptions.Add(MoveTemp(Subscription));
	}
	else
	{
		AddSubscription(MoveTemp(Subscription));
	}

	return Handle;
}

void UArcGameEventSubsystem::Unsubscribe(FDelegateHandle Handle)
{
	check(IsInGameThread());

	FSubscriptionLocation Location;
	if (!HandleLocations.RemoveAndCopyValue(Handle, Location))
	{
		return;
	}

	if (!bDispatching)
	{
		RemoveSubscription(Handle, Location);
		return;
	}

	const int32 PendingIndex = PendingSubscriptions.IndexOfByPredicate(
		[Handle](const FSubscription& Subscription) { return Subscription.Handle == Handle; });
	if (PendingIndex != INDEX_NONE)
	{
		PendingSubscriptions.RemoveAtSwap(PendingIndex);
		return;
	}

	if (TArray<FSubscription>* Subscriptions = FindSubscriptionArray(Location))
	{
		for (FSubscription& Subscription : *Subscriptions)
		{
			if (Subscription.Handle == Handle)
			{
				Subscription.bRemoved = true;
				break;
			}
		}
	}
	PendingRemovals.Emplace(Handle, Location);
}

void UArcGameEventSubsystem::DrainOnPostActorTick(UWorld* InWorld, ELevelTick /*TickType*/, float /*DeltaSeconds*/)
{
	if (InWorld != GetWorld())
	{
		return;
	}

	DrainQueue();
}

void UArcGameEventSubsystem::DrainQueue()
{
	check(IsInGameThread());

	if (bDispatching)
	{
		return;
	}

	// Taking the queue whole means events queued by subscribers during this drain wait for the next drain.
	TArray<FArcGameEvent> Events;
	{
		FScopeLock Lock(&QueueLock);
		Events = MoveTemp(QueuedEvents);
		QueuedEvents.Reset();
		CoalescedAttributeEvents.Reset();
	}

	bDispatching = true;
	for (const FArcGameEvent& Event : Events)
	{
		Dispatch(Event);
	}
	bDispatching = false;

	for (const TPair<FDelegateHandle, FSubscriptionLocation>& Removal : PendingRemovals)
	{
		RemoveSubscription(Removal.Key, Removal.Value);
	}
	PendingRemovals.Reset();

	for (FSubscription& Subscription : PendingSubscriptions)
	{
		AddSubscription(MoveTemp(Subscription));
	}
	PendingSubscriptions.Reset();

	EvictInvalidEntities();
}

void UArcGameEventSubsystem::Dispatch(const FArcGameEvent& Event)
{
	FChannelKey Key;
	Key.Tag = Event.Tag;

	FChannelKey FilteredKey;
	const FChannelKey* FilteredKeyPtr = nullptr;
	if (Event.Tag == ArcGameEventTags::AttributeChanged.GetTag())
	{
		if (const FArcAttributeChangedPayload* Payload = Event.Payload.GetPtr<FArcAttributeChangedPayload>())
		{
			FilteredKey.Tag = Event.Tag;
			FilteredKey.Attribute = Payload->Attribute;
			FilteredKeyPtr = &FilteredKey;
		}
	}
	else if (ArcGameEventSubsystemPrivate::IsTagChannel(Event.Tag))
	{
		if (const FArcTagChangedPayload* Payload = Event.Payload.GetPtr<FArcTagChangedPayload>())
		{
			FilteredKey.Tag = Event.Tag;
			FilteredKey.ChangedTag = Payload->Tag;
			FilteredKeyPtr = &FilteredKey;
		}
	}

	if (Event.Instigator.IsSet())
	{
		DispatchToEntity(Event, Event.Instigator, Key, FilteredKeyPtr);
	}
	if (Event.Target.IsSet() && Event.Target != Event.Instigator)
	{
		DispatchToEntity(Event, Event.Target, Key, FilteredKeyPtr);
	}

	if (TArray<FSubscription>* Subscriptions = GlobalSubscriptions.Find(Key))
	{
		for (FSubscription& Subscription : *Subscriptions)
		{
			if (!Subscription.bRemoved)
			{
				Subscription.Delegate.ExecuteIfBound(Event);
			}
		}
	}
}

void UArcGameEventSubsystem::DispatchToEntity(const FArcGameEvent& Event, FMassEntityHandle Entity, const FChannelKey& Key,
                                              const FChannelKey* FilteredKey)
{
	FChannelSubscriptions* Channels = PerEntitySubscriptions.Find(Entity);
	if (Channels == nullptr)
	{
		return;
	}

	TouchedEntities.Add(Entity);

	const bool bIsInstigator = Event.Instigator == Entity;
	const bool bIsTarget = Event.Target == Entity;

	TArray<FSubscription>* Lists[2] = {
		Channels->Find(Key),
		FilteredKey != nullptr ? Channels->Find(*FilteredKey) : nullptr
	};

	for (TArray<FSubscription>* Subscriptions : Lists)
	{
		if (Subscriptions == nullptr)
		{
			continue;
		}

		for (FSubscription& Subscription : *Subscriptions)
		{
			if (Subscription.bRemoved)
			{
				continue;
			}

			const bool bRoleMatches = Subscription.Role == EArcEventRole::Any
				|| (Subscription.Role == EArcEventRole::Instigator && bIsInstigator)
				|| (Subscription.Role == EArcEventRole::Target && bIsTarget);
			if (bRoleMatches)
			{
				Subscription.Delegate.ExecuteIfBound(Event);
			}
		}
	}
}

void UArcGameEventSubsystem::AddSubscription(FSubscription&& Subscription)
{
	if (Subscription.Entity.IsSet())
	{
		PerEntitySubscriptions.FindOrAdd(Subscription.Entity).FindOrAdd(Subscription.Key).Add(MoveTemp(Subscription));
	}
	else
	{
		GlobalSubscriptions.FindOrAdd(Subscription.Key).Add(MoveTemp(Subscription));
	}
}

TArray<UArcGameEventSubsystem::FSubscription>* UArcGameEventSubsystem::FindSubscriptionArray(const FSubscriptionLocation& Location)
{
	if (Location.Entity.IsSet())
	{
		FChannelSubscriptions* Channels = PerEntitySubscriptions.Find(Location.Entity);
		return Channels != nullptr ? Channels->Find(Location.Key) : nullptr;
	}
	return GlobalSubscriptions.Find(Location.Key);
}

void UArcGameEventSubsystem::RemoveSubscription(FDelegateHandle Handle, const FSubscriptionLocation& Location)
{
	TArray<FSubscription>* Subscriptions = FindSubscriptionArray(Location);
	if (Subscriptions == nullptr)
	{
		return;
	}

	Subscriptions->RemoveAllSwap([Handle](const FSubscription& Subscription) { return Subscription.Handle == Handle; });
	if (!Subscriptions->IsEmpty())
	{
		return;
	}

	if (!Location.Entity.IsSet())
	{
		GlobalSubscriptions.Remove(Location.Key);
		return;
	}

	FChannelSubscriptions& Channels = PerEntitySubscriptions.FindChecked(Location.Entity);
	Channels.Remove(Location.Key);
	if (Channels.IsEmpty())
	{
		PerEntitySubscriptions.Remove(Location.Entity);
	}
}

void UArcGameEventSubsystem::EvictEntity(FMassEntityHandle Entity)
{
	FChannelSubscriptions Evicted;
	if (!PerEntitySubscriptions.RemoveAndCopyValue(Entity, Evicted))
	{
		return;
	}

	for (const TPair<FChannelKey, TArray<FSubscription>>& Channel : Evicted)
	{
		for (const FSubscription& Subscription : Channel.Value)
		{
			HandleLocations.Remove(Subscription.Handle);
		}
	}
}

void UArcGameEventSubsystem::EvictInvalidEntities()
{
	const UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld());
	if (EntitySubsystem == nullptr)
	{
		TouchedEntities.Reset();
		return;
	}
	const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();

	for (const FMassEntityHandle& Entity : TouchedEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			EvictEntity(Entity);
		}
	}
	TouchedEntities.Reset();

	if (SweepEntities.IsEmpty())
	{
		PerEntitySubscriptions.GenerateKeyArray(SweepEntities);
	}

	const int32 Budget = FMath::Min(ArcGameEventSubsystemPrivate::SweepBudgetPerDrain, SweepEntities.Num());
	for (int32 Step = 0; Step < Budget; ++Step)
	{
		const FMassEntityHandle Entity = SweepEntities.Pop(EAllowShrinking::No);
		if (!EntityManager.IsEntityValid(Entity))
		{
			EvictEntity(Entity);
		}
	}
}
