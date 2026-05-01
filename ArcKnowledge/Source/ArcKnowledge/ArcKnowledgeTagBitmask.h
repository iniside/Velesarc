// Copyright ArcGame. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"

// 128-bit bitmask for fast tag rejection in R-tree queries.
// Each bit corresponds to a registered knowledge tag.
// This is a fast-reject filter only — full FGameplayTagQuery::Matches()
// is always run on candidates that pass the bitmask check.
struct FArcKnowledgeTagBitmask
{
	uint64 Low = 0;
	uint64 High = 0;

	void SetBit(uint8 Index)
	{
		if (Index < 64)
		{
			Low |= (1ULL << Index);
		}
		else
		{
			High |= (1ULL << (Index - 64));
		}
	}

	bool HasAllBits(const FArcKnowledgeTagBitmask& Required) const
	{
		return (Low & Required.Low) == Required.Low
			&& (High & Required.High) == Required.High;
	}

	bool HasAnyBit(const FArcKnowledgeTagBitmask& Other) const
	{
		return (Low & Other.Low) != 0 || (High & Other.High) != 0;
	}

	bool IsEmpty() const { return Low == 0 && High == 0; }
};

// Maps FGameplayTag → bit index. Assigns indices on first encounter.
// Subsystem-owned, shared by R-tree and query dispatch.
struct FArcKnowledgeTagBitmaskRegistry
{
	uint8 GetOrAssignIndex(FGameplayTag Tag)
	{
		uint8* Existing = TagToIndex.Find(Tag);
		if (Existing)
		{
			return *Existing;
		}
		uint8 NewIndex = NextIndex++;
		checkf(NewIndex < 128, TEXT("ArcKnowledge tag bitmask overflow — more than 128 unique knowledge tags registered"));
		TagToIndex.Add(Tag, NewIndex);
		return NewIndex;
	}

	FArcKnowledgeTagBitmask BuildBitmask(const FGameplayTagContainer& Tags) const
	{
		FArcKnowledgeTagBitmask Mask;
		for (const FGameplayTag& Tag : Tags)
		{
			const uint8* Index = TagToIndex.Find(Tag);
			if (Index)
			{
				Mask.SetBit(*Index);
			}
		}
		return Mask;
	}

	FArcKnowledgeTagBitmask BuildBitmaskWithRegistration(const FGameplayTagContainer& Tags)
	{
		FArcKnowledgeTagBitmask Mask;
		for (const FGameplayTag& Tag : Tags)
		{
			Mask.SetBit(GetOrAssignIndex(Tag));
		}
		return Mask;
	}

	int32 GetRegisteredTagCount() const { return NextIndex; }

private:
	TMap<FGameplayTag, uint8> TagToIndex;
	uint8 NextIndex = 0;
};
