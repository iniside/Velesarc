// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "ArcMassEquipmentSyncSubsystem.generated.h"

struct FMassEntityManager;
class UArcMassItemEventSubsystem;

/**
 * UArcMassEquipmentSyncSubsystem
 *
 * Keeps FArcMassEquipmentFragment::EquippedItems honest. Invariant:
 * equipped => slotted. The equip/unequip commands maintain the map
 * synchronously; this subsystem evicts stale entries for every OTHER path
 * that removes or unslots an item (RemoveItem/DestroyItem/MoveItem cascades,
 * direct RemoveItemFromSlot calls) by consuming UArcMassItemEventSubsystem's
 * OnItemRemoved and OnItemSlotCleared rails. Re-entrancy is benign: when a
 * command already removed the entry, the drained event finds nothing.
 */
UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassEquipmentSyncSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool EnsureCached();

	/** Two-pass removal of every EquippedItems entry whose value equals ItemId. */
	void EvictItemFromEquipment(FMassEntityHandle Entity, FArcItemId ItemId);

	void OnItemRemoved(FMassEntityHandle Entity, FArcItemId ItemId, const UScriptStruct* StoreType);
	void OnItemSlotCleared(FMassEntityHandle Entity, FArcItemId ItemId, FGameplayTag OldSlot, const UScriptStruct* StoreType);

	FMassEntityManager* EntityManager = nullptr;
	UArcMassItemEventSubsystem* EventSubsystem = nullptr;

	FDelegateHandle HandleItemRemoved;
	FDelegateHandle HandleSlotCleared;
};
