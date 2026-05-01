// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityElementTypes.h"
#include "Items/ArcItemTypes.h"
#include "GameplayTagContainer.h"
#include "ArcMassItemEventFragment.generated.h"

UENUM()
enum class EArcMassItemEventType : uint8
{
	Added,
	Removed,
	SlotChanged,
	StacksChanged,
	Attached,
	Detached
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FArcItemId ItemId;

	UPROPERTY()
	EArcMassItemEventType Type = EArcMassItemEventType::Added;

	UPROPERTY()
	FGameplayTag SlotTag;

	UPROPERTY()
	int32 StackDelta = 0;

	// The store fragment type that emitted this event.
	// Listeners with multiple stores on one entity use this to filter to their store.
	UPROPERTY()
	UScriptStruct* StoreType = nullptr;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemEventFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcMassItemEvent> Events;
};

template<>
struct TMassFragmentTraits<FArcMassItemEventFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
