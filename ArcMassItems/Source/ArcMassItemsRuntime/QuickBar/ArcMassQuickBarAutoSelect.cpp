// Copyright Lukasz Baran. All Rights Reserved.

#include "QuickBar/ArcMassQuickBarAutoSelect.h"

#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarInternal.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "QuickBar/ArcMassQuickBarTypes.h"

#include "Items/ArcItemData.h"

#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

namespace ArcMassQuickBarAutoSelect_Internal
{
	const FArcMassQuickBarSharedFragment* ResolveSharedFragment(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType)
	{
		if (!StoreType || !EntityManager.IsEntityValid(Entity))
		{
			return nullptr;
		}

		FMassEntityView View(EntityManager, Entity);
		return View.GetConstSharedFragmentDataPtr<FArcMassQuickBarSharedFragment>();
	}

	void RemoveItemFromQuickBarSlots(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcItemId ItemId,
		const UScriptStruct* StoreType,
		bool bOnlyTagManaged)
	{
		FMassEntityView View(EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		if (!State)
		{
			return;
		}

		TArray<TPair<FGameplayTag, FGameplayTag>, TInlineAllocator<4>> SlotsToRemove;
		for (const FArcMassQuickBarActiveSlot& ActiveSlot : State->ActiveSlots)
		{
			if (ActiveSlot.AssignedItemId != ItemId)
			{
				continue;
			}

			const FArcMassQuickBarLocation Location = ArcMassItems::QuickBar::FindBar(EntityManager, Entity, ActiveSlot.BarId);
			if (!Location.Bar || Location.Config->StoreType.Get() != StoreType || Location.Bar->ParentBarId.IsValid())
			{
				continue;
			}

			if (bOnlyTagManaged)
			{
				const FArcMassQuickBarSlot* QSlot = Location.Bar->Slots.FindByPredicate(
					[&ActiveSlot](const FArcMassQuickBarSlot& S) { return S.QuickBarSlotId == ActiveSlot.QuickSlotId; });
				// A slot without ItemSlotId is a manual assignment and survives assign/move events.
				if (!QSlot || !QSlot->ItemSlotId.IsValid())
				{
					continue;
				}
			}

			SlotsToRemove.Emplace(ActiveSlot.BarId, ActiveSlot.QuickSlotId);
		}
		for (const TPair<FGameplayTag, FGameplayTag>& Pair : SlotsToRemove)
		{
			ArcMassItems::QuickBar::RemoveSlot(EntityManager, Entity, Pair.Key, Pair.Value);
		}
	}

	void AddItemToMatchingQuickBarSlots(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcMassQuickBarSharedFragment& Shared,
		FArcItemId ItemId,
		FGameplayTag ItemSlotTag,
		const UScriptStruct* StoreType)
	{
		if (!ItemSlotTag.IsValid())
		{
			return;
		}

		for (const TObjectPtr<UArcMassQuickBarConfig>& Config : Shared.Configs)
		{
			if (!Config || Config->StoreType.Get() != StoreType)
			{
				continue;
			}

			ArcMassItems::QuickBar::ForEachBarConfig(*Config,
				[&](const FArcMassQuickBarEntry& BarEntry, const FArcMassQuickBarGroup*) -> bool
				{
					if (BarEntry.ParentBarId.IsValid())
					{
						return true;
					}

					for (const FArcMassQuickBarSlot& QSlot : BarEntry.Slots)
					{
						if (QSlot.ItemSlotId != ItemSlotTag)
						{
							continue;
						}

						const FArcItemData* ItemData = ArcMassItems::QuickBar::Internal::GetItemFromStore(
							EntityManager, Entity, StoreType, ItemId);

						if (!ArcMassItems::QuickBar::Internal::ItemMatchesRequiredTags(ItemData, BarEntry.ItemRequiredTags) ||
							!ArcMassItems::QuickBar::Internal::ItemMatchesRequiredTags(ItemData, QSlot.ItemRequiredTags))
						{
							continue;
						}

						ArcMassItems::QuickBar::AddAndActivateSlot(
							EntityManager, Entity, BarEntry.BarId, QSlot.QuickBarSlotId, ItemId);
					}
					return true;
				});
		}
	}
}

namespace ArcMassItems::QuickBar::AutoSelect
{

void HandleSlotAssigned(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcItemId ItemId,
	FGameplayTag NewSlot,
	const UScriptStruct* StoreType)
{
	const FArcMassQuickBarSharedFragment* Shared =
		ArcMassQuickBarAutoSelect_Internal::ResolveSharedFragment(EntityManager, Entity, StoreType);
	if (!Shared)
	{
		return;
	}

	ArcMassQuickBarAutoSelect_Internal::AddItemToMatchingQuickBarSlots(
		EntityManager, Entity, *Shared, ItemId, NewSlot, StoreType);
}

void HandleSlotCleared(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcItemId ItemId,
	const UScriptStruct* StoreType)
{
	const FArcMassQuickBarSharedFragment* Shared =
		ArcMassQuickBarAutoSelect_Internal::ResolveSharedFragment(EntityManager, Entity, StoreType);
	if (!Shared)
	{
		return;
	}

	ArcMassQuickBarAutoSelect_Internal::RemoveItemFromQuickBarSlots(
		EntityManager, Entity, ItemId, StoreType, /*bOnlyTagManaged=*/false);
}

void HandleSlotChanged(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcItemId ItemId,
	FGameplayTag NewSlot,
	const UScriptStruct* StoreType)
{
	const FArcMassQuickBarSharedFragment* Shared =
		ArcMassQuickBarAutoSelect_Internal::ResolveSharedFragment(EntityManager, Entity, StoreType);
	if (!Shared)
	{
		return;
	}

	const bool bIsClear = !NewSlot.IsValid();
	ArcMassQuickBarAutoSelect_Internal::RemoveItemFromQuickBarSlots(
		EntityManager, Entity, ItemId, StoreType, /*bOnlyTagManaged=*/!bIsClear);
	ArcMassQuickBarAutoSelect_Internal::AddItemToMatchingQuickBarSlots(
		EntityManager, Entity, *Shared, ItemId, NewSlot, StoreType);
}

}
