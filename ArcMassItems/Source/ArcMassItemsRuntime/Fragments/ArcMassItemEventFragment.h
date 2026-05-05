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
	Attached,           // item-relationship (existing)
	Detached,           // item-relationship (existing)
	VisualAttached,     // drives Phase 2 visual spawn
	VisualDetached,     // drives Phase 2 visual despawn
	AttachmentChanged   // drives Phase 3 visual mutation
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
	TObjectPtr<UScriptStruct> StoreType = nullptr;

	// GFrameCounter at commit time. Used by transaction commit to prune
	// entries left over from prior frames.
	UPROPERTY() uint64 FrameNumber = 0;
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
