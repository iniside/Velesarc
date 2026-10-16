// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Containers/Queue.h"
#include "Engine/World.h"
#include "Bags/ArcBagId.h"
#include "ArcBagEventSubsystem.generated.h"

// ---------------------------------------------------------------------------
// Plain-struct queue payloads (not USTRUCTs — worker-thread PODs only)
// ---------------------------------------------------------------------------

struct FArcQueuedBagListChange
{
	FMassEntityHandle Entity;
};

struct FArcQueuedBagSlotsChange
{
	FMassEntityHandle Entity;
	FArcBagId         BagId;
};

// ---------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------

/**
 * World-routed bag-change signal. Bag ops enqueue change notifications on the game
 * thread; the queues drain post-actor-tick onto game-thread delegates. Keyed on
 * FMassEntityHandle only — there is one bag fragment per entity (no StoreType).
 *
 * Two granularities map cleanly onto the 2-level bag ViewModel:
 *   - OnBagListChanged(Entity)        — the set/order of bags changed (add / remove / reorder).
 *   - OnBagSlotsChanged(Entity, BagId)— one bag's slot contents changed (place / clear / move / scrub).
 */
UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassBagEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// -- Typed delegate signatures ------------------------------------------

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBagListChanged, FMassEntityHandle /*Entity*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBagSlotsChanged, FMassEntityHandle /*Entity*/, FArcBagId /*BagId*/);

	// -- Global multicasts (fired for every drained event) ------------------

	FOnBagListChanged  OnBagListChanged;
	FOnBagSlotsChanged OnBagSlotsChanged;

	// -- Per-entity bind / unbind API ---------------------------------------
	//    Returns a FDelegateHandle the caller must store to unbind.
	//    Call RemoveForEntity to unbind — it searches both per-entity maps.

	FDelegateHandle AddBagListChangedForEntity(FMassEntityHandle Entity, FOnBagListChanged::FDelegate Delegate);
	FDelegateHandle AddBagSlotsChangedForEntity(FMassEntityHandle Entity, FOnBagSlotsChanged::FDelegate Delegate);

	/** Removes the binding identified by Handle from whichever per-entity multicast owns it.
	 *  Walks both per-entity maps; prunes empty map entries. */
	void RemoveForEntity(FMassEntityHandle Entity, FDelegateHandle Handle);

	// -- Producer API (game thread today; MPSC-safe by construction) --------

	void QueueBagListChanged(FMassEntityHandle Entity);
	void QueueBagSlotsChanged(FMassEntityHandle Entity, FArcBagId BagId);

	// -- USubsystem ---------------------------------------------------------

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Drain all queued events immediately and fire delegates.
	 *  Intended for test code only — production code relies on the automatic
	 *  post-actor-tick drain registered in Initialize(). */
	void DrainQueues();

	/** Called each post-actor-tick to drain queued events onto game-thread delegates.
	 *  Public so ArcEventSubsystemCore::BindPostActorTickDrain can form a member pointer
	 *  from outside this translation unit. */
	void DrainOnPostActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);

	/** Remove all per-entity delegate entries for Entity. */
	void EvictEntity(FMassEntityHandle Entity);

private:
	// MPSC queues — enqueued by producers, drained on the game thread.
	TQueue<FArcQueuedBagListChange,  EQueueMode::Mpsc> BagListChangeQueue;
	TQueue<FArcQueuedBagSlotsChange, EQueueMode::Mpsc> BagSlotsChangeQueue;

	// Per-entity dispatch maps — keyed on FMassEntityHandle (game-thread only).
	TMap<FMassEntityHandle, FOnBagListChanged>  PerEntityBagListChanged;
	TMap<FMassEntityHandle, FOnBagSlotsChanged> PerEntityBagSlotsChanged;

	/** Handle returned by FWorldDelegates::OnWorldPostActorTick.Add(...). */
	FDelegateHandle PostActorTickHandle;
};
