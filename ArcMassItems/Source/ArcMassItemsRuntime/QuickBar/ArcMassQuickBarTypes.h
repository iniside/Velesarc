// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcMassQuickBarTypes.generated.h"

// Decides which quick slots of an ACTIVE bar are active. A slot of an inactive bar is never active.
UENUM()
enum class EArcMassQuickSlotsMode : uint8
{
	// At most one slot is active (weapon set: one weapon in hand). An added item activates only if no other
	// slot of the bar is active and its slot has bAutoSelect; removing the active item selects nothing new.
	// CycleSlot moves the active slot; ActivateBar restores the last active slot if it still holds an item,
	// otherwise picks the first filled slot that passes the CycleSlot checks.
	Cyclable,
	// Every slot holding an item is active (skill bar: every skill usable at once). An added item activates
	// at once while the bar is active; ActivateBar activates every filled slot. Not allowed on a parent bar.
	AutoActivateOnly,
	// Only slots activated explicitly (ActivateSlot / FArcMassActivateQuickSlotCommand) are active; adding an
	// item never activates its slot. Explicit activations are not remembered across bar deactivation.
	ManualActivationOnly
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarSlot
{
	GENERATED_BODY()

	// Cyclable bars only: an item added to this slot of an active bar activates the slot when no other slot of
	// the bar is active (first weapon picked up is drawn). When false the item waits for CycleSlot or an
	// explicit activation. Ignored by AutoActivateOnly and ManualActivationOnly bars.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	bool bAutoSelect = true;

	// Identity of the slot inside its bar; a quick slot is always addressed by (BarId, QuickBarSlotId) in ops,
	// commands and UI (e.g. QuickSlot.Weapon.1). Must be unique within the bar.
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "QuickSlot"))
	FGameplayTag QuickBarSlotId;

	// Not read by any runtime path (the add-to-quick-slot commands use ItemSlotId or SlotId.Active).
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "SlotId"))
	FGameplayTag DefaultItemSlotId;

	// Item slot this quick slot mirrors. Placing an item of the config's store into this item slot assigns it
	// to the quick slot (auto-select); clearing or moving the item out removes it. With bDeferItemActivation on
	// the bar, an item in this item slot is active only while the quick slot is active. Unset: the slot is
	// filled only by the add-to-quick-slot commands (item goes to the shared SlotId.Active) and survives
	// item slot changes. Required on every slot of a grouped bar and of a child bar.
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "SlotId"))
	FGameplayTag ItemSlotId;

	// The item's aggregated tags must contain all of these for auto-select to assign it here and for
	// CycleSlot to pick this slot (e.g. Item.Weapon.Ranged for a bow slot). ActivateBar checks them only for
	// its fallback pick, not when restoring the last active slot. Empty = any item.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	FGameplayTagContainer ItemRequiredTags;

	// Input binding applied when the slot activates and removed when it deactivates (e.g. bind the item's
	// granted ability to InputTag.Skill.1). Binds only while the slot holds an item.
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassQuickBarInputBinding"))
	FInstancedStruct InputBind;

	// Extra conditions checked when CycleSlot selects the active slot of a Cyclable bar, and by ActivateBar's
	// fallback pick; a failing validator skips this slot (e.g. no ammo). Not checked when ActivateBar restores
	// the last active slot, by auto-activation on add, or by ActivateSlot.
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassQuickBarSlotValidator", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> Validators;

	// Handlers called with the slot's item: OnSlotSelected when the slot activates, OnSlotDeselected when it
	// deactivates (e.g. play a draw/holster montage, show a weapon mesh).
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassQuickSlotHandler", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> SelectedHandlers;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarEntry
{
	GENERATED_BODY()

	// Identity of the bar (e.g. QuickBar.Abilities.01). Unique across all configs of a QuickBar trait; ops and
	// commands find the bar, its config and store by this id alone.
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "QuickBar,BarId"))
	FGameplayTag BarId;

	// Bar is active when the entity spawns. Changed later by ActivateBar / DeactivateBar / CycleActiveBar and
	// the activate/cycle bar commands. AutoActivateOnly + bActiveOnStart = a bar that is active as a whole:
	// every added item is active and ready at once. At most one bar per group; not allowed on child bars.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	bool bActiveOnStart = false;

	// Which slots of this bar are active while the bar is active; see EArcMassQuickSlotsMode.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	EArcMassQuickSlotsMode QuickSlotsMode = EArcMassQuickSlotsMode::Cyclable;

	// Items placed in the ItemSlotIds of this bar's slots stay inactive (no abilities, effects or granted
	// tags) until their quick slot activates, and deactivate with it. Use for weapon sets and skill bars whose
	// items must only work while selected. Off: an item activates on item-slot placement, whatever the quick
	// slot or bar state (passive bars). Required on grouped bars and child bars.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	bool bDeferItemActivation = false;

	// Makes this bar the single child bar of the named parent bar: an item with FArcItemFragment_QuickBarItems
	// added to a parent slot fills this bar with its child items while that parent slot is active (e.g. a
	// spellbook filling the spell bar). A child bar is driven only by its parent; commands reject it.
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "QuickBar,BarId"))
	FGameplayTag ParentBarId;

	// The item's aggregated tags must contain all of these for any slot of this bar to accept it through
	// auto-select or to be picked by CycleSlot or ActivateBar's fallback pick (e.g. Item.Weapon). Empty = any item.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	FGameplayTagContainer ItemRequiredTags;

	// Actions called when the bar becomes active (QuickBarActivated) or inactive (QuickBarDeactivated) through
	// ActivateBar / DeactivateBar / CycleActiveBar. Not called for bars that start active (bActiveOnStart).
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassQuickBarSelectedAction", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> QuickBarSelectedActions;

	// The bar's quick slots, in cycling order.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TArray<FArcMassQuickBarSlot> Slots;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarGroup
{
	GENERATED_BODY()

	// Identity of the group (e.g. QuickBarGroup.WeaponSets), used by CycleActiveBar and the cycle bar command.
	// Unique across all configs of a QuickBar trait.
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "QuickBar,GroupId"))
	FGameplayTag GroupId;

	// Allows CycleActiveBar on this group: the next bar in Bars order (wrapping) that holds at least one item
	// becomes the active bar (weapon set A -> B -> A). Off: bars switch only through ActivateBar.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	bool bCyclable = false;

	// Bars of which at most one is active at a time: activating one deactivates the group's other active bar
	// first. Bars that may be active together are not grouped. Every bar here must set bDeferItemActivation
	// and every slot an ItemSlotId, so the items of an inactive bar are inactive. Bars are defined exactly as
	// ungrouped ones; the one with bActiveOnStart (at most one) is active on spawn, otherwise none is.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TArray<FArcMassQuickBarEntry> Bars;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data", meta = (AllowedTypes = "ArcItem"))
	FArcNamedPrimaryAssetId Item;
};

USTRUCT(meta = (DisplayName = "Quick Bar Items", Category = "QuickBar", ToolTip = "Items created into the child bar of the quick bar this item is added to; they fill the child bar while this item's quick slot is active and are destroyed when this item leaves the quick slot."))
struct ARCMASSITEMSRUNTIME_API FArcItemFragment_QuickBarItems : public FArcItemFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<FArcMassQuickBarItem> ItemsToAdd;
};
