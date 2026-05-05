// Copyright Lukasz Baran. All Rights Reserved.

#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarTypes.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "QuickBar/ArcMassQuickBarHandlers.h"
#include "QuickBar/ArcMassQuickBarInputBinding.h"
#include "QuickBar/ArcMassQuickBarInternal.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"

#include "MassEntityManager.h"
#include "MassEntityView.h"

namespace ArcMassItems::QuickBar::Internal
{
	bool ItemMatchesRequiredTags(const FArcItemData* ItemData, const FGameplayTagContainer& Required)
	{
		if (Required.IsEmpty())
		{
			return true;
		}
		if (!ItemData)
		{
			return false;
		}
		return ItemData->GetItemAggregatedTags().HasAll(Required);
	}

	const FArcItemData* GetItemFromStore(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FArcItemId ItemId)
	{
		FStructView View = EntityManager.GetMutableSparseElementDataForEntity(StoreType, Entity);
		FArcMassItemStoreFragment* Store = View.GetPtr<FArcMassItemStoreFragment>();
		if (!Store)
		{
			return nullptr;
		}
		FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
		return Item ? Item->ToItem() : nullptr;
	}
}

namespace
{
	const FArcMassQuickBarEntry* FindBarConfig(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId)
	{
		FMassEntityView View(EntityManager, Entity);
		const FArcMassQuickBarSharedFragment& Shared = View.GetConstSharedFragmentData<FArcMassQuickBarSharedFragment>();
		if (!Shared.Config)
		{
			return nullptr;
		}
		return Shared.Config->QuickBars.FindByPredicate(
			[BarId](const FArcMassQuickBarEntry& B) { return B.BarId == BarId; });
	}

	const FArcMassQuickBarSlot* FindSlotConfig(
		const FArcMassQuickBarEntry& Bar,
		FGameplayTag SlotId)
	{
		return Bar.Slots.FindByPredicate(
			[SlotId](const FArcMassQuickBarSlot& S) { return S.QuickBarSlotId == SlotId; });
	}

	FArcMassQuickBarStateFragment* GetState(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity)
	{
		FStructView View = EntityManager.GetMutableSparseElementDataForEntity(
			FArcMassQuickBarStateFragment::StaticStruct(), Entity);
		return View.GetPtr<FArcMassQuickBarStateFragment>();
	}

	FArcMassQuickBarActiveSlot* FindActiveSlot(
		FArcMassQuickBarStateFragment& State,
		FGameplayTag BarId,
		FGameplayTag SlotId)
	{
		return State.ActiveSlots.FindByPredicate(
			[BarId, SlotId](const FArcMassQuickBarActiveSlot& S)
			{
				return S.BarId == BarId && S.QuickSlotId == SlotId;
			});
	}
}

namespace ArcMassItems::QuickBar
{

void AddAndActivateSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	FGameplayTag BarId,
	FGameplayTag SlotId,
	FArcItemId ItemId)
{
	const FArcMassQuickBarEntry* Bar = FindBarConfig(EntityManager, Entity, BarId);
	if (!Bar)
	{
		return;
	}

	const FArcMassQuickBarSlot* QSlot = FindSlotConfig(*Bar, SlotId);
	if (!QSlot)
	{
		return;
	}

	FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
	if (!State)
	{
		return;
	}

	if (FindActiveSlot(*State, BarId, SlotId))
	{
		return;
	}

	const FArcItemData* ItemData = Internal::GetItemFromStore(EntityManager, Entity, StoreType, ItemId);

	FArcMassQuickBarActiveSlot NewActiveSlot;
	NewActiveSlot.BarId = BarId;
	NewActiveSlot.QuickSlotId = SlotId;
	NewActiveSlot.AssignedItemId = ItemId;
	NewActiveSlot.ItemSlot = ItemData ? ItemData->GetSlotId() : FGameplayTag();
	NewActiveSlot.bIsSlotActive = false;

	State->ActiveSlots.Add(NewActiveSlot);

	if (Bar->QuickSlotsMode == EArcMassQuickSlotsMode::AutoActivateOnly || QSlot->bAutoSelect)
	{
		ActivateSlot(EntityManager, Entity, StoreType, BarId, SlotId);
	}
}

void RemoveSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	FGameplayTag BarId,
	FGameplayTag SlotId)
{
	FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
	if (!State)
	{
		return;
	}

	FArcMassQuickBarActiveSlot* ActiveSlot = FindActiveSlot(*State, BarId, SlotId);
	if (!ActiveSlot)
	{
		return;
	}

	if (ActiveSlot->bIsSlotActive)
	{
		DeactivateSlot(EntityManager, Entity, StoreType, BarId, SlotId);
	}

	State->ActiveSlots.RemoveAll(
		[BarId, SlotId](const FArcMassQuickBarActiveSlot& S)
		{
			return S.BarId == BarId && S.QuickSlotId == SlotId;
		});
}

bool ActivateSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	FGameplayTag BarId,
	FGameplayTag SlotId)
{
	const FArcMassQuickBarEntry* Bar = FindBarConfig(EntityManager, Entity, BarId);
	if (!Bar)
	{
		return false;
	}

	const FArcMassQuickBarSlot* QSlot = FindSlotConfig(*Bar, SlotId);
	if (!QSlot)
	{
		return false;
	}

	FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
	if (!State)
	{
		return false;
	}

	FArcMassQuickBarActiveSlot* ActiveSlot = FindActiveSlot(*State, BarId, SlotId);
	if (!ActiveSlot)
	{
		return false;
	}

	if (ActiveSlot->bIsSlotActive)
	{
		return true;
	}

	const FArcItemData* ItemData = Internal::GetItemFromStore(EntityManager, Entity, StoreType, ActiveSlot->AssignedItemId);

	for (const FInstancedStruct& IS : QSlot->SelectedHandlers)
	{
		const FArcMassQuickSlotHandler* Handler = IS.GetPtr<FArcMassQuickSlotHandler>();
		if (Handler)
		{
			Handler->OnSlotSelected(EntityManager, Entity, ItemData, Bar, QSlot);
		}
	}

	const FArcMassQuickBarInputBinding* InputBind = QSlot->InputBind.GetPtr<FArcMassQuickBarInputBinding>();
	if (InputBind && InputBind->InputTag.IsValid() && ItemData)
	{
		InputBinding::BindInputForItem(EntityManager, Entity, ItemData, InputBind->InputTag);
	}

	ActiveSlot->bIsSlotActive = true;
	return true;
}

bool DeactivateSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	FGameplayTag BarId,
	FGameplayTag SlotId)
{
	const FArcMassQuickBarEntry* Bar = FindBarConfig(EntityManager, Entity, BarId);
	if (!Bar)
	{
		return false;
	}

	const FArcMassQuickBarSlot* QSlot = FindSlotConfig(*Bar, SlotId);
	if (!QSlot)
	{
		return false;
	}

	FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
	if (!State)
	{
		return false;
	}

	FArcMassQuickBarActiveSlot* ActiveSlot = FindActiveSlot(*State, BarId, SlotId);
	if (!ActiveSlot)
	{
		return false;
	}

	if (!ActiveSlot->bIsSlotActive)
	{
		return true;
	}

	const FArcItemData* ItemData = Internal::GetItemFromStore(EntityManager, Entity, StoreType, ActiveSlot->AssignedItemId);

	for (const FInstancedStruct& IS : QSlot->SelectedHandlers)
	{
		const FArcMassQuickSlotHandler* Handler = IS.GetPtr<FArcMassQuickSlotHandler>();
		if (Handler)
		{
			Handler->OnSlotDeselected(EntityManager, Entity, ItemData, Bar, QSlot);
		}
	}

	const FArcMassQuickBarInputBinding* InputBind = QSlot->InputBind.GetPtr<FArcMassQuickBarInputBinding>();
	if (InputBind && InputBind->InputTag.IsValid() && ItemData)
	{
		InputBinding::UnbindInputForItem(EntityManager, Entity, ItemData, InputBind->InputTag);
	}

	ActiveSlot->bIsSlotActive = false;
	return true;
}

FGameplayTag CycleSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	FGameplayTag BarId,
	FGameplayTag CurrentSlotId,
	int32 Direction,
	TFunction<bool(const FArcItemData*)> SlotValidCondition)
{
	const FArcMassQuickBarEntry* Bar = FindBarConfig(EntityManager, Entity, BarId);
	if (!Bar)
	{
		return FGameplayTag::EmptyTag;
	}

	if (Bar->QuickSlotsMode != EArcMassQuickSlotsMode::Cyclable)
	{
		return FGameplayTag::EmptyTag;
	}

	const int32 Num = Bar->Slots.Num();
	if (Num == 0)
	{
		return FGameplayTag::EmptyTag;
	}

	int32 StartIdx = Bar->Slots.IndexOfByPredicate(
		[CurrentSlotId](const FArcMassQuickBarSlot& S) { return S.QuickBarSlotId == CurrentSlotId; });
	if (StartIdx == INDEX_NONE)
	{
		return FGameplayTag::EmptyTag;
	}

	FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
	if (!State)
	{
		return FGameplayTag::EmptyTag;
	}

	int32 Idx = StartIdx;
	do
	{
		Idx = ((Idx + Direction) % Num + Num) % Num;

		const FArcMassQuickBarSlot& QSlot = Bar->Slots[Idx];

		bool bValidatorsPass = true;
		for (const FInstancedStruct& V : QSlot.Validators)
		{
			const FArcMassQuickBarSlotValidator* Validator = V.GetPtr<FArcMassQuickBarSlotValidator>();
			if (Validator && !Validator->IsValid(EntityManager, Entity, *Bar, QSlot))
			{
				bValidatorsPass = false;
				break;
			}
		}
		if (!bValidatorsPass)
		{
			continue;
		}

		FArcMassQuickBarActiveSlot* ActiveSlot = FindActiveSlot(*State, BarId, QSlot.QuickBarSlotId);
		if (!ActiveSlot || !ActiveSlot->AssignedItemId.IsValid())
		{
			continue;
		}

		const FArcItemData* Data = Internal::GetItemFromStore(EntityManager, Entity, StoreType, ActiveSlot->AssignedItemId);
		if (!Data)
		{
			continue;
		}

		if (!Internal::ItemMatchesRequiredTags(Data, Bar->ItemRequiredTags) ||
			!Internal::ItemMatchesRequiredTags(Data, QSlot.ItemRequiredTags))
		{
			continue;
		}

		if (!SlotValidCondition || SlotValidCondition(Data))
		{
			DeactivateSlot(EntityManager, Entity, StoreType, BarId, CurrentSlotId);
			ActivateSlot(EntityManager, Entity, StoreType, BarId, QSlot.QuickBarSlotId);
			return QSlot.QuickBarSlotId;
		}

	} while (Idx != StartIdx);

	return FGameplayTag::EmptyTag;
}

void ActivateBar(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	FGameplayTag BarId)
{
	const FArcMassQuickBarEntry* Bar = FindBarConfig(EntityManager, Entity, BarId);
	if (!Bar)
	{
		return;
	}

	for (const FArcMassQuickBarSlot& QSlot : Bar->Slots)
	{
		ActivateSlot(EntityManager, Entity, StoreType, BarId, QSlot.QuickBarSlotId);
	}

	for (const FInstancedStruct& IS : Bar->QuickBarSelectedActions)
	{
		const FArcMassQuickBarSelectedAction* Action = IS.GetPtr<FArcMassQuickBarSelectedAction>();
		if (Action)
		{
			Action->QuickBarActivated(EntityManager, Entity, *Bar);
		}
	}
}

void DeactivateBar(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	FGameplayTag BarId)
{
	const FArcMassQuickBarEntry* Bar = FindBarConfig(EntityManager, Entity, BarId);
	if (!Bar)
	{
		return;
	}

	for (const FArcMassQuickBarSlot& QSlot : Bar->Slots)
	{
		DeactivateSlot(EntityManager, Entity, StoreType, BarId, QSlot.QuickBarSlotId);
	}

	for (const FInstancedStruct& IS : Bar->QuickBarSelectedActions)
	{
		const FArcMassQuickBarSelectedAction* Action = IS.GetPtr<FArcMassQuickBarSelectedAction>();
		if (Action)
		{
			Action->QuickBarDeactivated(EntityManager, Entity, *Bar);
		}
	}
}

FGameplayTag CycleBar(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	FGameplayTag OldBarId,
	int32 Direction)
{
	FMassEntityView View(EntityManager, Entity);
	const FArcMassQuickBarSharedFragment& Shared = View.GetConstSharedFragmentData<FArcMassQuickBarSharedFragment>();
	if (!Shared.Config)
	{
		return FGameplayTag::EmptyTag;
	}

	const TArray<FArcMassQuickBarEntry>& Bars = Shared.Config->QuickBars;
	const int32 NumBars = Bars.Num();
	if (NumBars == 0)
	{
		return FGameplayTag::EmptyTag;
	}

	int32 StartIdx = Bars.IndexOfByPredicate(
		[OldBarId](const FArcMassQuickBarEntry& B) { return B.BarId == OldBarId; });
	if (StartIdx == INDEX_NONE)
	{
		return FGameplayTag::EmptyTag;
	}

	FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
	if (!State)
	{
		return FGameplayTag::EmptyTag;
	}

	int32 Idx = StartIdx;
	for (int32 Step = 0; Step < NumBars - 1; ++Step)
	{
		Idx = ((Idx + Direction) % NumBars + NumBars) % NumBars;

		const FArcMassQuickBarEntry& NewBar = Bars[Idx];

		bool bAllSlotsValid = true;
		for (const FArcMassQuickBarSlot& QSlot : NewBar.Slots)
		{
			FArcMassQuickBarActiveSlot* ActiveSlot = FindActiveSlot(*State, NewBar.BarId, QSlot.QuickBarSlotId);
			if (!ActiveSlot || !ActiveSlot->AssignedItemId.IsValid())
			{
				bAllSlotsValid = false;
				break;
			}

			const FArcItemData* Data = Internal::GetItemFromStore(EntityManager, Entity, StoreType, ActiveSlot->AssignedItemId);
			if (!Data)
			{
				bAllSlotsValid = false;
				break;
			}

			if (!Internal::ItemMatchesRequiredTags(Data, NewBar.ItemRequiredTags) ||
				!Internal::ItemMatchesRequiredTags(Data, QSlot.ItemRequiredTags))
			{
				bAllSlotsValid = false;
				break;
			}
		}

		if (bAllSlotsValid)
		{
			DeactivateBar(EntityManager, Entity, StoreType, OldBarId);
			ActivateBar(EntityManager, Entity, StoreType, NewBar.BarId);
			return NewBar.BarId;
		}
	}

	return FGameplayTag::EmptyTag;
}

bool IsSlotActive(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId,
	FGameplayTag SlotId)
{
	FMassEntityView View(EntityManager, Entity);
	const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
	if (!State)
	{
		return false;
	}

	const FArcMassQuickBarActiveSlot* Slot = State->ActiveSlots.FindByPredicate(
		[BarId, SlotId](const FArcMassQuickBarActiveSlot& S)
		{
			return S.BarId == BarId && S.QuickSlotId == SlotId;
		});

	return Slot && Slot->bIsSlotActive;
}

FArcItemId GetAssignedItem(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId,
	FGameplayTag SlotId)
{
	FMassEntityView View(EntityManager, Entity);
	const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
	if (!State)
	{
		return FArcItemId();
	}

	const FArcMassQuickBarActiveSlot* Slot = State->ActiveSlots.FindByPredicate(
		[BarId, SlotId](const FArcMassQuickBarActiveSlot& S)
		{
			return S.BarId == BarId && S.QuickSlotId == SlotId;
		});

	return Slot ? Slot->AssignedItemId : FArcItemId();
}

TArray<FGameplayTag> GetActiveSlots(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId)
{
	TArray<FGameplayTag> Result;

	FMassEntityView View(EntityManager, Entity);
	const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
	if (!State)
	{
		return Result;
	}

	for (const FArcMassQuickBarActiveSlot& AS : State->ActiveSlots)
	{
		if (AS.BarId == BarId)
		{
			Result.Add(AS.QuickSlotId);
		}
	}

	return Result;
}

} // namespace ArcMassItems::QuickBar
