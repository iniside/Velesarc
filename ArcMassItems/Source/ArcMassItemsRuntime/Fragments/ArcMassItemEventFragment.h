// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityElementTypes.h"
#include "Items/ArcItemTypes.h"
#include "GameplayTagContainer.h"
#include "ArcMassItemEventFragment.generated.h"

UENUM()
enum class EArcMassItemEventType : uint8
{
	Added,
	Removed,
	SlotAssigned,       // unslotted → slot (new assignment); SlotTag = new slot (always valid)
	SlotCleared,        // slot → unslotted (explicit removal); OldSlotTag = prior slot (always valid)
	SlotChanged,        // slot A → slot B (true move); SlotTag = new, OldSlotTag = old (both valid)
	StacksChanged,
	Attached,           // item-relationship (existing)
	Detached,           // item-relationship (existing)
	AttachmentChanged   // drives visual mutation
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FArcItemId ItemId;

	UPROPERTY()
	EArcMassItemEventType Type = EArcMassItemEventType::Added;

	// New slot tag.
	// SlotAssigned : always valid (the slot the item entered).
	// SlotChanged  : always valid (the destination slot of the move).
	// SlotCleared  : invalid (item is now unslotted — use OldSlotTag).
	// Attached/Detached: the attachment slot.
	UPROPERTY()
	FGameplayTag SlotTag;

	// Prior slot tag.
	// SlotCleared  : always valid (the slot the item left).
	// SlotChanged  : always valid (the source slot of the move).
	// SlotAssigned : invalid (item was previously unslotted).
	UPROPERTY()
	FGameplayTag OldSlotTag;

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
