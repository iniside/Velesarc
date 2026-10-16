// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityHandle.h"

struct FMassEntityManager;
class UScriptStruct;

// Auto-select logic for quick-bar slots, driven by item slot events.
//
// Free functions so tests can drive the logic directly without a world subsystem;
// UArcMassQuickBarAutoSelectSubsystem is the thin delegate adapter that forwards
// UArcMassItemEventSubsystem's slot rails here in production.
//
// Each function: bails if the entity is invalid or lacks FArcMassQuickBarSharedFragment,
// and only visits configs whose StoreType equals StoreType. Child bars (ParentBarId set)
// are never touched: only their parent fills them.
// Payload comes from the delegate — no FArcMassItemEventFragment iteration, so handlers
// invoked downstream may freely run item operations (no iterator-invalidation hazard).
namespace ArcMassItems::QuickBar::AutoSelect
{
	// Item entered a slot from unslotted: add+activate matching quick-bar slots.
	ARCMASSITEMSRUNTIME_API void HandleSlotAssigned(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcItemId ItemId,
		FGameplayTag NewSlot,
		const UScriptStruct* StoreType);

	// Item left its slot: remove it from any quick-bar slots holding it.
	ARCMASSITEMSRUNTIME_API void HandleSlotCleared(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcItemId ItemId,
		const UScriptStruct* StoreType);

	// Item moved slot A -> slot B: remove from current quick-bar slots, then
	// add+activate quick-bar slots matching the destination.
	ARCMASSITEMSRUNTIME_API void HandleSlotChanged(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcItemId ItemId,
		FGameplayTag NewSlot,
		const UScriptStruct* StoreType);
}
