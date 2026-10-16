// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcAreaTypes.h"
#include "ArcMacroDefines.h"
#include "ArcAreaSubsystem.generated.h"

class UArcAreaDefinition;

/** Broadcast when any slot changes state. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcAreaSlotStateChanged,
	const FSmartObjectSlotHandle& /*SlotHandle*/,
	EArcAreaSlotState /*NewState*/);

/**
 * Central authority for area data and NPC slot assignments.
 *
 * All assignment state lives here — Mass fragments on NPCs are lightweight caches.
 * Event-driven (not tickable): state changes happen through explicit API calls.
 *
 * Slot state changes are broadcast via OnSlotStateChanged delegate.
 */
UCLASS()
class ARCAREA_API UArcAreaSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ---------- USubsystem ----------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ====================================================================
	// Events
	// ====================================================================

	DEFINE_ARC_DELEGATE(FArcAreaSlotStateChanged, OnSlotStateChanged);

public:
	// ====================================================================
	// Area Registration (called by observer)
	// ====================================================================

	/**
	 * Register an area from its definition, keyed by the SmartObject it owns.
	 * Called by UArcAreaAddObserver when a SmartObject Mass entity with UArcAreaTrait spawns.
	 * One runtime slot is built per SmartObject slot; broadcasts OnSlotStateChanged Vacant for each.
	 */
	bool RegisterArea(
		const UArcAreaDefinition* Definition,
		FVector Location,
		FSmartObjectHandle SOHandle,
		FMassEntityHandle OwnerEntity);

	/** Unregister an area. Unassigns all NPCs, broadcasts Disabled for each slot. */
	void UnregisterArea(FSmartObjectHandle Handle);

	// ====================================================================
	// Assignment (single source of truth)
	// ====================================================================

	/**
	 * Assign an NPC entity to a specific slot.
	 * Validates slot is Vacant and NPC meets requirements.
	 * Updates both subsystem state and NPC's FArcAreaAssignmentFragment via deferred commands.
	 * Broadcasts OnSlotStateChanged (Vacant → Assigned).
	 */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Assignment")
	bool AssignToSlot(FSmartObjectSlotHandle SlotHandle, FMassEntityHandle Entity);

	/** Unassign an NPC from a specific slot. Broadcasts OnSlotStateChanged (→ Vacant). */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Assignment")
	bool UnassignFromSlot(FSmartObjectSlotHandle SlotHandle);

	/** Find and remove an entity from wherever it's assigned. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Assignment")
	bool UnassignEntity(FMassEntityHandle Entity);

	/** Find which slot an entity occupies in the given area (invalid handle if not found). */
	FSmartObjectSlotHandle FindSlotForEntity(FSmartObjectHandle AreaHandle, FMassEntityHandle Entity) const;

	// ====================================================================
	// SmartObject Bridge (notification-based)
	// ====================================================================

	/** Notify that an assigned NPC has claimed the SmartObject slot (Assigned → Active). */
	void NotifySlotClaimed(FSmartObjectSlotHandle SlotHandle);

	/** Notify that an NPC released the SmartObject claim (Active → Assigned). */
	void NotifySlotReleased(FSmartObjectSlotHandle SlotHandle);

	// ====================================================================
	// Slot Management
	// ====================================================================

	/** Disable a slot — prevents assignment. Broadcasts OnSlotStateChanged (→ Disabled). */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Slots")
	void DisableSlot(FSmartObjectSlotHandle SlotHandle);

	/** Re-enable a disabled slot — returns it to Vacant state. Broadcasts OnSlotStateChanged (→ Vacant). */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Slots")
	void EnableSlot(FSmartObjectSlotHandle SlotHandle);

	// ====================================================================
	// Queries
	// ====================================================================

	/** Get full area data (nullptr if not found). */
	const FArcAreaData* GetAreaData(FSmartObjectHandle Handle) const;

	/** Get the current state of a specific slot. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	EArcAreaSlotState GetSlotState(FSmartObjectSlotHandle SlotHandle) const;

	/** Find all areas matching a tag query. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	TArray<FSmartObjectHandle> FindAreasByTags(const FGameplayTagQuery& TagQuery) const;

	/** Get all vacant slots in an area. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	TArray<FSmartObjectSlotHandle> GetVacantSlots(FSmartObjectHandle Handle) const;

	/** Get all registered areas. */
	const TMap<FSmartObjectHandle, FArcAreaData>& GetAllAreas() const { return Areas; }

private:
	void UpdateNPCAssignmentFragment(FMassEntityHandle Entity, FSmartObjectSlotHandle SlotHandle);
	void ClearNPCAssignmentFragment(FMassEntityHandle Entity);

	// ---------- Storage ----------

	TMap<FSmartObjectHandle, FArcAreaData> Areas;

	/** Reverse index: entity → area slot (for fast UnassignEntity lookup). */
	TMap<FMassEntityHandle, FSmartObjectSlotHandle> EntityAssignmentIndex;
};
