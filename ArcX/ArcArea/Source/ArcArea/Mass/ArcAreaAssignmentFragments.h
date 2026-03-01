// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "ArcAreaTypes.h"

#include "ArcAreaAssignmentFragments.generated.h"

/**
 * Per-entity mutable fragment: current area assignment for an NPC.
 * This is a lightweight cache — the subsystem is the single source of truth.
 * Updated by UArcAreaSubsystem when assignments change.
 */
USTRUCT(BlueprintType)
struct ARCAREA_API FArcAreaAssignmentFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	FArcAreaHandle AreaHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	FGameplayTag RoleTag;

	bool IsAssigned() const { return AreaHandle.IsValid() && SlotIndex != INDEX_NONE; }
};

/**
 * Const shared fragment: what roles this NPC archetype can fill.
 * Shared across all entities of the same NPC archetype.
 */
USTRUCT(BlueprintType)
struct ARCAREA_API FArcAreaAssignmentConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Role tags this NPC type is eligible for (e.g., "Role.Blacksmith", "Role.Guard"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	FGameplayTagContainer EligibleRoles;
};

/**
 * Marker tag for NPC entities that can be assigned to areas.
 * Used by the cleanup observer.
 */
USTRUCT()
struct ARCAREA_API FArcAreaAssignmentTag : public FMassTag
{
	GENERATED_BODY()
};
