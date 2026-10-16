// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Items/ArcItemId.h"
#include "Bags/ArcBagId.h"

struct FMassEntityManager;
struct FArcMassBagStoreFragment;
struct FArcBagInstance;
struct FArcItemData;
struct FArcItemSpec;
class UArcBagDefinition;
struct FArcItemDefinitionData;
class UScriptStruct;

/**
 * Free-function operations over the bag overlay (FArcMassBagStoreFragment). Each
 * mutating op resolves the bag fragment from the entity, validates ids/indices
 * against the bag's live Slots.Num() (never the definition's SlotCount), and marks
 * affected bag instances dirty. Slot↔item mutations route through the item-store
 * operations (ArcMassItems::ModifyStacks / RemoveItem) so item lifecycle, events,
 * and dirty marking stay centralised — bag ops never poke FArcItemData stacks.
 */
namespace ArcMassItems::Bags
{
	/** Append a new bag instance sized to Def->SlotCount; returns its stable BagId (none on failure). */
	ARCMASSITEMSRUNTIME_API FArcBagId AddBag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UArcBagDefinition* Def);

	/** Remove a bag — fails if any slot is occupied. */
	ARCMASSITEMSRUNTIME_API bool RemoveBag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId);

	/** Place a resident item into an empty in-range slot it is accepted by; fail-closed if the def can't resolve. */
	ARCMASSITEMSRUNTIME_API bool PlaceItemInSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId, int32 SlotIndex, FArcItemId ItemId);

	/** Clear a slot's item reference only — the item itself stays in the store. */
	ARCMASSITEMSRUNTIME_API bool ClearSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId, int32 SlotIndex);

	/** Move/merge/swap an item between two bag slots (same entity overlay). See .cpp for the merge/swap policy. */
	ARCMASSITEMSRUNTIME_API bool MoveItemBetweenSlots(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId FromBag, int32 FromSlot, FArcBagId ToBag, int32 ToSlot);

	/** Reorder a bag's display position; the matching instance moves to the clamped NewDisplayIndex (BagId stays stable). */
	ARCMASSITEMSRUNTIME_API bool ReorderBag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId, int32 NewDisplayIndex);

	/** Grow Slots to Def.SlotCount; shrink only if the tail beyond SlotCount is entirely empty (never drops an occupied slot). */
	ARCMASSITEMSRUNTIME_API void ReconcileBagSlots(FArcBagInstance& Bag, const UArcBagDefinition& Def);

	// --- Queries ---

	ARCMASSITEMSRUNTIME_API FArcBagInstance* FindBag(FArcMassBagStoreFragment& Frag, FArcBagId BagId);

	ARCMASSITEMSRUNTIME_API FArcItemId GetSlotItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId, int32 SlotIndex);

	ARCMASSITEMSRUNTIME_API bool IsBagOccupied(const FArcBagInstance& Bag);

	ARCMASSITEMSRUNTIME_API bool CanAcceptItemInSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId, int32 SlotIndex, FArcItemId ItemId);

	/** First empty slot across all bags (Bags.Items order) that accepts ItemId. Returns true and fills
	 *  OutBag/OutSlot; false if none. Acceptance mirrors PlaceItemInSlot (resident-in-store + ItemFitsBagSlot). */
	ARCMASSITEMSRUNTIME_API bool FindFirstFreeSlotForItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId, FArcBagId& OutBag, int32& OutSlot);

	/** True if ItemId currently occupies any slot in any bag on this entity (and ItemId is valid). */
	ARCMASSITEMSRUNTIME_API bool IsItemInAnyBag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId);

	/**
	 * Add Spec to the bag's target store then place the resulting item in the first free accepting bag
	 * slot. The store is derived from the entity's bag overlay (FArcMassBagStoreFragment::TargetStoreType,
	 * defaulting to FArcMassItemStoreFragment) — never passed in, so it can never mismatch the bag.
	 * Returns FArcItemId::InvalidId if the entity has no bag overlay. All-or-nothing into the bag: if no
	 * accepting slot is free the just-added item is rolled back and InvalidId is returned, so the caller
	 * can leave the spec in its source. If AddItem stacked the spec wholly into an already-bagged item,
	 * returns that id with no extra placement (the double-place guard). See the .cpp for the
	 * exact-rollback rationale. */
	ARCMASSITEMSRUNTIME_API FArcItemId AddItemToFirstFreeSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemSpec& Spec);

	/**
	 * Specific-slot sibling of AddItemToFirstFreeSlot: add Spec to the bag's target store
	 * (derived from FArcMassBagStoreFragment::TargetStoreType, never passed in) then place the
	 * resulting item into exactly BagId/SlotIndex via PlaceItemInSlot. Same guarantees as the
	 * sibling: whole-spec merge into an already-bagged resident returns that id with no extra
	 * placement, and a failed placement (occupied / out-of-range / rejected slot) rolls the store
	 * back EXACTLY (MergedDelta-aware — see the sibling's .cpp rationale) and returns
	 * FArcItemId::InvalidId. No bag overlay → InvalidId. */
	ARCMASSITEMSRUNTIME_API FArcItemId AddItemToSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemSpec& Spec, FArcBagId BagId, int32 SlotIndex);

	/**
	 * Maximum stacks a single item of this definition may hold, read straight from the def's stack
	 * method (StackByType's MaxStacks scalable float) — no live FArcItemData required. Non-stacking
	 * or null def → 1; never returns < 1. Used to size spill chunks before any item exists. */
	ARCMASSITEMSRUNTIME_API int32 GetMaxStacksForDef(const FArcItemDefinitionData* Def);

	/**
	 * Spilling add: fill existing bagged partials of the same definition, then successive free slots
	 * (splitting at def-max per slot), and STOP at the first slot that can't be seated — leaving the
	 * remainder in the source. Returns the total stacks that actually landed in bags; the caller
	 * decrements its source by this count only (atomic partial fill, no loss/dup). Each placed chunk
	 * rolls back exactly on failure (mirrors AddItemToFirstFreeSlot). No bag overlay → returns 0. */
	ARCMASSITEMSRUNTIME_API int32 AddItemSpilling(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemSpec& Spec);

	/** Resolve (and cache) the bag's definition — CachedDefinition first, else via the asset manager. May return null. */
	ARCMASSITEMSRUNTIME_API const UArcBagDefinition* ResolveBagDefinition(FArcBagInstance& Bag);

	/** Acceptance test: RequiredTags (HasAll), IgnoreTags (none of HasAny), and CustomCondition::CanAccept. */
	ARCMASSITEMSRUNTIME_API bool ItemFitsBagSlot(const UArcBagDefinition& Def, const FArcItemData& Item);
}
