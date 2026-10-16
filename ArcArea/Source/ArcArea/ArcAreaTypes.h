// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "SmartObjectTypes.h"
#include "Mass/EntityHandle.h"
#include "ArcAreaTypes.generated.h"

/** Current state of an area slot. */
UENUM(BlueprintType)
enum class EArcAreaSlotState : uint8
{
	/** No NPC assigned to this slot. */
	Vacant,
	/** An NPC is assigned but not actively at the SmartObject (away eating, sleeping, etc.). */
	Assigned,
	/** An NPC is assigned AND currently has a SmartObject claim (actively working). */
	Active,
	Disabled
};

/** Runtime per-slot data, owned by the subsystem. */
USTRUCT()
struct ARCAREA_API FArcAreaSlotRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	EArcAreaSlotState State = EArcAreaSlotState::Vacant;

	UPROPERTY()
	FMassEntityHandle AssignedEntity;

	UPROPERTY()
	FGameplayTagContainer ActivityTags;

	/** Object-level user query; empty when the definition's filtering policy does not apply one. */
	UPROPERTY()
	FGameplayTagQuery ObjectUserTagFilter;

	/** Slot-level user query; empty when the definition's filtering policy does not apply one. */
	UPROPERTY()
	FGameplayTagQuery SlotUserTagFilter;

	/** Mirrors USmartObjectSubsystem's per-query "empty means no constraint, otherwise must match". */
	bool MatchesUserTags(const FGameplayTagContainer& UserTags) const
	{
		return (ObjectUserTagFilter.IsEmpty() || ObjectUserTagFilter.Matches(UserTags))
			&& (SlotUserTagFilter.IsEmpty() || SlotUserTagFilter.Matches(UserTags));
	}
};

/** Runtime per-area data, owned by the subsystem. Single source of truth for area state. */
USTRUCT(BlueprintType)
struct ARCAREA_API FArcAreaData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Area")
	FPrimaryAssetId DefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Area")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Area")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Area")
	FGameplayTagContainer AreaTags;

	UPROPERTY()
	FSmartObjectHandle SmartObjectHandle;

	/** Mass entity that registered this area (the SO entity). */
	UPROPERTY()
	FMassEntityHandle OwnerEntity;

	UPROPERTY()
	TMap<FSmartObjectSlotHandle, FArcAreaSlotRuntime> Slots;
};
