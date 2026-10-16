// Copyright Lukasz Baran. All Rights Reserved.

#include "Bags/ArcBagOperations.h"

#include "Bags/ArcBagStoreFragment.h"
#include "Bags/ArcBagDefinition.h"
#include "Bags/ArcBagEventSubsystem.h"
#include "Operations/ArcMassItemOperations.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Rarity.h"
#include "Items/ArcItemStackMethod.h"
#include "Engine/AssetManager.h"
#include "Mass/EntityManager.h"
#include "StructUtils/StructView.h"

namespace ArcMassItems::Bags
{

// File-local resolution helpers — unique namespace so they never collide with the
// (identically purposed) Private helpers in ArcMassItemOperations.cpp.
namespace ArcBagOpsInternal
{
	FArcMassBagStoreFragment* GetBagStore(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
	{
		FStructView View = EntityManager.GetMutableSparseElementDataForEntity(FArcMassBagStoreFragment::StaticStruct(), Entity);
		return View.GetPtr<FArcMassBagStoreFragment>();
	}

	FArcItemData* ResolveStoreItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
	{
		if (!StoreType || !ItemId.IsValid())
		{
			return nullptr;
		}
		FStructView View = EntityManager.GetMutableSparseElementDataForEntity(StoreType, Entity);
		FArcMassItemStoreFragment* Store = View.GetPtr<FArcMassItemStoreFragment>();
		if (!Store)
		{
			return nullptr;
		}
		FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
		return Item ? Item->ToItem() : nullptr;
	}

	int32 GetItemMaxStacks(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemData& Item)
	{
		const FArcItemDefinitionData* Def = Item.GetItemDefinition();
		if (!Def)
		{
			return 1;
		}
		const FArcItemStackMethod* Method = Def->GetStackMethod<FArcItemStackMethod>();
		if (!Method)
		{
			return 1;
		}
		return Method->GetMaxStacksMass(EntityManager, Entity, StoreType, Item);
	}
}

const UArcBagDefinition* ResolveBagDefinition(FArcBagInstance& Bag)
{
	if (Bag.CachedDefinition)
	{
		return Bag.CachedDefinition;
	}
	if (!Bag.DefinitionId.IsValid())
	{
		return nullptr;
	}
	UArcBagDefinition* Resolved = UAssetManager::Get().GetPrimaryAssetObject<UArcBagDefinition>(Bag.DefinitionId);
	Bag.CachedDefinition = Resolved;
	return Resolved;
}

bool ItemFitsBagSlot(const UArcBagDefinition& Def, const FArcItemData& Item)
{
	const FGameplayTagContainer& ItemTags = Item.ItemAggregatedTags;

	if (!Def.RequiredTags.IsEmpty() && !ItemTags.HasAll(Def.RequiredTags))
	{
		return false;
	}
	if (!Def.IgnoreTags.IsEmpty() && ItemTags.HasAny(Def.IgnoreTags))
	{
		return false;
	}
	if (Def.CustomCondition.IsValid())
	{
		const FArcBagAcceptanceCondition* Condition = Def.CustomCondition.GetPtr<FArcBagAcceptanceCondition>();
		if (Condition && !Condition->CanAccept(Item))
		{
			return false;
		}
	}
	return true;
}

void ReconcileBagSlots(FArcBagInstance& Bag, const UArcBagDefinition& Def)
{
	const int32 DesiredCount = FMath::Max(0, Def.SlotCount);

	if (Bag.Slots.Num() < DesiredCount)
	{
		// Grow: pad with default (empty) item ids.
		Bag.Slots.SetNum(DesiredCount);
		return;
	}

	if (Bag.Slots.Num() > DesiredCount)
	{
		// Shrink only when the tail beyond SlotCount is entirely empty — never drop an
		// occupied slot (the longer length is kept until those slots are cleared).
		bool bTailEmpty = true;
		for (int32 Index = DesiredCount; Index < Bag.Slots.Num(); ++Index)
		{
			if (Bag.Slots[Index].IsValid())
			{
				bTailEmpty = false;
				break;
			}
		}
		if (bTailEmpty)
		{
			Bag.Slots.SetNum(DesiredCount);
		}
	}
}

FArcBagId AddBag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UArcBagDefinition* Def)
{
	if (!Def)
	{
		return FArcBagId();
	}

	FArcMassBagStoreFragment* Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!Frag)
	{
		return FArcBagId();
	}

	FArcBagInstance NewBag;
	NewBag.BagId = FArcBagId::Generate();
	NewBag.DefinitionId = Def->GetPrimaryAssetId();
	// Set the resolved cache directly so transient/test definitions (not registered with
	// the asset manager) still resolve through ResolveBagDefinition.
	NewBag.CachedDefinition = Def;
	ReconcileBagSlots(NewBag, *Def);

	const FArcBagId NewId = NewBag.BagId;
	Frag->Bags.AddItem(MoveTemp(NewBag));

	UWorld* World = EntityManager.GetWorld();
	UArcMassBagEventSubsystem* BagEvents = World ? World->GetSubsystem<UArcMassBagEventSubsystem>() : nullptr;
	if (BagEvents)
	{
		BagEvents->QueueBagListChanged(Entity);
	}
	return NewId;
}

bool RemoveBag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId)
{
	if (BagId.IsNone())
	{
		return false;
	}

	FArcMassBagStoreFragment* Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!Frag)
	{
		return false;
	}

	const int32 Index = Frag->Bags.FindIndexById(BagId);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	const FArcBagInstance& Bag = Frag->Bags.Items[Index];
	for (const FArcItemId& SlotItem : Bag.Slots)
	{
		if (SlotItem.IsValid())
		{
			return false; // refuse to drop a bag that still holds items
		}
	}

	Frag->Bags.RemoveItemAt(Index);

	UWorld* World = EntityManager.GetWorld();
	UArcMassBagEventSubsystem* BagEvents = World ? World->GetSubsystem<UArcMassBagEventSubsystem>() : nullptr;
	if (BagEvents)
	{
		BagEvents->QueueBagListChanged(Entity);
	}
	return true;
}

FArcBagInstance* FindBag(FArcMassBagStoreFragment& Frag, FArcBagId BagId)
{
	if (BagId.IsNone())
	{
		return nullptr;
	}
	return Frag.Bags.FindById(BagId);
}

FArcItemId GetSlotItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId, int32 SlotIndex)
{
	FArcMassBagStoreFragment* Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!Frag)
	{
		return FArcItemId();
	}

	FArcBagInstance* Bag = FindBag(*Frag, BagId);
	if (!Bag || SlotIndex < 0 || SlotIndex >= Bag->Slots.Num())
	{
		return FArcItemId();
	}
	return Bag->Slots[SlotIndex];
}

bool IsBagOccupied(const FArcBagInstance& Bag)
{
	for (const FArcItemId& SlotItem : Bag.Slots)
	{
		if (SlotItem.IsValid())
		{
			return true;
		}
	}
	return false;
}

bool PlaceItemInSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId, int32 SlotIndex, FArcItemId ItemId)
{
	if (BagId.IsNone() || !ItemId.IsValid())
	{
		return false;
	}

	FArcMassBagStoreFragment* Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!Frag)
	{
		return false;
	}

	FArcBagInstance* Bag = FindBag(*Frag, BagId);
	if (!Bag || SlotIndex < 0 || SlotIndex >= Bag->Slots.Num())
	{
		return false;
	}
	if (Bag->Slots[SlotIndex].IsValid())
	{
		return false; // slot occupied
	}

	FArcItemData* ItemData = ArcBagOpsInternal::ResolveStoreItem(EntityManager, Entity, Frag->TargetStoreType, ItemId);
	if (!ItemData)
	{
		return false; // item not resident in the target store
	}

	const UArcBagDefinition* Def = ResolveBagDefinition(*Bag);
	if (!Def || !ItemFitsBagSlot(*Def, *ItemData))
	{
		return false; // fail-closed if the def can't resolve
	}

	Bag->Slots[SlotIndex] = ItemId;
	Frag->Bags.MarkItemDirty(*Bag);

	UWorld* World = EntityManager.GetWorld();
	UArcMassBagEventSubsystem* BagEvents = World ? World->GetSubsystem<UArcMassBagEventSubsystem>() : nullptr;
	if (BagEvents)
	{
		BagEvents->QueueBagSlotsChanged(Entity, BagId);
	}
	return true;
}

bool ClearSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId, int32 SlotIndex)
{
	if (BagId.IsNone())
	{
		return false;
	}

	FArcMassBagStoreFragment* Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!Frag)
	{
		return false;
	}

	FArcBagInstance* Bag = FindBag(*Frag, BagId);
	if (!Bag || SlotIndex < 0 || SlotIndex >= Bag->Slots.Num())
	{
		return false;
	}

	// Clear the reference only — the item itself remains in the store.
	Bag->Slots[SlotIndex] = FArcItemId();
	Frag->Bags.MarkItemDirty(*Bag);

	UWorld* World = EntityManager.GetWorld();
	UArcMassBagEventSubsystem* BagEvents = World ? World->GetSubsystem<UArcMassBagEventSubsystem>() : nullptr;
	if (BagEvents)
	{
		BagEvents->QueueBagSlotsChanged(Entity, BagId);
	}
	return true;
}

bool CanAcceptItemInSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId, int32 SlotIndex, FArcItemId ItemId)
{
	if (BagId.IsNone() || !ItemId.IsValid())
	{
		return false;
	}

	FArcMassBagStoreFragment* Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!Frag)
	{
		return false;
	}

	FArcBagInstance* Bag = FindBag(*Frag, BagId);
	if (!Bag || SlotIndex < 0 || SlotIndex >= Bag->Slots.Num())
	{
		return false;
	}
	if (Bag->Slots[SlotIndex].IsValid())
	{
		return false; // occupied
	}

	FArcItemData* ItemData = ArcBagOpsInternal::ResolveStoreItem(EntityManager, Entity, Frag->TargetStoreType, ItemId);
	if (!ItemData)
	{
		return false;
	}

	const UArcBagDefinition* Def = ResolveBagDefinition(*Bag);
	return Def != nullptr && ItemFitsBagSlot(*Def, *ItemData);
}

bool FindFirstFreeSlotForItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId, FArcBagId& OutBag, int32& OutSlot)
{
	if (!ItemId.IsValid())
	{
		return false;
	}

	FArcMassBagStoreFragment* Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!Frag)
	{
		return false;
	}

	FArcItemData* ItemData = ArcBagOpsInternal::ResolveStoreItem(EntityManager, Entity, Frag->TargetStoreType, ItemId);
	if (!ItemData)
	{
		return false; // item not resident in the target store
	}

	for (FArcBagInstance& Bag : Frag->Bags.Items)
	{
		const UArcBagDefinition* Def = ResolveBagDefinition(Bag);
		if (!Def || !ItemFitsBagSlot(*Def, *ItemData))
		{
			continue; // fail-closed: skip bags that can't resolve or don't accept the item
		}

		for (int32 SlotIndex = 0; SlotIndex < Bag.Slots.Num(); ++SlotIndex)
		{
			if (!Bag.Slots[SlotIndex].IsValid())
			{
				OutBag = Bag.BagId;
				OutSlot = SlotIndex;
				return true;
			}
		}
	}
	return false;
}

bool IsItemInAnyBag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId)
{
	if (!ItemId.IsValid())
	{
		return false;
	}

	FArcMassBagStoreFragment* Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!Frag)
	{
		return false;
	}

	for (const FArcBagInstance& Bag : Frag->Bags.Items)
	{
		for (const FArcItemId& Slot : Bag.Slots)
		{
			if (Slot == ItemId)
			{
				return true;
			}
		}
	}
	return false;
}

// Add Spec to the store, then place the resulting item in the first free accepting bag slot.
//
// EXACT-ROLLBACK rationale (the whole point of this op): when Spec is stackable and a same-definition
// resident already exists, AddItem MUTATES that resident's stacks AND, only if the spec overflows
// MaxStacks, returns a NEW overflow item id. A naive "add → on full, RemoveItem(NewId)" rollback would
// delete only the overflow and silently leave the merged stacks behind while reporting failure — the
// caller then keeps the full spec in its source → ITEM DUPLICATION. To roll back exactly we pre-read the
// would-merge delta with the PURE StackCheckMass query (no mutation) plus FindItemByDefinition's pre-merge
// stack count, then on a failed placement undo BOTH the overflow item (RemoveItem) AND the partial merge
// (ModifyStacks by -MergedDelta). Net effect on the store is zero. Symmetric case: if AddItem merged the
// WHOLE spec into an existing resident (NewId == ExistingId, no new item — e.g. a stackable item the
// pawn-init path "left in store" unbagged), the failed-placement rollback undoes only the added stacks
// (never RemoveItem, which would destroy the resident's pre-existing stacks).
//
// Documented limitations:
//  - Multi-item stacking overflow: the shipped StackByType produces at most one overflow item (one slot
//    suffices), so this is a non-issue for it. A hypothetical custom stack method splitting a spec into
//    many items would leave the extras resident in the store (not rolled back). Accepted.
//  - Bag-array replication is NOT wired yet (FArcMassBagStoreFragment is not in ReplicatedFragments): slot
//    assignments here are server-side only; clients won't see bag placement until that lands. Store
//    contents still replicate; the two transient same-frame stack events the exact rollback emits are
//    immaterial while that path is unwired.
FArcItemId AddItemToFirstFreeSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemSpec& Spec)
{
	// 0. Derive the target store from the bag overlay itself — the bag names the store its slots index
	//    into (TargetStoreType), so it can never mismatch. No bag overlay → nothing to add through.
	FArcMassBagStoreFragment* BagFrag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!BagFrag)
	{
		return FArcItemId::InvalidId;
	}
	const UScriptStruct* StoreType = BagFrag->TargetStoreType
		? BagFrag->TargetStoreType.Get()
		: FArcMassItemStoreFragment::StaticStruct();

	// 1. Pre-read the would-merge delta with the pure stacking query (no mutation).
	FArcItemId ExistingId;
	int32 MergedDelta = 0;
	const FArcItemDefinitionData* Def = Spec.GetItemDefinition();
	if (Def)
	{
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();
		if (StackMethod && StackMethod->CanStackMass(EntityManager, Entity, StoreType, Spec))
		{
			uint16 NewStacks = 0;
			uint16 Remaining = 0;
			ExistingId = StackMethod->StackCheckMass(EntityManager, Entity, StoreType, Spec, NewStacks, Remaining);
			// Pair the delta with the item StackCheckMass actually targeted — first-by-definition
			// can be a DIFFERENT (full) stack than the merge target once several stacks exist.
			const FArcItemData* Pre = ArcMassItems::FindItemById(EntityManager, Entity, StoreType, ExistingId);
			if (Pre)
			{
				MergedDelta = static_cast<int32>(NewStacks) - static_cast<int32>(Pre->GetStacks());
			}
		}
	}

	// 2. Add to the store (this may merge into ExistingId and/or create an overflow item).
	const FArcItemId NewId = ArcMassItems::AddItem(EntityManager, Entity, StoreType, Spec);

	// 3. Store-level reject (e.g. CanNotStackUnique) — nothing was mutated.
	if (!NewId.IsValid())
	{
		return FArcItemId::InvalidId;
	}

	// 4. The whole spec merged into an already-bagged stack — success, no slot needed. Without this guard
	//    we would place the (already-slotted) merge target id into a SECOND slot.
	if (IsItemInAnyBag(EntityManager, Entity, NewId))
	{
		return NewId;
	}

	// 5. Place the resulting item into the first free accepting slot.
	FArcBagId OutBag;
	int32 OutSlot = INDEX_NONE;
	if (FindFirstFreeSlotForItem(EntityManager, Entity, NewId, OutBag, OutSlot)
		&& PlaceItemInSlot(EntityManager, Entity, OutBag, OutSlot, NewId))
	{
		return NewId;
	}

	// 6. No accepting slot — roll back EXACTLY so nothing is duplicated AND nothing pre-existing is lost.
	if (ExistingId.IsValid() && NewId == ExistingId)
	{
		// AddItem merged the whole spec into an existing resident; no new item was created. Removing the
		// item would destroy the resident's PRE-EXISTING stacks (e.g. a stackable item the pawn-init "left
		// in store" when bags were full). Undo only the stacks we added.
		if (MergedDelta > 0)
		{
			ArcMassItems::ModifyStacks(EntityManager, Entity, StoreType, ExistingId, -MergedDelta);
		}
	}
	else
	{
		// NewId is a fresh / overflow item: remove it, and also undo any partial merge into a pre-existing
		// resident (the dup-prevention case).
		ArcMassItems::RemoveItem(EntityManager, Entity, StoreType, NewId);
		if (ExistingId.IsValid() && MergedDelta > 0)
		{
			ArcMassItems::ModifyStacks(EntityManager, Entity, StoreType, ExistingId, -MergedDelta);
		}
	}
	return FArcItemId::InvalidId;
}

// Specific-slot sibling of AddItemToFirstFreeSlot: identical store derivation, pre-read merge
// delta, already-bagged double-place guard, and EXACT rollback (see the rationale block above
// AddItemToFirstFreeSlot) — only the placement step differs: the caller names BagId/SlotIndex
// and PlaceItemInSlot performs the range/occupancy/acceptance validation.
FArcItemId AddItemToSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemSpec& Spec, FArcBagId BagId, int32 SlotIndex)
{
	// 0. Derive the target store from the bag overlay itself (TargetStoreType names the store the
	//    bag's slots index into, so it can never mismatch). No bag overlay → nothing to add through.
	FArcMassBagStoreFragment* BagFrag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!BagFrag)
	{
		return FArcItemId::InvalidId;
	}
	const UScriptStruct* StoreType = BagFrag->TargetStoreType
		? BagFrag->TargetStoreType.Get()
		: FArcMassItemStoreFragment::StaticStruct();

	// 1. Pre-read the would-merge delta with the pure stacking query (no mutation).
	FArcItemId ExistingId;
	int32 MergedDelta = 0;
	const FArcItemDefinitionData* Def = Spec.GetItemDefinition();
	if (Def)
	{
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();
		if (StackMethod && StackMethod->CanStackMass(EntityManager, Entity, StoreType, Spec))
		{
			uint16 NewStacks = 0;
			uint16 Remaining = 0;
			ExistingId = StackMethod->StackCheckMass(EntityManager, Entity, StoreType, Spec, NewStacks, Remaining);
			// Pair the delta with the item StackCheckMass actually targeted — first-by-definition
			// can be a DIFFERENT (full) stack than the merge target once several stacks exist.
			const FArcItemData* Pre = ArcMassItems::FindItemById(EntityManager, Entity, StoreType, ExistingId);
			if (Pre)
			{
				MergedDelta = static_cast<int32>(NewStacks) - static_cast<int32>(Pre->GetStacks());
			}
		}
	}

	// 2. Add to the store (this may merge into ExistingId and/or create an overflow item).
	const FArcItemId NewId = ArcMassItems::AddItem(EntityManager, Entity, StoreType, Spec);

	// 3. Store-level reject (e.g. CanNotStackUnique) — nothing was mutated.
	if (!NewId.IsValid())
	{
		return FArcItemId::InvalidId;
	}

	// 4. The whole spec merged into an already-bagged stack — success, no slot needed. Without this
	//    guard we would place the (already-slotted) merge target id into a SECOND slot.
	if (IsItemInAnyBag(EntityManager, Entity, NewId))
	{
		return NewId;
	}

	// 5. Place the resulting item into the requested slot. PlaceItemInSlot validates the bag id,
	//    the index against the live Slots.Num(), occupancy, and the def's acceptance rules.
	if (PlaceItemInSlot(EntityManager, Entity, BagId, SlotIndex, NewId))
	{
		return NewId;
	}

	// 6. Placement failed — roll back EXACTLY so nothing is duplicated AND nothing pre-existing is
	//    lost (never a bare RemoveItem; see AddItemToFirstFreeSlot's rationale).
	if (ExistingId.IsValid() && NewId == ExistingId)
	{
		// AddItem merged the whole spec into an existing (unbagged) resident; no new item was
		// created. Removing the item would destroy the resident's PRE-EXISTING stacks. Undo only
		// the stacks we added.
		if (MergedDelta > 0)
		{
			ArcMassItems::ModifyStacks(EntityManager, Entity, StoreType, ExistingId, -MergedDelta);
		}
	}
	else
	{
		// NewId is a fresh / overflow item: remove it, and also undo any partial merge into a
		// pre-existing resident (the dup-prevention case).
		ArcMassItems::RemoveItem(EntityManager, Entity, StoreType, NewId);
		if (ExistingId.IsValid() && MergedDelta > 0)
		{
			ArcMassItems::ModifyStacks(EntityManager, Entity, StoreType, ExistingId, -MergedDelta);
		}
	}
	return FArcItemId::InvalidId;
}

int32 GetMaxStacksForDef(const FArcItemDefinitionData* Def)
{
	// Definition-level max stacks — used BEFORE any FArcItemData exists (the spilling add
	// must chunk by def-max before its first AddItem). ArcBagOpsInternal::GetItemMaxStacks
	// reads the same value off a live item via the virtual GetMaxStacksMass; here we read it
	// straight from the def's stack method instead. Only StackByType carries a MaxStacks
	// scalable float — any other (or missing) method is non-stacking → 1. Floor mirrors every
	// other MaxStacks read in this module; clamp to >=1 so chunk sizing can never be 0 (a 0
	// would make AddItemSpilling's b-phase spin forever).
	if (!Def)
	{
		return 1;
	}
	const FArcItemStackMethod_StackByType* StackByType = Def->GetStackMethod<FArcItemStackMethod_StackByType>();
	if (!StackByType)
	{
		return 1;
	}
	return FMath::Max(1, FMath::FloorToInt(StackByType->MaxStacks.GetValue()));
}

// Spilling sibling of AddItemToFirstFreeSlot: instead of all-or-nothing, fill as much of Spec
// as the bag overlay can hold and return the number of stacks that actually landed in bags.
// The caller decrements its source by THIS return value only — so a partial fill leaves the
// unplaceable remainder in the source with no loss and no duplication.
//
// Two phases:
//   (a) Top up every existing BAGGED partial stack of the same definition and rarity first (direct
//       ModifyStacks, so a slot-full-but-stack-partial bag still absorbs stacks). Unbagged
//       residents are deliberately skipped — only stacks visible in a bag slot count as placed.
//   (b) While stacks remain, split into def-max chunks and route each through the proven
//       AddItem → FindFirstFreeSlotForItem → PlaceItemInSlot path, mirroring
//       AddItemToFirstFreeSlot's exact rollback (and its already-bagged double-place guard).
//       The first failure to find/seat a free slot rolls that chunk back exactly and stops —
//       the remainder stays in the source.
int32 AddItemSpilling(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemSpec& Spec)
{
	// 0. Derive the target store from the bag overlay (same rule as AddItemToFirstFreeSlot —
	//    the bag names the store its slots index into, so it can never mismatch). No overlay →
	//    nothing to add through; report 0 placed so the caller leaves everything in its source.
	FArcMassBagStoreFragment* BagFrag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!BagFrag)
	{
		return 0;
	}
	const UScriptStruct* StoreType = BagFrag->TargetStoreType
		? BagFrag->TargetStoreType.Get()
		: FArcMassItemStoreFragment::StaticStruct();

	const FArcItemDefinitionData* Def = Spec.GetItemDefinition();
	const int32 MaxStacks = GetMaxStacksForDef(Def);

	int32 Remaining = static_cast<int32>(Spec.Amount);
	int32 Placed = 0;

	// --- (a-phase) Top up existing BAGGED partials of the same stack key. ---
	// Snapshot the matching ids FIRST: ModifyStacks below opens a transaction that can add the
	// event sparse fragment and shift/realloc the store array, so we must not hold store
	// pointers (or iterate the live array) across a mutation.
	TArray<FArcItemId> BaggedPartialIds;
	{
		FStructView StoreView = EntityManager.GetMutableSparseElementDataForEntity(StoreType, Entity);
		FArcMassItemStoreFragment* Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
		if (Store)
		{
			for (const FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
			{
				const FArcItemData* Data = Item.ToItem();
				if (!Data || !ArcMassItems::IsSameStackKey(Data->Spec, Spec))
				{
					continue;
				}
				const FArcItemId ItemId = Data->GetItemId();
				if (IsItemInAnyBag(EntityManager, Entity, ItemId))
				{
					BaggedPartialIds.Add(ItemId);
				}
			}
		}
	}

	for (const FArcItemId& ItemId : BaggedPartialIds)
	{
		if (Remaining <= 0)
		{
			break;
		}
		// Re-resolve current stacks each iteration — ModifyStacks mutates the store.
		const FArcItemData* Data = ArcBagOpsInternal::ResolveStoreItem(EntityManager, Entity, StoreType, ItemId);
		if (!Data)
		{
			continue;
		}
		const int32 CurrentStacks = static_cast<int32>(Data->GetStacks());
		const int32 Transfer = FMath::Min(Remaining, MaxStacks - CurrentStacks);
		if (Transfer > 0 && ArcMassItems::ModifyStacks(EntityManager, Entity, StoreType, ItemId, Transfer))
		{
			Placed += Transfer;
			Remaining -= Transfer;
		}
	}

	// --- (b-phase) Spill the rest into free slots, splitting at def-max per slot. ---
	while (Remaining > 0)
	{
		const int32 Chunk = FMath::Min(Remaining, MaxStacks);

		// Fresh spec per chunk. The id MUST be regenerated: PlaceNewItem keeps a valid Spec.ItemId
		// verbatim, so reusing the source spec's id would stamp duplicate ids across chunks.
		FArcItemSpec ChunkSpec = Spec;
		ChunkSpec.SetItemId(FArcItemId::Generate());
		ChunkSpec.SetAmount(Chunk);

		// Pre-read the would-merge delta with the pure stacking query (no mutation) — the rollback
		// currency, identical to AddItemToFirstFreeSlot.
		FArcItemId ExistingId;
		int32 MergedDelta = 0;
		if (Def)
		{
			const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();
			if (StackMethod && StackMethod->CanStackMass(EntityManager, Entity, StoreType, ChunkSpec))
			{
				uint16 NewStacks = 0;
				uint16 RemainingStacks = 0;
				ExistingId = StackMethod->StackCheckMass(EntityManager, Entity, StoreType, ChunkSpec, NewStacks, RemainingStacks);
				// Pair the delta with the actual merge target (see AddItemToFirstFreeSlot).
				const FArcItemData* Pre = ArcMassItems::FindItemById(EntityManager, Entity, StoreType, ExistingId);
				if (Pre)
				{
					MergedDelta = static_cast<int32>(NewStacks) - static_cast<int32>(Pre->GetStacks());
				}
			}
		}

		const FArcItemId NewId = ArcMassItems::AddItem(EntityManager, Entity, StoreType, ChunkSpec);
		if (!NewId.IsValid())
		{
			break; // store-level reject (nothing mutated) — stop spilling.
		}

		// The whole chunk merged into an already-bagged stack — it is already seated, count it and
		// move on. Without this guard we would place an already-slotted id into a SECOND slot.
		if (IsItemInAnyBag(EntityManager, Entity, NewId))
		{
			Placed += Chunk;
			Remaining -= Chunk;
			continue;
		}

		FArcBagId OutBag;
		int32 OutSlot = INDEX_NONE;
		if (FindFirstFreeSlotForItem(EntityManager, Entity, NewId, OutBag, OutSlot)
			&& PlaceItemInSlot(EntityManager, Entity, OutBag, OutSlot, NewId))
		{
			Placed += Chunk;
			Remaining -= Chunk;
			continue;
		}

		// No accepting free slot — roll back EXACTLY (mirror AddItemToFirstFreeSlot) so this chunk
		// is undone and the source keeps it, then stop.
		if (ExistingId.IsValid() && NewId == ExistingId)
		{
			if (MergedDelta > 0)
			{
				ArcMassItems::ModifyStacks(EntityManager, Entity, StoreType, ExistingId, -MergedDelta);
			}
		}
		else
		{
			ArcMassItems::RemoveItem(EntityManager, Entity, StoreType, NewId);
			if (ExistingId.IsValid() && MergedDelta > 0)
			{
				ArcMassItems::ModifyStacks(EntityManager, Entity, StoreType, ExistingId, -MergedDelta);
			}
		}
		break;
	}

	return Placed;
}

bool MoveItemBetweenSlots(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId FromBag, int32 FromSlot, FArcBagId ToBag, int32 ToSlot)
{
	if (FromBag.IsNone() || ToBag.IsNone())
	{
		return false;
	}

	FArcMassBagStoreFragment* Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!Frag)
	{
		return false;
	}

	// Resolve once for the op — only the POD params (FromBag/ToBag) are read at the
	// success exits, so this is safe across the later sparse-pointer re-resolve.
	UWorld* World = EntityManager.GetWorld();
	UArcMassBagEventSubsystem* BagEvents = World ? World->GetSubsystem<UArcMassBagEventSubsystem>() : nullptr;

	FArcBagInstance* SourceBag = FindBag(*Frag, FromBag);
	FArcBagInstance* TargetBag = FindBag(*Frag, ToBag);
	if (!SourceBag || !TargetBag)
	{
		return false;
	}
	if (FromSlot < 0 || FromSlot >= SourceBag->Slots.Num())
	{
		return false;
	}
	if (ToSlot < 0 || ToSlot >= TargetBag->Slots.Num())
	{
		return false;
	}

	const FArcItemId SourceItemId = SourceBag->Slots[FromSlot];
	if (!SourceItemId.IsValid())
	{
		return false; // empty source — nothing to move
	}
	if (FromBag == ToBag && FromSlot == ToSlot)
	{
		return false; // no-op move onto itself
	}

	const UScriptStruct* StoreType = Frag->TargetStoreType;
	const UArcBagDefinition* SourceDef = ResolveBagDefinition(*SourceBag);
	const UArcBagDefinition* TargetDef = ResolveBagDefinition(*TargetBag);

	const FArcItemId TargetItemId = TargetBag->Slots[ToSlot];

	// --- Target empty: straight move (revalidate the source item against ToBag). ---
	if (!TargetItemId.IsValid())
	{
		FArcItemData* SourceItem = ArcBagOpsInternal::ResolveStoreItem(EntityManager, Entity, StoreType, SourceItemId);
		if (!SourceItem)
		{
			return false;
		}
		if (!TargetDef || !ItemFitsBagSlot(*TargetDef, *SourceItem))
		{
			return false; // fail-closed
		}

		TargetBag->Slots[ToSlot] = SourceItemId;
		SourceBag->Slots[FromSlot] = FArcItemId();
		Frag->Bags.MarkItemDirty(*SourceBag);
		if (SourceBag != TargetBag)
		{
			Frag->Bags.MarkItemDirty(*TargetBag);
		}
		if (BagEvents)
		{
			BagEvents->QueueBagSlotsChanged(Entity, FromBag);
			if (ToBag != FromBag)
			{
				BagEvents->QueueBagSlotsChanged(Entity, ToBag);
			}
		}
		return true;
	}

	// --- Target occupied. ---
	FArcItemData* SourceItem = ArcBagOpsInternal::ResolveStoreItem(EntityManager, Entity, StoreType, SourceItemId);
	FArcItemData* TargetItem = ArcBagOpsInternal::ResolveStoreItem(EntityManager, Entity, StoreType, TargetItemId);
	if (!SourceItem || !TargetItem)
	{
		return false;
	}

	const bool bSameStackKey = SourceItem->GetItemDefinition() != nullptr
		&& ArcMassItems::IsSameStackKey(SourceItem->Spec, TargetItem->Spec);
	const int32 TargetMax = ArcBagOpsInternal::GetItemMaxStacks(EntityManager, Entity, StoreType, *TargetItem);
	const bool bStackable = TargetMax > 1;

	if (bSameStackKey && bStackable)
	{
		const int32 SourceStacks = static_cast<int32>(SourceItem->GetStacks());
		const int32 TargetStacks = static_cast<int32>(TargetItem->GetStacks());
		const int32 Transfer = FMath::Min(SourceStacks, TargetMax - TargetStacks);

		if (Transfer > 0)
		{
			// Route the stack movement through the item-store ops so events + dirty
			// marking happen once, centrally. RemoveItem destroys the source item when
			// its stacks reach zero.
			if (!ArcMassItems::ModifyStacks(EntityManager, Entity, StoreType, TargetItemId, Transfer))
				{
					return false; // never remove the source stacks if the add failed
				}
			ArcMassItems::RemoveItem(EntityManager, Entity, StoreType, SourceItemId, Transfer);

				// The store ops above open a transaction that may add the event sparse
				// fragment, which can invalidate the bag-fragment pointer. Re-resolve
				// before touching any bag slot (the stack move itself is already done).
				Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
				SourceBag = Frag ? FindBag(*Frag, FromBag) : nullptr;
				if (!Frag || !SourceBag || FromSlot < 0 || FromSlot >= SourceBag->Slots.Num())
				{
					// Stacks already transferred above — the UI is stale if we skip this.
					if (BagEvents)
					{
						BagEvents->QueueBagSlotsChanged(Entity, FromBag);
						if (ToBag != FromBag)
						{
							BagEvents->QueueBagSlotsChanged(Entity, ToBag);
						}
					}
					return true;
				}

			// If the source item was fully consumed (destroyed), clear its now-dangling
			// slot reference; otherwise the leftover stack stays where it is.
			FArcItemData* SourceAfter = ArcBagOpsInternal::ResolveStoreItem(EntityManager, Entity, StoreType, SourceItemId);
			if (!SourceAfter)
			{
				SourceBag->Slots[FromSlot] = FArcItemId();
			}
			Frag->Bags.MarkItemDirty(*SourceBag);
			if (BagEvents)
			{
				BagEvents->QueueBagSlotsChanged(Entity, FromBag);
				if (ToBag != FromBag)
				{
					BagEvents->QueueBagSlotsChanged(Entity, ToBag);
				}
			}
			return true;
		}
		// Transfer == 0 → target already at max; fall through to the swap path.
	}

	// --- Swap: different stack key, non-stackable, or target already at max. ---
	// Require both directions to be accepted; if either fails, no mutation.
	if (!SourceDef || !TargetDef)
	{
		return false; // fail-closed
	}
	if (!ItemFitsBagSlot(*TargetDef, *SourceItem) || !ItemFitsBagSlot(*SourceDef, *TargetItem))
	{
		return false;
	}

	SourceBag->Slots[FromSlot] = TargetItemId;
	TargetBag->Slots[ToSlot] = SourceItemId;
	Frag->Bags.MarkItemDirty(*SourceBag);
	if (SourceBag != TargetBag)
	{
		Frag->Bags.MarkItemDirty(*TargetBag);
	}
	if (BagEvents)
	{
		BagEvents->QueueBagSlotsChanged(Entity, FromBag);
		if (ToBag != FromBag)
		{
			BagEvents->QueueBagSlotsChanged(Entity, ToBag);
		}
	}
	return true;
}

bool ReorderBag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcBagId BagId, int32 NewDisplayIndex)
{
	if (BagId.IsNone())
	{
		return false;
	}

	FArcMassBagStoreFragment* Frag = ArcBagOpsInternal::GetBagStore(EntityManager, Entity);
	if (!Frag)
	{
		return false;
	}

	const int32 Index = Frag->Bags.FindIndexById(BagId);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	const int32 Clamped = FMath::Clamp(NewDisplayIndex, 0, Frag->Bags.Items.Num() - 1);
	if (Clamped == Index)
	{
		return true; // already in place
	}

	// Stable remove + insert (NOT RemoveAt-in-loop). The whole instance — and its
	// replication ids — moves with it, so the BagId stays a valid address.
	FArcBagInstance Moved = MoveTemp(Frag->Bags.Items[Index]);
	Frag->Bags.Items.RemoveAt(Index);
	Frag->Bags.Items.Insert(MoveTemp(Moved), Clamped);
	Frag->Bags.MarkArrayDirty();

	UWorld* World = EntityManager.GetWorld();
	UArcMassBagEventSubsystem* BagEvents = World ? World->GetSubsystem<UArcMassBagEventSubsystem>() : nullptr;
	if (BagEvents)
	{
		BagEvents->QueueBagListChanged(Entity);
	}
	return true;
}

}
