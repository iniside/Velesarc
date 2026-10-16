// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
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

	UPROPERTY()
	TArray<FArcItemId> ChildItemIds;
};

USTRUCT()
struct FArcMassQuickBarBarState
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag BarId;

	UPROPERTY()
	bool bIsBarActive = false;

	UPROPERTY()
	FGameplayTag LastActiveSlotId;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarStateFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcMassQuickBarActiveSlot> ActiveSlots;

	UPROPERTY()
	TArray<FArcMassQuickBarBarState> BarStates;
};

template<>
struct TMassFragmentTraits<FArcMassQuickBarStateFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
