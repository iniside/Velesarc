// Copyright Lukasz Baran. All Rights Reserved.

#include "Fragments/ArcOwnedTagsFragment.h"

FArcTagDelta FArcOwnedTagsFragment::AddTag(FGameplayTag Tag)
{
	int32 OldCount = Tags.GetCount(Tag);
	int32 NewCount = Tags.AddTag(Tag);
	return FArcTagDelta{OldCount, NewCount};
}

FArcTagDelta FArcOwnedTagsFragment::RemoveTag(FGameplayTag Tag)
{
	int32 OldCount = Tags.GetCount(Tag);
	int32 NewCount = Tags.RemoveTag(Tag);
	return FArcTagDelta{OldCount, NewCount};
}

void FArcOwnedTagsFragment::AddTags(const FGameplayTagContainer& InTags, TArray<FArcPendingTagEvent>& OutEvents, FMassEntityHandle Entity)
{
	for (const FGameplayTag& Tag : InTags)
	{
		FArcTagDelta Delta = AddTag(Tag);
		if (Delta.OldCount != Delta.NewCount)
		{
			OutEvents.Add(FArcPendingTagEvent{Entity, Tag, Delta.OldCount, Delta.NewCount});
		}
	}
}

void FArcOwnedTagsFragment::RemoveTags(const FGameplayTagContainer& InTags, TArray<FArcPendingTagEvent>& OutEvents, FMassEntityHandle Entity)
{
	for (const FGameplayTag& Tag : InTags)
	{
		FArcTagDelta Delta = RemoveTag(Tag);
		if (Delta.OldCount != Delta.NewCount)
		{
			OutEvents.Add(FArcPendingTagEvent{Entity, Tag, Delta.OldCount, Delta.NewCount});
		}
	}
}
