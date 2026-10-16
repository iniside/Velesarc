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
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsHelpers.h"
#include "Operations/ArcMassItemOperations.h"

#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassQuickBar, Log, All);

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
		if (!StoreType)
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
}

namespace ArcMassQuickBarOperations_Private
{
	const FArcMassQuickBarSharedFragment* GetQuickBarShared(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			return nullptr;
		}
		FMassEntityView View(EntityManager, Entity);
		return View.GetConstSharedFragmentDataPtr<FArcMassQuickBarSharedFragment>();
	}

	FArcMassQuickBarLocation FindBarInShared(
		const FArcMassQuickBarSharedFragment& Shared,
		FGameplayTag BarId)
	{
		FArcMassQuickBarLocation Location;
		for (const TObjectPtr<UArcMassQuickBarConfig>& Config : Shared.Configs)
		{
			if (!Config)
			{
				continue;
			}
			ArcMassItems::QuickBar::ForEachBarConfig(*Config,
				[BarId, &Config, &Location](const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarGroup* Group) -> bool
				{
					if (Bar.BarId != BarId)
					{
						return true;
					}
					Location.Config = Config;
					Location.Bar = &Bar;
					Location.Group = Group;
					return false;
				});
			if (Location.Bar)
			{
				break;
			}
		}
		return Location;
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

	FArcMassQuickBarBarState* FindBarState(
		FArcMassQuickBarStateFragment& State,
		FGameplayTag BarId)
	{
		return State.BarStates.FindByPredicate(
			[BarId](const FArcMassQuickBarBarState& B) { return B.BarId == BarId; });
	}

	bool BarHasActiveSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId)
	{
		const FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
		if (!State)
		{
			return false;
		}
		return State->ActiveSlots.ContainsByPredicate(
			[BarId](const FArcMassQuickBarActiveSlot& S) { return S.BarId == BarId && S.bIsSlotActive; });
	}

	bool SlotHoldsItem(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FGameplayTag BarId,
		FGameplayTag SlotId)
	{
		const FArcItemId ItemId = ArcMassItems::QuickBar::GetAssignedItem(EntityManager, Entity, BarId, SlotId);
		return ItemId.IsValid() && ArcMassItems::QuickBar::Internal::GetItemFromStore(EntityManager, Entity, StoreType, ItemId) != nullptr;
	}

	bool IsSlotSelectable(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		const FArcMassQuickBarEntry& Bar,
		const FArcMassQuickBarSlot& QSlot,
		const TFunction<bool(const FArcItemData*)>& SlotValidCondition)
	{
		for (const FInstancedStruct& V : QSlot.Validators)
		{
			const FArcMassQuickBarSlotValidator* Validator = V.GetPtr<FArcMassQuickBarSlotValidator>();
			if (Validator && !Validator->IsValid(EntityManager, Entity, Bar, QSlot))
			{
				return false;
			}
		}

		const FArcItemId ItemId = ArcMassItems::QuickBar::GetAssignedItem(EntityManager, Entity, Bar.BarId, QSlot.QuickBarSlotId);
		if (!ItemId.IsValid())
		{
			return false;
		}

		const FArcItemData* Data = ArcMassItems::QuickBar::Internal::GetItemFromStore(EntityManager, Entity, StoreType, ItemId);
		if (!Data)
		{
			return false;
		}

		if (!ArcMassItems::QuickBar::Internal::ItemMatchesRequiredTags(Data, Bar.ItemRequiredTags) ||
			!ArcMassItems::QuickBar::Internal::ItemMatchesRequiredTags(Data, QSlot.ItemRequiredTags))
		{
			return false;
		}

		return !SlotValidCondition || SlotValidCondition(Data);
	}

	bool BarHoldsAnyItem(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		const FArcMassQuickBarEntry& Bar)
	{
		for (const FArcMassQuickBarSlot& QSlot : Bar.Slots)
		{
			if (SlotHoldsItem(EntityManager, Entity, StoreType, Bar.BarId, QSlot.QuickBarSlotId))
			{
				return true;
			}
		}
		return false;
	}

	FArcMassQuickBarLocation FindChildBar(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag ParentBarId)
	{
		FArcMassQuickBarLocation ChildBar;
		const FArcMassQuickBarSharedFragment* Shared = GetQuickBarShared(EntityManager, Entity);
		if (!Shared || !ParentBarId.IsValid())
		{
			return ChildBar;
		}
		for (const TObjectPtr<UArcMassQuickBarConfig>& Config : Shared->Configs)
		{
			if (!Config)
			{
				continue;
			}
			ArcMassItems::QuickBar::ForEachBarConfig(*Config,
				[ParentBarId, &Config, &ChildBar](const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarGroup* Group) -> bool
				{
					if (Bar.ParentBarId != ParentBarId)
					{
						return true;
					}
					ChildBar.Config = Config;
					ChildBar.Bar = &Bar;
					ChildBar.Group = Group;
					return false;
				});
			if (ChildBar.Bar)
			{
				break;
			}
		}
		return ChildBar;
	}

	void CreateChildItems(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag ParentBarId,
		FGameplayTag ParentSlotId,
		const FArcMassQuickBarLocation& ChildBar,
		const TArray<FArcMassQuickBarItem>& ItemsToAdd)
	{
		const UScriptStruct* ChildStoreType = ChildBar.Config->StoreType;
		if (!ChildStoreType)
		{
			UE_LOGF(LogArcMassQuickBar, Warning, "CreateChildItems: child bar %ls has no store type", *ChildBar.Bar->BarId.ToString());
			return;
		}

		const int32 SlotCount = ChildBar.Bar->Slots.Num();
		if (ItemsToAdd.Num() > SlotCount)
		{
			UE_LOGF(LogArcMassQuickBar, Warning, "CreateChildItems: more items (%d) than slots (%d) in child bar %ls; extra items skipped",
				ItemsToAdd.Num(), SlotCount, *ChildBar.Bar->BarId.ToString());
		}

		TArray<FArcItemId> CreatedIds;
		const int32 NumToCreate = FMath::Min(ItemsToAdd.Num(), SlotCount);
		for (int32 Idx = 0; Idx < NumToCreate; ++Idx)
		{
			const FPrimaryAssetId& Definition = ItemsToAdd[Idx].Item;
			if (!Definition.IsValid())
			{
				UE_LOGF(LogArcMassQuickBar, Warning, "CreateChildItems: null item %d for child bar %ls", Idx, *ChildBar.Bar->BarId.ToString());
				continue;
			}

			const FArcItemId NewItemId = ArcMassItems::AddItem(EntityManager, Entity, ChildStoreType, FArcItemSpec::NewItem(Definition, 1, 1));
			if (!NewItemId.IsValid())
			{
				UE_LOGF(LogArcMassQuickBar, Warning, "CreateChildItems: AddItem failed for %ls", *Definition.ToString());
				continue;
			}
			CreatedIds.Add(NewItemId);
		}

		FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
		FArcMassQuickBarActiveSlot* ParentSlot = State ? FindActiveSlot(*State, ParentBarId, ParentSlotId) : nullptr;
		if (!ParentSlot)
		{
			for (const FArcItemId& CreatedId : CreatedIds)
			{
				ArcMassItems::DestroyItem(EntityManager, Entity, ChildStoreType, CreatedId);
			}
			return;
		}
		ParentSlot->ChildItemIds = MoveTemp(CreatedIds);
	}

	void UnbindChildBar(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcMassQuickBarLocation& ChildBar)
	{
		const UScriptStruct* ChildStoreType = ChildBar.Config->StoreType;
		const FGameplayTag ChildBarId = ChildBar.Bar->BarId;
		for (const FArcMassQuickBarSlot& ChildSlot : ChildBar.Bar->Slots)
		{
			const FArcItemId ChildItemId = ArcMassItems::QuickBar::GetAssignedItem(EntityManager, Entity, ChildBarId, ChildSlot.QuickBarSlotId);
			ArcMassItems::QuickBar::RemoveSlot(EntityManager, Entity, ChildBarId, ChildSlot.QuickBarSlotId);

			const FArcItemData* ChildItem = ChildItemId.IsValid()
				? ArcMassItems::QuickBar::Internal::GetItemFromStore(EntityManager, Entity, ChildStoreType, ChildItemId)
				: nullptr;
			// Every parent slot's children share these item slots; activation requires a slotted item (ArcMassItemOperations.cpp:162), so each bind re-slots.
			if (ChildItem && ChildSlot.ItemSlotId.IsValid() && ChildItem->GetSlotId() == ChildSlot.ItemSlotId)
			{
				ArcMassItems::RemoveItemFromSlot(EntityManager, Entity, ChildStoreType, ChildItemId);
			}
		}
	}

	void BindChildBar(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag ParentBarId,
		FGameplayTag ParentSlotId,
		const FArcMassQuickBarLocation& ChildBar)
	{
		const UScriptStruct* ChildStoreType = ChildBar.Config->StoreType;
		const FGameplayTag ChildBarId = ChildBar.Bar->BarId;

		UnbindChildBar(EntityManager, Entity, ChildBar);

		FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
		FArcMassQuickBarActiveSlot* ParentSlot = State ? FindActiveSlot(*State, ParentBarId, ParentSlotId) : nullptr;
		if (!ParentSlot)
		{
			return;
		}

		TArray<FArcItemId> LiveIds;
		for (const FArcItemId& ChildItemId : ParentSlot->ChildItemIds)
		{
			if (ArcMassItems::QuickBar::Internal::GetItemFromStore(EntityManager, Entity, ChildStoreType, ChildItemId))
			{
				LiveIds.Add(ChildItemId);
			}
		}
		if (LiveIds.Num() != ParentSlot->ChildItemIds.Num())
		{
			ParentSlot->ChildItemIds = LiveIds;
		}

		const int32 NumToBind = FMath::Min(LiveIds.Num(), ChildBar.Bar->Slots.Num());
		for (int32 Idx = 0; Idx < NumToBind; ++Idx)
		{
			const FArcMassQuickBarSlot& ChildSlot = ChildBar.Bar->Slots[Idx];
			const FArcItemId ChildItemId = LiveIds[Idx];
			const FArcItemData* ChildItem = ArcMassItems::QuickBar::Internal::GetItemFromStore(EntityManager, Entity, ChildStoreType, ChildItemId);
			if (!ChildItem)
			{
				continue;
			}

			if (ChildItem->GetSlotId() != ChildSlot.ItemSlotId
				&& !ArcMassItems::AddItemToSlot(EntityManager, Entity, ChildStoreType, ChildItemId, ChildSlot.ItemSlotId))
			{
				UE_LOGF(LogArcMassQuickBar, Warning, "BindChildBar: item slot %ls of child Bar=%ls Slot=%ls is occupied; %ls stays unslotted",
					*ChildSlot.ItemSlotId.ToString(), *ChildBarId.ToString(), *ChildSlot.QuickBarSlotId.ToString(), *ChildItemId.ToString());
				continue;
			}

			ArcMassItems::QuickBar::AddAndActivateSlot(EntityManager, Entity, ChildBarId, ChildSlot.QuickBarSlotId, ChildItemId);
			ArcMassItems::QuickBar::ActivateSlot(EntityManager, Entity, ChildBarId, ChildSlot.QuickBarSlotId);
		}
	}

	TArray<FGameplayTag> GetSelectedSlotIds(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId)
	{
		TArray<FGameplayTag> Result;
		const FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
		if (!State)
		{
			return Result;
		}
		for (const FArcMassQuickBarActiveSlot& ActiveSlot : State->ActiveSlots)
		{
			if (ActiveSlot.BarId == BarId && ActiveSlot.bIsSlotActive)
			{
				Result.Add(ActiveSlot.QuickSlotId);
			}
		}
		return Result;
	}

	void AppendBarSelection(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcMassQuickBarEntry& Bar,
		TArray<FArcMassQuickBarBarSelection>& OutSelection)
	{
		FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
		const FArcMassQuickBarBarState* BarState = State ? FindBarState(*State, Bar.BarId) : nullptr;
		if (!BarState)
		{
			return;
		}
		FArcMassQuickBarBarSelection& Selection = OutSelection.AddDefaulted_GetRef();
		Selection.BarId = Bar.BarId;
		Selection.bIsBarActive = BarState->bIsBarActive;
		Selection.LastActiveSlotId = BarState->LastActiveSlotId;
		Selection.ActiveSlotIds = GetSelectedSlotIds(EntityManager, Entity, Bar.BarId);
	}

	void SetLastActiveSlotId(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FGameplayTag BarId,
		FGameplayTag SlotId)
	{
		FArcMassQuickBarStateFragment* State = GetState(EntityManager, Entity);
		FArcMassQuickBarBarState* BarState = State ? FindBarState(*State, BarId) : nullptr;
		if (BarState)
		{
			BarState->LastActiveSlotId = SlotId;
		}
	}
}

namespace ArcMassItems::QuickBar
{

void ForEachBarConfig(
	const UArcMassQuickBarConfig& Config,
	TFunctionRef<bool(const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarGroup* Group)> Visitor)
{
	for (const FArcMassQuickBarEntry& Bar : Config.QuickBars)
	{
		if (!Visitor(Bar, nullptr))
		{
			return;
		}
	}
	for (const FArcMassQuickBarGroup& Group : Config.Groups)
	{
		for (const FArcMassQuickBarEntry& Bar : Group.Bars)
		{
			if (!Visitor(Bar, &Group))
			{
				return;
			}
		}
	}
}

void SeedBarStates(
	const TArray<TObjectPtr<UArcMassQuickBarConfig>>& Configs,
	FArcMassQuickBarStateFragment& OutState)
{
	OutState.BarStates.Reset();
	for (const TObjectPtr<UArcMassQuickBarConfig>& Config : Configs)
	{
		if (!Config)
		{
			continue;
		}
		ForEachBarConfig(*Config,
			[&OutState](const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarGroup*) -> bool
			{
				if (Bar.ParentBarId.IsValid())
				{
					return true;
				}
				FArcMassQuickBarBarState& BarState = OutState.BarStates.AddDefaulted_GetRef();
				BarState.BarId = Bar.BarId;
				BarState.bIsBarActive = Bar.bActiveOnStart;
				return true;
			});
	}
}

FArcMassQuickBarLocation FindBar(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId)
{
	const FArcMassQuickBarSharedFragment* Shared = ArcMassQuickBarOperations_Private::GetQuickBarShared(EntityManager, Entity);
	if (!Shared)
	{
		return FArcMassQuickBarLocation();
	}
	return ArcMassQuickBarOperations_Private::FindBarInShared(*Shared, BarId);
}

FArcMassQuickBarLocation FindGroup(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag GroupId)
{
	FArcMassQuickBarLocation Location;
	const FArcMassQuickBarSharedFragment* Shared = ArcMassQuickBarOperations_Private::GetQuickBarShared(EntityManager, Entity);
	if (!Shared)
	{
		return Location;
	}
	for (const TObjectPtr<UArcMassQuickBarConfig>& Config : Shared->Configs)
	{
		if (!Config)
		{
			continue;
		}
		const FArcMassQuickBarGroup* Group = Config->Groups.FindByPredicate(
			[GroupId](const FArcMassQuickBarGroup& G) { return G.GroupId == GroupId; });
		if (Group)
		{
			Location.Config = Config;
			Location.Group = Group;
			break;
		}
	}
	return Location;
}

void AddAndActivateSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId,
	FGameplayTag SlotId,
	FArcItemId ItemId)
{
	const FArcMassQuickBarLocation Location = FindBar(EntityManager, Entity, BarId);
	const FArcMassQuickBarEntry* Bar = Location.Bar;
	if (!Bar)
	{
		UE_LOGF(LogArcMassQuickBar, Warning, "AddAndActivateSlot: no bar config %ls", *BarId.ToString());
		return;
	}
	const UScriptStruct* StoreType = Location.Config->StoreType;

	const FArcMassQuickBarSlot* QSlot = ArcMassQuickBarOperations_Private::FindSlotConfig(*Bar, SlotId);
	if (!QSlot)
	{
		UE_LOGF(LogArcMassQuickBar, Warning, "AddAndActivateSlot: no slot config %ls in bar %ls", *SlotId.ToString(), *BarId.ToString());
		return;
	}

	FArcMassQuickBarStateFragment* State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
	if (!State)
	{
		return;
	}

	if (const FArcMassQuickBarActiveSlot* Existing = ArcMassQuickBarOperations_Private::FindActiveSlot(*State, BarId, SlotId))
	{
		UE_LOGF(LogArcMassQuickBar, Warning, "AddAndActivateSlot: slot %ls in bar %ls already holds item %ls; rejected %ls",
			*SlotId.ToString(), *BarId.ToString(), *Existing->AssignedItemId.ToString(), *ItemId.ToString());
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

	const FArcMassQuickBarLocation ChildBar = ArcMassQuickBarOperations_Private::FindChildBar(EntityManager, Entity, BarId);
	const FArcItemFragment_QuickBarItems* QuickBarItems = (ChildBar.Bar && ItemData) ? ArcItemsHelper::FindFragment<FArcItemFragment_QuickBarItems>(ItemData) : nullptr;
	if (QuickBarItems)
	{
		const TArray<FArcMassQuickBarItem> ItemsToAdd = QuickBarItems->ItemsToAdd;
		ArcMassQuickBarOperations_Private::CreateChildItems(EntityManager, Entity, BarId, SlotId, ChildBar, ItemsToAdd);
	}

	if (Bar->ParentBarId.IsValid() || !IsBarActive(EntityManager, Entity, BarId))
	{
		return;
	}

	switch (Bar->QuickSlotsMode)
	{
	case EArcMassQuickSlotsMode::ManualActivationOnly:
		break;
	case EArcMassQuickSlotsMode::AutoActivateOnly:
		ActivateSlot(EntityManager, Entity, BarId, SlotId);
		break;
	case EArcMassQuickSlotsMode::Cyclable:
		if (QSlot->bAutoSelect && !ArcMassQuickBarOperations_Private::BarHasActiveSlot(EntityManager, Entity, BarId))
		{
			ActivateSlot(EntityManager, Entity, BarId, SlotId);
		}
		break;
	}
}

void RemoveSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId,
	FGameplayTag SlotId)
{
	FArcMassQuickBarStateFragment* State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
	if (!State)
	{
		return;
	}

	FArcMassQuickBarActiveSlot* ActiveSlot = ArcMassQuickBarOperations_Private::FindActiveSlot(*State, BarId, SlotId);
	if (!ActiveSlot)
	{
		return;
	}

	if (ActiveSlot->bIsSlotActive)
	{
		DeactivateSlot(EntityManager, Entity, BarId, SlotId);
	}

	State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
	ActiveSlot = State ? ArcMassQuickBarOperations_Private::FindActiveSlot(*State, BarId, SlotId) : nullptr;
	if (!ActiveSlot)
	{
		return;
	}

	const TArray<FArcItemId> ChildItemIds = ActiveSlot->ChildItemIds;
	if (ChildItemIds.Num() > 0)
	{
		const FArcMassQuickBarLocation ChildBar = ArcMassQuickBarOperations_Private::FindChildBar(EntityManager, Entity, BarId);
		const UScriptStruct* ChildStoreType = ChildBar.Config ? ChildBar.Config->StoreType.Get() : nullptr;
		if (ChildStoreType)
		{
			for (const FArcItemId& ChildItemId : ChildItemIds)
			{
				ArcMassItems::DestroyItem(EntityManager, Entity, ChildStoreType, ChildItemId);
			}
		}
	}

	State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
	if (!State)
	{
		return;
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
	FGameplayTag BarId,
	FGameplayTag SlotId)
{
	const FArcMassQuickBarLocation Location = FindBar(EntityManager, Entity, BarId);
	const FArcMassQuickBarEntry* Bar = Location.Bar;
	if (!Bar)
	{
		return false;
	}
	const UScriptStruct* StoreType = Location.Config->StoreType;

	const FArcMassQuickBarSlot* QSlot = ArcMassQuickBarOperations_Private::FindSlotConfig(*Bar, SlotId);
	if (!QSlot)
	{
		return false;
	}

	if (!Bar->ParentBarId.IsValid() && !IsBarActive(EntityManager, Entity, BarId))
	{
		UE_LOGF(LogArcMassQuickBar, Warning, "ActivateSlot: bar %ls is inactive; slot %ls not activated", *BarId.ToString(), *SlotId.ToString());
		return false;
	}

	FArcMassQuickBarStateFragment* State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
	if (!State)
	{
		return false;
	}

	FArcMassQuickBarActiveSlot* ActiveSlot = ArcMassQuickBarOperations_Private::FindActiveSlot(*State, BarId, SlotId);
	if (!ActiveSlot)
	{
		return false;
	}

	if (ActiveSlot->bIsSlotActive)
	{
		return true;
	}

	const FArcMassQuickBarLocation ChildBar = ArcMassQuickBarOperations_Private::FindChildBar(EntityManager, Entity, BarId);
	if (ChildBar.Bar || Bar->QuickSlotsMode == EArcMassQuickSlotsMode::Cyclable)
	{
		TArray<FGameplayTag, TInlineAllocator<4>> OtherActiveSlotIds;
		for (const FArcMassQuickBarActiveSlot& Other : State->ActiveSlots)
		{
			if (Other.BarId == BarId && Other.QuickSlotId != SlotId && Other.bIsSlotActive)
			{
				OtherActiveSlotIds.Add(Other.QuickSlotId);
			}
		}
		for (const FGameplayTag& OtherSlotId : OtherActiveSlotIds)
		{
			DeactivateSlot(EntityManager, Entity, BarId, OtherSlotId);
		}

		State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
		ActiveSlot = State ? ArcMassQuickBarOperations_Private::FindActiveSlot(*State, BarId, SlotId) : nullptr;
		if (!ActiveSlot)
		{
			return false;
		}
	}

	const FArcItemData* ItemData = Internal::GetItemFromStore(EntityManager, Entity, StoreType, ActiveSlot->AssignedItemId);

	// Item activation grants the abilities that InputBind binds below.
	if (ItemData && IsItemActivationDeferred(EntityManager, Entity, StoreType, ItemData->GetSlotId()))
	{
		ArcMassItems::ActivateItem(EntityManager, Entity, StoreType, ItemData->GetItemId());
	}

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
		InputBind->Bind(EntityManager, Entity, ItemData);
	}

	State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
	ActiveSlot = State ? ArcMassQuickBarOperations_Private::FindActiveSlot(*State, BarId, SlotId) : nullptr;
	if (!ActiveSlot)
	{
		return false;
	}
	ActiveSlot->bIsSlotActive = true;

	if (ChildBar.Bar)
	{
		ArcMassQuickBarOperations_Private::BindChildBar(EntityManager, Entity, BarId, SlotId, ChildBar);
	}
	return true;
}

bool DeactivateSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId,
	FGameplayTag SlotId)
{
	const FArcMassQuickBarLocation Location = FindBar(EntityManager, Entity, BarId);
	const FArcMassQuickBarEntry* Bar = Location.Bar;
	if (!Bar)
	{
		return false;
	}
	const UScriptStruct* StoreType = Location.Config->StoreType;

	const FArcMassQuickBarSlot* QSlot = ArcMassQuickBarOperations_Private::FindSlotConfig(*Bar, SlotId);
	if (!QSlot)
	{
		return false;
	}

	FArcMassQuickBarStateFragment* State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
	if (!State)
	{
		return false;
	}

	FArcMassQuickBarActiveSlot* ActiveSlot = ArcMassQuickBarOperations_Private::FindActiveSlot(*State, BarId, SlotId);
	if (!ActiveSlot)
	{
		return false;
	}

	if (!ActiveSlot->bIsSlotActive)
	{
		return true;
	}

	const FArcItemId AssignedItemId = ActiveSlot->AssignedItemId;

	const FArcMassQuickBarLocation ChildBar = ArcMassQuickBarOperations_Private::FindChildBar(EntityManager, Entity, BarId);
	if (ChildBar.Bar)
	{
		ArcMassQuickBarOperations_Private::UnbindChildBar(EntityManager, Entity, ChildBar);
	}

	const FArcItemData* ItemData = Internal::GetItemFromStore(EntityManager, Entity, StoreType, AssignedItemId);

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
		InputBind->Unbind(EntityManager, Entity, ItemData);
	}

	if (ItemData && IsItemActivationDeferred(EntityManager, Entity, StoreType, ItemData->GetSlotId()))
	{
		ArcMassItems::DeactivateItem(EntityManager, Entity, StoreType, ItemData->GetItemId());
	}

	State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
	ActiveSlot = State ? ArcMassQuickBarOperations_Private::FindActiveSlot(*State, BarId, SlotId) : nullptr;
	if (!ActiveSlot)
	{
		return false;
	}
	ActiveSlot->bIsSlotActive = false;
	return true;
}

FGameplayTag CycleSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId,
	FGameplayTag CurrentSlotId,
	int32 Direction,
	TFunction<bool(const FArcItemData*)> SlotValidCondition)
{
	const FArcMassQuickBarLocation Location = FindBar(EntityManager, Entity, BarId);
	const FArcMassQuickBarEntry* Bar = Location.Bar;
	if (!Bar)
	{
		return FGameplayTag::EmptyTag;
	}
	const UScriptStruct* StoreType = Location.Config->StoreType;

	if (Bar->QuickSlotsMode != EArcMassQuickSlotsMode::Cyclable || !IsBarActive(EntityManager, Entity, BarId))
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

	int32 Idx = StartIdx;
	do
	{
		Idx = ((Idx + Direction) % Num + Num) % Num;

		const FArcMassQuickBarSlot& QSlot = Bar->Slots[Idx];
		if (ArcMassQuickBarOperations_Private::IsSlotSelectable(EntityManager, Entity, StoreType, *Bar, QSlot, SlotValidCondition))
		{
			DeactivateSlot(EntityManager, Entity, BarId, CurrentSlotId);
			ActivateSlot(EntityManager, Entity, BarId, QSlot.QuickBarSlotId);
			return QSlot.QuickBarSlotId;
		}
	} while (Idx != StartIdx);

	return FGameplayTag::EmptyTag;
}

bool ActivateBar(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId)
{
	const FArcMassQuickBarLocation Location = FindBar(EntityManager, Entity, BarId);
	const FArcMassQuickBarEntry* Bar = Location.Bar;
	if (!Bar || Bar->ParentBarId.IsValid())
	{
		return false;
	}

	if (IsBarActive(EntityManager, Entity, BarId))
	{
		return true;
	}

	if (const FArcMassQuickBarGroup* Group = Location.Group)
	{
		for (const FArcMassQuickBarEntry& Sibling : Group->Bars)
		{
			if (Sibling.BarId != BarId && IsBarActive(EntityManager, Entity, Sibling.BarId))
			{
				DeactivateBar(EntityManager, Entity, Sibling.BarId);
			}
		}
	}

	FArcMassQuickBarStateFragment* State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
	FArcMassQuickBarBarState* BarState = State ? ArcMassQuickBarOperations_Private::FindBarState(*State, BarId) : nullptr;
	if (!BarState)
	{
		UE_LOGF(LogArcMassQuickBar, Warning, "ActivateBar: no state for bar %ls", *BarId.ToString());
		return false;
	}
	BarState->bIsBarActive = true;
	const FGameplayTag LastActiveSlotId = BarState->LastActiveSlotId;

	const UScriptStruct* StoreType = Location.Config->StoreType;
	switch (Bar->QuickSlotsMode)
	{
	case EArcMassQuickSlotsMode::ManualActivationOnly:
		break;
	case EArcMassQuickSlotsMode::AutoActivateOnly:
		for (const FArcMassQuickBarSlot& QSlot : Bar->Slots)
		{
			if (ArcMassQuickBarOperations_Private::SlotHoldsItem(EntityManager, Entity, StoreType, BarId, QSlot.QuickBarSlotId))
			{
				ActivateSlot(EntityManager, Entity, BarId, QSlot.QuickBarSlotId);
			}
		}
		break;
	case EArcMassQuickSlotsMode::Cyclable:
	{
		FGameplayTag SlotToActivate;
		if (LastActiveSlotId.IsValid()
			&& ArcMassQuickBarOperations_Private::SlotHoldsItem(EntityManager, Entity, StoreType, BarId, LastActiveSlotId))
		{
			SlotToActivate = LastActiveSlotId;
		}
		else
		{
			for (const FArcMassQuickBarSlot& QSlot : Bar->Slots)
			{
				if (ArcMassQuickBarOperations_Private::IsSlotSelectable(EntityManager, Entity, StoreType, *Bar, QSlot, nullptr))
				{
					SlotToActivate = QSlot.QuickBarSlotId;
					break;
				}
			}
		}
		if (SlotToActivate.IsValid())
		{
			ActivateSlot(EntityManager, Entity, BarId, SlotToActivate);
		}
		break;
	}
	}

	for (const FInstancedStruct& IS : Bar->QuickBarSelectedActions)
	{
		const FArcMassQuickBarSelectedAction* Action = IS.GetPtr<FArcMassQuickBarSelectedAction>();
		if (Action)
		{
			Action->QuickBarActivated(EntityManager, Entity, *Bar);
		}
	}
	return true;
}

void DeactivateBar(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId)
{
	const FArcMassQuickBarLocation Location = FindBar(EntityManager, Entity, BarId);
	const FArcMassQuickBarEntry* Bar = Location.Bar;
	if (!Bar || Bar->ParentBarId.IsValid() || !IsBarActive(EntityManager, Entity, BarId))
	{
		return;
	}

	FGameplayTag ActiveSlotId;
	for (const FArcMassQuickBarSlot& QSlot : Bar->Slots)
	{
		if (IsSlotActive(EntityManager, Entity, BarId, QSlot.QuickBarSlotId))
		{
			ActiveSlotId = QSlot.QuickBarSlotId;
			break;
		}
	}

	for (const FArcMassQuickBarSlot& QSlot : Bar->Slots)
	{
		DeactivateSlot(EntityManager, Entity, BarId, QSlot.QuickBarSlotId);
	}

	FArcMassQuickBarStateFragment* State = ArcMassQuickBarOperations_Private::GetState(EntityManager, Entity);
	FArcMassQuickBarBarState* BarState = State ? ArcMassQuickBarOperations_Private::FindBarState(*State, BarId) : nullptr;
	if (BarState)
	{
		BarState->bIsBarActive = false;
		if (Bar->QuickSlotsMode == EArcMassQuickSlotsMode::Cyclable)
		{
			BarState->LastActiveSlotId = ActiveSlotId;
		}
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

FGameplayTag CycleActiveBar(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag GroupId,
	int32 Direction)
{
	const FArcMassQuickBarLocation Location = FindGroup(EntityManager, Entity, GroupId);
	const FArcMassQuickBarGroup* Group = Location.Group;
	if (!Group || !Group->bCyclable || Direction == 0)
	{
		return FGameplayTag::EmptyTag;
	}
	const UScriptStruct* StoreType = Location.Config->StoreType;

	const TArray<FArcMassQuickBarEntry>& Bars = Group->Bars;
	const int32 NumBars = Bars.Num();
	if (NumBars == 0)
	{
		return FGameplayTag::EmptyTag;
	}

	const FGameplayTag CurrentBarId = GetActiveBarInGroup(EntityManager, Entity, GroupId);

	int32 StartIdx = INDEX_NONE;
	if (CurrentBarId.IsValid())
	{
		StartIdx = Bars.IndexOfByPredicate(
			[CurrentBarId](const FArcMassQuickBarEntry& B) { return B.BarId == CurrentBarId; });
	}
	if (StartIdx == INDEX_NONE)
	{
		StartIdx = (Direction > 0) ? -1 : 0;
	}

	int32 Idx = StartIdx;
	for (int32 Step = 0; Step < NumBars; ++Step)
	{
		Idx = ((Idx + Direction) % NumBars + NumBars) % NumBars;

		const FArcMassQuickBarEntry& NewBar = Bars[Idx];
		if (NewBar.BarId == CurrentBarId)
		{
			continue;
		}

		if (ArcMassQuickBarOperations_Private::BarHoldsAnyItem(EntityManager, Entity, StoreType, NewBar)
			&& ActivateBar(EntityManager, Entity, NewBar.BarId))
		{
			return NewBar.BarId;
		}
	}

	return FGameplayTag::EmptyTag;
}

TArray<FArcMassQuickBarBarSelection> CaptureBarSelection(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId)
{
	TArray<FArcMassQuickBarBarSelection> Selection;
	const FArcMassQuickBarLocation Location = FindBar(EntityManager, Entity, BarId);
	if (!Location.Bar || Location.Bar->ParentBarId.IsValid())
	{
		return Selection;
	}

	if (Location.Group)
	{
		for (const FArcMassQuickBarEntry& Bar : Location.Group->Bars)
		{
			ArcMassQuickBarOperations_Private::AppendBarSelection(EntityManager, Entity, Bar, Selection);
		}
	}
	else
	{
		ArcMassQuickBarOperations_Private::AppendBarSelection(EntityManager, Entity, *Location.Bar, Selection);
	}
	return Selection;
}

TArray<FArcMassQuickBarBarSelection> CaptureGroupSelection(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag GroupId)
{
	TArray<FArcMassQuickBarBarSelection> Selection;
	const FArcMassQuickBarLocation Location = FindGroup(EntityManager, Entity, GroupId);
	if (!Location.Group)
	{
		return Selection;
	}

	for (const FArcMassQuickBarEntry& Bar : Location.Group->Bars)
	{
		ArcMassQuickBarOperations_Private::AppendBarSelection(EntityManager, Entity, Bar, Selection);
	}
	return Selection;
}

void RestoreSelection(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const TArray<FArcMassQuickBarBarSelection>& Selection)
{
	if (Selection.IsEmpty() || !EntityManager.IsEntityValid(Entity))
	{
		return;
	}

	for (const FArcMassQuickBarBarSelection& Captured : Selection)
	{
		const TArray<FGameplayTag> CurrentSlotIds = ArcMassQuickBarOperations_Private::GetSelectedSlotIds(EntityManager, Entity, Captured.BarId);
		for (const FGameplayTag& SlotId : CurrentSlotIds)
		{
			if (!Captured.ActiveSlotIds.Contains(SlotId))
			{
				DeactivateSlot(EntityManager, Entity, Captured.BarId, SlotId);
			}
		}
	}

	for (const FArcMassQuickBarBarSelection& Captured : Selection)
	{
		if (!Captured.bIsBarActive && IsBarActive(EntityManager, Entity, Captured.BarId))
		{
			DeactivateBar(EntityManager, Entity, Captured.BarId);
		}
	}

	// The written LastActiveSlotId makes a Cyclable bar's own pick in ActivateBar the captured slot.
	for (const FArcMassQuickBarBarSelection& Captured : Selection)
	{
		if (Captured.bIsBarActive && !IsBarActive(EntityManager, Entity, Captured.BarId))
		{
			const FGameplayTag CapturedSlotId = Captured.ActiveSlotIds.IsEmpty() ? FGameplayTag::EmptyTag : Captured.ActiveSlotIds[0];
			ArcMassQuickBarOperations_Private::SetLastActiveSlotId(EntityManager, Entity, Captured.BarId, CapturedSlotId);
			ActivateBar(EntityManager, Entity, Captured.BarId);
		}
	}

	for (const FArcMassQuickBarBarSelection& Captured : Selection)
	{
		for (const FGameplayTag& SlotId : Captured.ActiveSlotIds)
		{
			if (!IsSlotActive(EntityManager, Entity, Captured.BarId, SlotId))
			{
				ActivateSlot(EntityManager, Entity, Captured.BarId, SlotId);
			}
		}

		const TArray<FGameplayTag> CurrentSlotIds = ArcMassQuickBarOperations_Private::GetSelectedSlotIds(EntityManager, Entity, Captured.BarId);
		for (const FGameplayTag& SlotId : CurrentSlotIds)
		{
			if (!Captured.ActiveSlotIds.Contains(SlotId))
			{
				DeactivateSlot(EntityManager, Entity, Captured.BarId, SlotId);
			}
		}
	}

	for (const FArcMassQuickBarBarSelection& Captured : Selection)
	{
		ArcMassQuickBarOperations_Private::SetLastActiveSlotId(EntityManager, Entity, Captured.BarId, Captured.LastActiveSlotId);
	}
}

FGameplayTag GetActiveBarInGroup(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag GroupId)
{
	const FArcMassQuickBarLocation Location = FindGroup(EntityManager, Entity, GroupId);
	if (!Location.Group)
	{
		return FGameplayTag::EmptyTag;
	}
	for (const FArcMassQuickBarEntry& Bar : Location.Group->Bars)
	{
		if (IsBarActive(EntityManager, Entity, Bar.BarId))
		{
			return Bar.BarId;
		}
	}
	return FGameplayTag::EmptyTag;
}

bool IsBarActive(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FGameplayTag BarId)
{
	if (!EntityManager.IsEntityValid(Entity))
	{
		return false;
	}
	FMassEntityView View(EntityManager, Entity);
	const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
	if (!State)
	{
		return false;
	}
	const FArcMassQuickBarBarState* BarState = State->BarStates.FindByPredicate(
		[BarId](const FArcMassQuickBarBarState& B) { return B.BarId == BarId; });
	return BarState && BarState->bIsBarActive;
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

bool IsItemActivationDeferred(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	FGameplayTag ItemSlot)
{
	if (!ItemSlot.IsValid() || !EntityManager.IsEntityValid(Entity))
	{
		return false;
	}

	FMassEntityView View(EntityManager, Entity);
	const FArcMassQuickBarSharedFragment* Shared = View.GetConstSharedFragmentDataPtr<FArcMassQuickBarSharedFragment>();
	if (!Shared || !StoreType)
	{
		return false;
	}

	bool bDeferred = false;
	for (const TObjectPtr<UArcMassQuickBarConfig>& Config : Shared->Configs)
	{
		if (!Config || Config->StoreType.Get() != StoreType)
		{
			continue;
		}
		ForEachBarConfig(*Config,
			[ItemSlot, &bDeferred](const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarGroup*) -> bool
			{
				if (!Bar.bDeferItemActivation)
				{
					return true;
				}
				bDeferred = Bar.Slots.ContainsByPredicate(
					[ItemSlot](const FArcMassQuickBarSlot& S) { return S.ItemSlotId == ItemSlot; });
				return !bDeferred;
			});
		if (bDeferred)
		{
			break;
		}
	}
	return bDeferred;
}

const FArcItemData* GetActiveItem(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const FGameplayTagContainer& RequiredItemTags)
{
	if (!EntityManager.IsEntityValid(Entity))
	{
		return nullptr;
	}

	FMassEntityView View(EntityManager, Entity);
	const FArcMassQuickBarSharedFragment* Shared = View.GetConstSharedFragmentDataPtr<FArcMassQuickBarSharedFragment>();
	const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
	if (!Shared || !State)
	{
		return nullptr;
	}

	for (const FArcMassQuickBarActiveSlot& ActiveSlot : State->ActiveSlots)
	{
		if (!ActiveSlot.bIsSlotActive || !ActiveSlot.AssignedItemId.IsValid())
		{
			continue;
		}

		const FArcMassQuickBarLocation Location = ArcMassQuickBarOperations_Private::FindBarInShared(*Shared, ActiveSlot.BarId);
		if (!Location.Config)
		{
			continue;
		}

		const FArcItemData* ItemData = Internal::GetItemFromStore(EntityManager, Entity, Location.Config->StoreType, ActiveSlot.AssignedItemId);
		if (ItemData && ItemData->IsActive() && Internal::ItemMatchesRequiredTags(ItemData, RequiredItemTags))
		{
			return ItemData;
		}
	}

	return nullptr;
}

} // namespace ArcMassItems::QuickBar
