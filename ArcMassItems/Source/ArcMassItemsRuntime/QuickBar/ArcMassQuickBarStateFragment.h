// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Items/ArcItemId.h"
#include "GameplayTagContainer.h"
#include "ArcMassQuickBarStateFragment.generated.h"

USTRUCT()
struct FArcMassQuickBarActiveSlot
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag BarId;

	UPROPERTY()
	FGameplayTag QuickSlotId;

	UPROPERTY()
	FArcItemId AssignedItemId;

	UPROPERTY()
	FGameplayTag ItemSlot;

	UPROPERTY()
	bool bIsSlotActive = false;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarStateFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcMassQuickBarActiveSlot> ActiveSlots;
};

template<>
struct TMassFragmentTraits<FArcMassQuickBarStateFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
