// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/Bags/ArcBagContainerViewModel.h"

#include "Bags/ArcBagDefinition.h"
#include "Commands/ArcMassAddBagCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "ViewModels/Bags/ArcBagEntries.h"

namespace ArcUIModelsBagContainerViewModel
{
	bool SetSlotSelected(FArcRmlStructList& List, int32 SlotIndex, bool bSelected)
	{
		const FString Key = FString::FromInt(SlotIndex);
		const FArcBagSlotEntry* Existing = List.FindAs<FArcBagSlotEntry>(Key);
		if (Existing == nullptr)
		{
			return false;
		}
		FArcBagSlotEntry Updated = *Existing;
		Updated.bSelected = bSelected;
		return List.Set(Key, Updated);
	}
}

UArcBagContainerViewModel::UArcBagContainerViewModel()
{
	Bags.SetRowType(FArcBagTabEntry::StaticStruct());
	Slots.SetRowType(FArcBagSlotEntry::StaticStruct());
}

void UArcBagContainerViewModel::SelectBag(int32 Index)
{
	if (Index < 0 || Index >= Bags.Num() || Index == ActiveBagIndex)
	{
		return;
	}

	SelectedSlotIndex = INDEX_NONE;
	UnhoverSlot();
	UE_MVVM_SET_PROPERTY_VALUE(ActiveBagIndex, Index);
	OnActiveBagChanged.Broadcast();
}

void UArcBagContainerViewModel::HoverSlot(int32 Index)
{
	HoveredSlotIndex = Index;
	RefreshHover();
}

void UArcBagContainerViewModel::UnhoverSlot()
{
	HoveredSlotIndex = INDEX_NONE;
	RefreshHover();
}

void UArcBagContainerViewModel::RefreshHover()
{
	const FArcBagSlotEntry* Row = HoveredSlotIndex == INDEX_NONE ? nullptr : Slots.FindAs<FArcBagSlotEntry>(FString::FromInt(HoveredSlotIndex));
	if (Row == nullptr)
	{
		HoveredSlotIndex = INDEX_NONE;
		UE_MVVM_SET_PROPERTY_VALUE(bHoverShown, false);
		UE_MVVM_SET_PROPERTY_VALUE(HoverName, FText());
		UE_MVVM_SET_PROPERTY_VALUE(HoverDescription, FText());
		UE_MVVM_SET_PROPERTY_VALUE(HoverIcon, TSoftObjectPtr<UObject>());
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(bHoverShown, Row->bHasItem);
	UE_MVVM_SET_PROPERTY_VALUE(HoverName, Row->ItemName);
	UE_MVVM_SET_PROPERTY_VALUE(HoverDescription, Row->ItemDescription);
	UE_MVVM_SET_PROPERTY_VALUE(HoverIcon, Row->Icon);
}

void UArcBagContainerViewModel::SelectSlot(int32 Index)
{
	if (Index < 0 || Index >= Slots.Num())
	{
		return;
	}

	const int32 NewSelection = Index == SelectedSlotIndex ? INDEX_NONE : Index;

	bool bChanged = false;
	if (SelectedSlotIndex != INDEX_NONE)
	{
		bChanged |= ArcUIModelsBagContainerViewModel::SetSlotSelected(Slots, SelectedSlotIndex, false);
	}
	if (NewSelection != INDEX_NONE)
	{
		bChanged |= ArcUIModelsBagContainerViewModel::SetSlotSelected(Slots, NewSelection, true);
	}
	SelectedSlotIndex = NewSelection;

	if (bChanged)
	{
		BroadcastSlotsChanged();
	}
}

bool UArcBagContainerViewModel::RequestAddBag(UObject* WorldContextObject, UArcBagDefinition* Def)
{
	if (WorldContextObject == nullptr || !TargetEntity.IsSet() || Def == nullptr)
	{
		return false;
	}

	return Arcx::SendServerCommand<FArcMassAddBagCommand>(
		WorldContextObject,
		TargetEntity,
		Def);
}

void UArcBagContainerViewModel::SetEntity(FMassEntityHandle InEntity)
{
	TargetEntity = InEntity;
}

void UArcBagContainerViewModel::BroadcastBagsChanged()
{
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Bags);
}

void UArcBagContainerViewModel::BroadcastSlotsChanged()
{
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	RefreshHover();
}

void UArcBagContainerViewModel::SetActiveBagIndex(int32 InIndex)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveBagIndex, InIndex);
}

void UArcBagContainerViewModel::SetCapacity(const FString& InCapacity)
{
	UE_MVVM_SET_PROPERTY_VALUE(Capacity, InCapacity);
}

void UArcBagContainerViewModel::ClearSelectedSlot()
{
	SelectedSlotIndex = INDEX_NONE;
}
