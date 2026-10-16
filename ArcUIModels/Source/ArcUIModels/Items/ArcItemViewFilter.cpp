// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcItemViewFilter.h"
#include "Items/ArcItemListEntry.h"

namespace ArcItemViewFilter_Impl
{
	static bool PassesSlotPolicy(const FArcItemListEntry& Row, EArcItemViewSlotPolicy Policy, const FGameplayTag& MatchSlot)
	{
		switch (Policy)
		{
		case EArcItemViewSlotPolicy::Any:
			return true;
		case EArcItemViewSlotPolicy::UnslottedOnly:
			return !Row.CurrentItemStoreSlot.IsValid();
		case EArcItemViewSlotPolicy::SlottedOnly:
			return Row.CurrentItemStoreSlot.IsValid();
		case EArcItemViewSlotPolicy::MatchSlot:
			return Row.CurrentItemStoreSlot == MatchSlot;
		}
		return true;
	}
} // namespace ArcItemViewFilter_Impl

bool FArcItemViewFilter::IsEmpty() const
{
	return RequiredTags.IsEmpty()
		&& IgnoreTags.IsEmpty()
		&& SlotPolicy == EArcItemViewSlotPolicy::Any
		&& MinLevel == 0
		&& MaxLevel == MAX_int32;
}

bool FArcItemViewFilter::Matches(const FArcItemListEntry& Row) const
{
	if (!RequiredTags.IsEmpty() && !Row.Tags.HasAll(RequiredTags))
	{
		return false;
	}

	if (!IgnoreTags.IsEmpty() && Row.Tags.HasAny(IgnoreTags))
	{
		return false;
	}

	if (!ArcItemViewFilter_Impl::PassesSlotPolicy(Row, SlotPolicy, MatchSlot))
	{
		return false;
	}

	const int32 RowLevel = Row.Level;
	if (RowLevel < MinLevel || RowLevel > MaxLevel)
	{
		return false;
	}

	return true;
}
