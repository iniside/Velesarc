// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcAreaTypes.h"
#include "ArcAreaSlotDefinition.h"
#include "ArcMacroDefines.h"
#include "ArcAreaSubsystem.generated.h"

class UArcAreaDefinition;

/** Broadcast when any slot changes state. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcAreaSlotStateChanged,
	FArcAreaHandle /*AreaHandle*/,
	int32 /*SlotIndex*/,
	EArcAreaSlotState /*NewState*/);

/**
 * Central authority for area data and NPC slot assignments.
 *
 * All assignment state lives here — Mass fragments on NPCs are lightweight caches.
 * Event-driven (not tickable): state changes happen through explicit API calls.
 *
 * Slot state changes are broadcast via OnSlotStateChanged delegate.
 * External listeners (e.g., UArcAreaAutoVacancyListener, future town manager)
 * decide whether to post advertisements to ArcKnowledge.
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
	 * Register an area from its definition.
	 * Called by UArcAreaAddObserver when a SmartObject Mass entity with UArcAreaTrait spawns.
	 * Broadcasts OnSlotStateChanged for each initial Vacant slot.
	 */
	FArcAreaHandle RegisterArea(
		const UArcAreaDefinition* Definition,
		FVector Location,
		FSmartObjectHandle SOHandle,
		FMassEntityHandle OwnerEntity);

	/** Unregister an area. Unassigns all NPCs, broadcasts Disabled for each slot. */
	void UnregisterArea(FArcAreaHandle Handle);

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
	bool AssignToSlot(FArcAreaHandle AreaHandle, int32 SlotIndex, FMassEntityHandle Entity);

	/** Unassign an NPC from a specific slot. Broadcasts OnSlotStateChanged (→ Vacant). */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Assignment")
	bool UnassignFromSlot(FArcAreaHandle AreaHandle, int32 SlotIndex);

	/** Find and remove an entity from wherever it's assigned. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Assignment")
	bool UnassignEntity(FMassEntityHandle Entity);

	/** Find which slot index an entity occupies in the given area (INDEX_NONE if not found). */
	int32 FindSlotForEntity(FArcAreaHandle AreaHandle, FMassEntityHandle Entity) const;

	// ====================================================================
	// SmartObject Bridge (notification-based)
	// ====================================================================

	/** Notify that an assigned NPC has claimed the SmartObject slot (Assigned → Active). */
	void NotifySlotClaimed(FArcAreaHandle AreaHandle, int32 SlotIndex);

	/** Notify that an NPC released the SmartObject claim (Active → Assigned). */
	void NotifySlotReleased(FArcAreaHandle AreaHandle, int32 SlotIndex);

	// ====================================================================
	// Slot Management
	// ====================================================================

	/** Disable a slot — prevents assignment. Broadcasts OnSlotStateChanged (→ Disabled). */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Slots")
	void DisableSlot(FArcAreaHandle AreaHandle, int32 SlotIndex);

	/** Re-enable a disabled slot — returns it to Vacant state. Broadcasts OnSlotStateChanged (→ Vacant). */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Slots")
	void EnableSlot(FArcAreaHandle AreaHandle, int32 SlotIndex);

	// ====================================================================
	// Queries
	// ====================================================================

	/** Get full area data (nullptr if not found). */
	const FArcAreaData* GetAreaData(FArcAreaHandle Handle) const;

	/** Get the current state of a specific slot. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	EArcAreaSlotState GetSlotState(FArcAreaHandle Handle, int32 SlotIndex) const;

	/** Find all areas matching a tag query. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	TArray<FArcAreaHandle> FindAreasByTags(const FGameplayTagQuery& TagQuery) const;

	/** Whether any slot in this area is vacant. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	bool HasVacancy(FArcAreaHandle Handle) const;

	/** Get indices of all vacant slots in an area. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	TArray<int32> GetVacantSlotIndices(FArcAreaHandle Handle) const;

	/** Get all registered areas. */
	const TMap<FArcAreaHandle, FArcAreaData>& GetAllAreas() const { return Areas; }

private:
	void UpdateNPCAssignmentFragment(FMassEntityHandle Entity, FArcAreaHandle AreaHandle, int32 SlotIndex, FGameplayTag RoleTag);
	void ClearNPCAssignmentFragment(FMassEntityHandle Entity);

	// ---------- Storage ----------

	/** All areas, keyed by handle. */
	TMap<FArcAreaHandle, FArcAreaData> Areas;

	/** Reverse index: entity → area slot (for fast UnassignEntity lookup). */
	TMap<FMassEntityHandle, FArcAreaSlotHandle> EntityAssignmentIndex;
};
