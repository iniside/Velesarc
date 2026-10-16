// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "UObject/ObjectPtr.h"

struct FMassEntityManager;
struct FMassEntityHandle;
struct FArcItemData;
struct FArcMassQuickBarEntry;
struct FArcMassQuickBarGroup;
struct FArcMassQuickBarStateFragment;
class UScriptStruct;
class UArcMassQuickBarConfig;

struct FArcMassQuickBarLocation
{
	const UArcMassQuickBarConfig* Config = nullptr;
	const FArcMassQuickBarEntry* Bar = nullptr;
	const FArcMassQuickBarGroup* Group = nullptr;
};

struct FArcMassQuickBarBarSelection
{
	FGameplayTag BarId;
	bool bIsBarActive = false;
	FGameplayTag LastActiveSlotId;
	TArray<FGameplayTag> ActiveSlotIds;
};

namespace ArcMassItems::QuickBar
{
	// Visit every bar in both ungrouped (Config.QuickBars) and grouped (Config.Groups[].Bars)
	// collections. Group is nullptr for bars in the ungrouped collection.
	// Return false from the visitor to stop iteration early.
	ARCMASSITEMSRUNTIME_API void ForEachBarConfig(
		const UArcMassQuickBarConfig& Config,
		TFunctionRef<bool(const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarGroup* Group)> Visitor);

	ARCMASSITEMSRUNTIME_API void SeedBarStates(
		const TArray<TObjectPtr<UArcMassQuickBarConfig>>& Configs,
		FArcMassQuickBarStateFragment& OutState);

	ARCMASSITEMSRUNTIME_API FArcMassQuickBarLocation FindBar(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId);

	// Bar is always null in the result.
	ARCMASSITEMSRUNTIME_API FArcMassQuickBarLocation FindGroup(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag GroupId);

	ARCMASSITEMSRUNTIME_API void AddAndActivateSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId,
		FGameplayTag SlotId,
		FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API void RemoveSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId,
		FGameplayTag SlotId);

	ARCMASSITEMSRUNTIME_API bool ActivateSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId,
		FGameplayTag SlotId);

	ARCMASSITEMSRUNTIME_API bool DeactivateSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId,
		FGameplayTag SlotId);

	ARCMASSITEMSRUNTIME_API FGameplayTag CycleSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId,
		FGameplayTag CurrentSlotId,
		int32 Direction,
		TFunction<bool(const FArcItemData*)> SlotValidCondition);

	ARCMASSITEMSRUNTIME_API bool ActivateBar(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId);

	ARCMASSITEMSRUNTIME_API void DeactivateBar(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId);

	ARCMASSITEMSRUNTIME_API FGameplayTag CycleActiveBar(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag GroupId,
		int32 Direction = 1);

	// The bar and, when grouped, every bar of its group; empty for child bars.
	ARCMASSITEMSRUNTIME_API TArray<FArcMassQuickBarBarSelection> CaptureBarSelection(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId);

	ARCMASSITEMSRUNTIME_API TArray<FArcMassQuickBarBarSelection> CaptureGroupSelection(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag GroupId);

	ARCMASSITEMSRUNTIME_API void RestoreSelection(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const TArray<FArcMassQuickBarBarSelection>& Selection);

	ARCMASSITEMSRUNTIME_API FGameplayTag GetActiveBarInGroup(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag GroupId);

	ARCMASSITEMSRUNTIME_API bool IsBarActive(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId);

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

	ARCMASSITEMSRUNTIME_API bool IsItemActivationDeferred(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FGameplayTag ItemSlot);

	ARCMASSITEMSRUNTIME_API const FArcItemData* GetActiveItem(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FGameplayTagContainer& RequiredItemTags);
}
