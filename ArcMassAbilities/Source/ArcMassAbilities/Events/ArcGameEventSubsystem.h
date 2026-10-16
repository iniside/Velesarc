// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HAL/CriticalSection.h"
#include "Engine/World.h"
#include "Attributes/ArcAttribute.h"
#include "Events/ArcGameEvent.h"
#include "ArcGameEventSubsystem.generated.h"

DECLARE_DELEGATE_OneParam(FArcGameEventDelegate, const FArcGameEvent&);

UCLASS()
class ARCMASSABILITIES_API UArcGameEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Queue(FArcGameEvent&& Event);

	FDelegateHandle Subscribe(FGameplayTag Tag, FMassEntityHandle Entity, EArcEventRole Role, FArcGameEventDelegate Delegate);

	/** ArcEvent.Attribute.Changed of one attribute on Entity; changes are coalesced per drain. */
	FDelegateHandle Subscribe(FMassEntityHandle Entity, const FArcAttributeRef& Attribute, FArcGameEventDelegate Delegate);

	/** ArcEvent.Tag.Added or ArcEvent.Tag.Removed (Tag) for exactly ChangedTag on Entity. */
	FDelegateHandle Subscribe(FGameplayTag Tag, FMassEntityHandle Entity, FGameplayTag ChangedTag, FArcGameEventDelegate Delegate);

	void Unsubscribe(FDelegateHandle Handle);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void DrainOnPostActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);

private:
	struct FChannelKey
	{
		FGameplayTag Tag;
		FGameplayTag ChangedTag;
		FArcAttributeRef Attribute;

		bool operator==(const FChannelKey& Other) const
		{
			return Tag == Other.Tag && ChangedTag == Other.ChangedTag && Attribute == Other.Attribute;
		}

		friend uint32 GetTypeHash(const FChannelKey& Key)
		{
			return HashCombine(HashCombine(GetTypeHash(Key.Tag), GetTypeHash(Key.ChangedTag)), GetTypeHash(Key.Attribute));
		}
	};

	struct FAttributeKey
	{
		FMassEntityHandle Entity;
		FArcAttributeRef Attribute;

		bool operator==(const FAttributeKey& Other) const
		{
			return Entity == Other.Entity && Attribute == Other.Attribute;
		}

		friend uint32 GetTypeHash(const FAttributeKey& Key)
		{
			return HashCombine(GetTypeHash(Key.Entity), GetTypeHash(Key.Attribute));
		}
	};

	struct FSubscription
	{
		FDelegateHandle Handle;
		FChannelKey Key;
		FMassEntityHandle Entity;
		EArcEventRole Role = EArcEventRole::Any;
		FArcGameEventDelegate Delegate;
		bool bRemoved = false;
	};

	struct FSubscriptionLocation
	{
		FMassEntityHandle Entity;
		FChannelKey Key;
	};

	using FChannelSubscriptions = TMap<FChannelKey, TArray<FSubscription>>;

	FDelegateHandle AddNewSubscription(FChannelKey&& Key, FMassEntityHandle Entity, EArcEventRole Role, FArcGameEventDelegate&& Delegate);
	void DrainQueue();
	void Dispatch(const FArcGameEvent& Event);
	void DispatchToEntity(const FArcGameEvent& Event, FMassEntityHandle Entity, const FChannelKey& Key, const FChannelKey* FilteredKey);
	void AddSubscription(FSubscription&& Subscription);
	TArray<FSubscription>* FindSubscriptionArray(const FSubscriptionLocation& Location);
	void RemoveSubscription(FDelegateHandle Handle, const FSubscriptionLocation& Location);
	void EvictEntity(FMassEntityHandle Entity);
	void EvictInvalidEntities();

	FCriticalSection QueueLock;
	TArray<FArcGameEvent> QueuedEvents;
	TMap<FAttributeKey, int32> CoalescedAttributeEvents;

	TMap<FMassEntityHandle, FChannelSubscriptions> PerEntitySubscriptions;
	FChannelSubscriptions GlobalSubscriptions;
	TMap<FDelegateHandle, FSubscriptionLocation> HandleLocations;

	TArray<FSubscription> PendingSubscriptions;
	TArray<TPair<FDelegateHandle, FSubscriptionLocation>> PendingRemovals;
	TSet<FMassEntityHandle> TouchedEntities;
	TArray<FMassEntityHandle> SweepEntities;

	bool bDispatching = false;

	FDelegateHandle PostActorTickHandle;
};
