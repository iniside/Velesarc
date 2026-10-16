// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Items/ArcItemId.h"
#include "ArcBagSyncSubsystem.generated.h"

struct FMassEntityManager;
class UArcMassItemEventSubsystem;

/**
 * UArcMassBagSyncSubsystem
 *
 * Keeps FArcMassBagStoreFragment slot arrays honest. Invariant:
 * a slot that held an item id is zeroed out the moment that item leaves the
 * store. The bag-assign/unassign commands maintain slots synchronously; this
 * subsystem evicts stale ids for every OTHER path that removes or destroys an
 * item (RemoveItem/DestroyItem/MoveItem cascades) by consuming
 * UArcMassItemEventSubsystem's OnItemRemoved rail. Re-entrancy is benign: when
 * a command already zeroed the slot, the drained event finds nothing to clear.
 *
 * It also forwards in-place stack changes to the bag rail: when an item already
 * sitting in a bag slot just changes its Stacks (same id stays slotted),
 * OnItemRemoved never fires, so OnItemStacksChanged re-broadcasts
 * QueueBagSlotsChanged for any bag holding that id — without mutating the
 * fragment — so the bag ViewModel refreshes its displayed stack count.
 */
UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassBagSyncSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool EnsureCached();

	void OnItemRemoved(FMassEntityHandle Entity, FArcItemId ItemId, const UScriptStruct* StoreType);

	void OnItemStacksChanged(FMassEntityHandle Entity, FArcItemId ItemId, int32 StackDelta, const UScriptStruct* StoreType);

	FMassEntityManager* EntityManager = nullptr;
	UArcMassItemEventSubsystem* EventSubsystem = nullptr;

	FDelegateHandle HandleItemRemoved;
	FDelegateHandle HandleItemStacksChanged;
};
