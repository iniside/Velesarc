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
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcAreaSlotStateChanged,
	const FArcAreaSlotHandle& /*SlotHandle*/,
	EArcAreaSlotState /*NewState*/);

/** Per-entity delegates for area assignment changes. */
struct FArcAreaEntityDelegates
{
	FSimpleMulticastDelegate OnAssigned;
	FSimpleMulticastDelegate OnUnassigned;
};

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
	bool AssignToSlot(FArcAreaSlotHandle SlotHandle, FMassEntityHandle Entity);

	/** Unassign an NPC from a specific slot. Broadcasts OnSlotStateChanged (→ Vacant). */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Assignment")
	bool UnassignFromSlot(FArcAreaSlotHandle SlotHandle);

	/** Find and remove an entity from wherever it's assigned. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Assignment")
	bool UnassignEntity(FMassEntityHandle Entity);

	// ====================================================================
	// Per-Entity Assignment Delegates
	// ====================================================================

	/** Get or create the delegate pair for an entity. Use to bind listeners. */
	FArcAreaEntityDelegates& GetOrCreateEntityDelegates(FMassEntityHandle Entity);

	/** Find existing delegate pair for an entity. Returns nullptr if none. Use for unbind/cleanup. */
	FArcAreaEntityDelegates* FindEntityDelegates(FMassEntityHandle Entity);

	/** Remove the delegate entry for an entity. Called on entity cleanup. */
	void RemoveEntityDelegates(FMassEntityHandle Entity);

	/** Find which slot an entity occupies in the given area (invalid handle if not found). */
	FArcAreaSlotHandle FindSlotForEntity(FArcAreaHandle AreaHandle, FMassEntityHandle Entity) const;

	// ====================================================================
	// SmartObject Bridge (notification-based)
	// ====================================================================

	/** Notify that an assigned NPC has claimed the SmartObject slot (Assigned → Active). */
	void NotifySlotClaimed(FArcAreaSlotHandle SlotHandle);

	/** Notify that an NPC released the SmartObject claim (Active → Assigned). */
	void NotifySlotReleased(FArcAreaSlotHandle SlotHandle);

	// ====================================================================
	// Slot Management
	// ====================================================================

	/** Disable a slot — prevents assignment. Broadcasts OnSlotStateChanged (→ Disabled). */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Slots")
	void DisableSlot(FArcAreaSlotHandle SlotHandle);

	/** Re-enable a disabled slot — returns it to Vacant state. Broadcasts OnSlotStateChanged (→ Vacant). */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Slots")
	void EnableSlot(FArcAreaSlotHandle SlotHandle);

	// ====================================================================
	// Queries
	// ====================================================================

	/** Get full area data (nullptr if not found). */
	const FArcAreaData* GetAreaData(FArcAreaHandle Handle) const;

	/** Get the current state of a specific slot. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	EArcAreaSlotState GetSlotState(FArcAreaSlotHandle SlotHandle) const;

	/** Find all areas matching a tag query. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	TArray<FArcAreaHandle> FindAreasByTags(const FGameplayTagQuery& TagQuery) const;

	/** Whether any slot in this area is vacant. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	bool HasVacancy(FArcAreaHandle Handle) const;

	/** Get all vacant slots in an area. */
	UFUNCTION(BlueprintCallable, Category = "ArcArea|Queries")
	TArray<FArcAreaSlotHandle> GetVacantSlots(FArcAreaHandle Handle) const;

	/** Get all registered areas. */
	const TMap<FArcAreaHandle, FArcAreaData>& GetAllAreas() const { return Areas; }

private:
	void UpdateNPCAssignmentFragment(FMassEntityHandle Entity, FArcAreaSlotHandle SlotHandle);
	void ClearNPCAssignmentFragment(FMassEntityHandle Entity);

	// ---------- Storage ----------

	/** All areas, keyed by handle. */
	TMap<FArcAreaHandle, FArcAreaData> Areas;

	/** Reverse index: entity → area slot (for fast UnassignEntity lookup). */
	TMap<FMassEntityHandle, FArcAreaSlotHandle> EntityAssignmentIndex;

	/** Per-entity delegates for assignment/unassignment notifications. */
	TMap<FMassEntityHandle, FArcAreaEntityDelegates> EntityAssignmentDelegates;
};
