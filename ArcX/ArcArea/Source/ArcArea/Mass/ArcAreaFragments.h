// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "ArcAreaTypes.h"
#include "ArcAreaDefinition.h"

#include "ArcAreaFragments.generated.h"

/**
 * Per-entity mutable fragment: stores the runtime area handle after registration.
 * Lives on SmartObject Mass entities that have the UArcAreaTrait.
 */
USTRUCT()
struct ARCAREA_API FArcAreaFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Set by the observer after the area is registered in the subsystem. */
	UPROPERTY()
	FArcAreaHandle AreaHandle;
};

/**
 * Const shared fragment: references the area definition data asset.
 * Shared across all entities of the same SmartObject archetype.
 */
USTRUCT()
struct ARCAREA_API FArcAreaConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Area")
	TWeakObjectPtr<UArcAreaDefinition> AreaDefinition;
};

/**
 * Marker tag for SmartObject entities that are area-managed.
 * Used by observer processors for add/remove registration.
 */
USTRUCT()
struct ARCAREA_API FArcAreaTag : public FMassTag
{
	GENERATED_BODY()
};
