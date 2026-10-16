// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcItemStoreViewModel.h"

#include "Items/ArcItemListEntry.h"

UArcItemStoreViewModel::UArcItemStoreViewModel()
{
	Items.SetRowType(FArcItemListEntry::StaticStruct());
}

void UArcItemStoreViewModel::ApplyFilter(const FArcItemViewFilter& NewFilter)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(Filter, NewFilter))
	{
		OnFilterChanged.Broadcast();
	}
}

void UArcItemStoreViewModel::SetFilter(const FArcItemViewFilter& InFilter)
{
	ApplyFilter(InFilter);
}

void UArcItemStoreViewModel::ClearFilter()
{
	ApplyFilter(FArcItemViewFilter());
}

void UArcItemStoreViewModel::SetFilterRequiredTags(const FGameplayTagContainer& InTags)
{
	FArcItemViewFilter NewFilter = Filter;
	NewFilter.RequiredTags = InTags;
	ApplyFilter(NewFilter);
}

void UArcItemStoreViewModel::SetFilterIgnoreTags(const FGameplayTagContainer& InTags)
{
	FArcItemViewFilter NewFilter = Filter;
	NewFilter.IgnoreTags = InTags;
	ApplyFilter(NewFilter);
}

void UArcItemStoreViewModel::SetFilterSlotPolicy(EArcItemViewSlotPolicy InPolicy, FGameplayTag InMatchSlot)
{
	FArcItemViewFilter NewFilter = Filter;
	NewFilter.SlotPolicy = InPolicy;
	NewFilter.MatchSlot = InMatchSlot;
	ApplyFilter(NewFilter);
}

void UArcItemStoreViewModel::SetFilterLevelRange(int32 InMin, int32 InMax)
{
	FArcItemViewFilter NewFilter = Filter;
	NewFilter.MinLevel = InMin;
	NewFilter.MaxLevel = InMax;
	ApplyFilter(NewFilter);
}

void UArcItemStoreViewModel::BroadcastItemsChanged()
{
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Items);
}

void UArcItemStoreViewModel::Reset()
{
	if (Items.Num() > 0)
	{
		Items.Reset();
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Items);
	}

	FArcItemViewFilter DefaultFilter;
	UE_MVVM_SET_PROPERTY_VALUE(Filter, DefaultFilter);
}
