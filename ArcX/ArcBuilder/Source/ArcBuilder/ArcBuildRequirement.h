// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcBuildRequirement.generated.h"

class UArcBuilderComponent;
class UArcBuildingDefinition;

/**
 * Base placement requirement for buildings.
 * Subclass and override to add custom placement validation
 * (line of sight, foundation check, terrain check, etc.).
 */
USTRUCT()
struct ARCBUILDER_API FArcBuildRequirement
{
	GENERATED_BODY()

public:
	/** Check whether placement can start at all (before preview spawns). */
	virtual bool CanStartPlacement(const UArcBuildingDefinition* InBuildDef, UArcBuilderComponent* InBuilderComponent) const { return true; }

	/** Check whether the building can be placed at the given transform (during preview). */
	virtual bool CanPlace(const FTransform& InTransform, const UArcBuildingDefinition* InBuildDef, UArcBuilderComponent* InBuilderComponent) const { return true; }

	virtual ~FArcBuildRequirement() = default;
};
