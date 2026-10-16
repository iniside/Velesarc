// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
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

	UPROPERTY(BlueprintReadOnly, Category = "Area")
	FSmartObjectSlotHandle SlotHandle;

	bool IsAssigned() const { return SlotHandle.IsValid(); }
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
