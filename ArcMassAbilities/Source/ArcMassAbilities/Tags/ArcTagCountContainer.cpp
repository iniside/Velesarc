// Copyright Lukasz Baran. All Rights Reserved.

#include "Tags/ArcTagCountContainer.h"

int32 FArcTagCountContainer::AddTag(FGameplayTag Tag)
{
	int32& Count = TagCounts.FindOrAdd(Tag, 0);
	if (Count == 0)
	{
		Tags.AddTag(Tag);
	}
	++Count;
	return Count;
}

int32 FArcTagCountContainer::RemoveTag(FGameplayTag Tag)
{
	int32* CountPtr = TagCounts.Find(Tag);
	if (!CountPtr || *CountPtr <= 0)
	{
		return 0;
	}

	--(*CountPtr);
	int32 NewCount = *CountPtr;
	if (NewCount == 0)
	{
		Tags.RemoveTag(Tag);
		TagCounts.Remove(Tag);
	}
	return NewCount;
}

void FArcTagCountContainer::AddTags(const FGameplayTagContainer& InTags)
{
	for (const FGameplayTag& Tag : InTags)
	{
		AddTag(Tag);
	}
}

void FArcTagCountContainer::RemoveTags(const FGameplayTagContainer& InTags)
{
	for (const FGameplayTag& Tag : InTags)
	{
		RemoveTag(Tag);
	}
}

int32 FArcTagCountContainer::GetCount(FGameplayTag Tag) const
{
	const int32* CountPtr = TagCounts.Find(Tag);
	return CountPtr ? *CountPtr : 0;
}

bool FArcTagCountContainer::HasTag(FGameplayTag Tag) const
{
	return Tags.HasTag(Tag);
}

bool FArcTagCountContainer::HasTagExact(FGameplayTag Tag) const
{
	return Tags.HasTagExact(Tag);
}

bool FArcTagCountContainer::HasAll(const FGameplayTagContainer& Other) const
{
	return Tags.HasAll(Other);
}

bool FArcTagCountContainer::HasAllExact(const FGameplayTagContainer& Other) const
{
	return Tags.HasAllExact(Other);
}

bool FArcTagCountContainer::HasAny(const FGameplayTagContainer& Other) const
{
	return Tags.HasAny(Other);
}

bool FArcTagCountContainer::HasAnyExact(const FGameplayTagContainer& Other) const
{
	return Tags.HasAnyExact(Other);
}

FGameplayTagContainer FArcTagCountContainer::Filter(const FGameplayTagContainer& Other) const
{
	return Tags.Filter(Other);
}

FGameplayTagContainer FArcTagCountContainer::FilterExact(const FGameplayTagContainer& Other) const
{
	return Tags.FilterExact(Other);
}

bool FArcTagCountContainer::IsEmpty() const
{
	return Tags.IsEmpty();
}

int32 FArcTagCountContainer::Num() const
{
	return Tags.Num();
}

const FGameplayTagContainer& FArcTagCountContainer::GetTagContainer() const
{
	return Tags;
}
