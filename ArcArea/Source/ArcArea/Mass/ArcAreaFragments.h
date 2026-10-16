// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcAreaDefinition.h"

#include "ArcAreaFragments.generated.h"

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
