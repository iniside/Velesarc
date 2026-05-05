// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"

struct FMassEntityManager;
struct FMassEntityHandle;
struct FArcItemData;
class UScriptStruct;

namespace ArcMassItems::QuickBar
{
	ARCMASSITEMSRUNTIME_API void AddAndActivateSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FGameplayTag BarId,
		FGameplayTag SlotId,
		FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API void RemoveSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FGameplayTag BarId,
		FGameplayTag SlotId);

	ARCMASSITEMSRUNTIME_API bool ActivateSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FGameplayTag BarId,
		FGameplayTag SlotId);

	ARCMASSITEMSRUNTIME_API bool DeactivateSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FGameplayTag BarId,
		FGameplayTag SlotId);

	ARCMASSITEMSRUNTIME_API FGameplayTag CycleSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FGameplayTag BarId,
		FGameplayTag CurrentSlotId,
		int32 Direction,
		TFunction<bool(const FArcItemData*)> SlotValidCondition);

	ARCMASSITEMSRUNTIME_API void ActivateBar(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FGameplayTag BarId);

	ARCMASSITEMSRUNTIME_API void DeactivateBar(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FGameplayTag BarId);

	ARCMASSITEMSRUNTIME_API FGameplayTag CycleBar(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FGameplayTag OldBarId,
		int32 Direction = 1);

	ARCMASSITEMSRUNTIME_API bool IsSlotActive(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId,
		FGameplayTag SlotId);

	ARCMASSITEMSRUNTIME_API FArcItemId GetAssignedItem(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId,
		FGameplayTag SlotId);

	ARCMASSITEMSRUNTIME_API TArray<FGameplayTag> GetActiveSlots(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId);
}
