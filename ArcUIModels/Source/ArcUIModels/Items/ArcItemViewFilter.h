// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ArcItemViewFilter.generated.h"

struct FArcItemListEntry;

UENUM(BlueprintType)
enum class EArcItemViewSlotPolicy : uint8
{
	Any,
	UnslottedOnly, // CurrentItemStoreSlot.IsValid() == false  (not in any slot)
	SlottedOnly,   // CurrentItemStoreSlot.IsValid() == true   (== equipped)
	MatchSlot      // CurrentItemStoreSlot == MatchSlot
};

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcItemViewFilter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer IgnoreTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EArcItemViewSlotPolicy SlotPolicy = EArcItemViewSlotPolicy::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "SlotPolicy==EArcItemViewSlotPolicy::MatchSlot"))
	FGameplayTag MatchSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 MaxLevel = MAX_int32;

	// Required by UE_MVVM_SET_PROPERTY_VALUE (SetPropertyValue calls Value == NewValue).
	bool operator==(const FArcItemViewFilter& Other) const
	{
		return RequiredTags == Other.RequiredTags
			&& IgnoreTags == Other.IgnoreTags
			&& SlotPolicy == Other.SlotPolicy
			&& MatchSlot == Other.MatchSlot
			&& MinLevel == Other.MinLevel
			&& MaxLevel == Other.MaxLevel;
	}

	bool operator!=(const FArcItemViewFilter& Other) const
	{
		return !(*this == Other);
	}

	// Returns true when all fields are at their default values — callers can skip
	// the per-row Matches() walk entirely when this is true.
	bool IsEmpty() const;

	// Returns true iff Row passes every filter criterion.
	bool Matches(const FArcItemListEntry& Row) const;
};
