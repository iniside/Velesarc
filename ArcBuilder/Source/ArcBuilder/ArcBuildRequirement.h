// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

#include "ArcBuildRequirement.generated.h"

struct FMassEntityManager;
class UArcBuildingDefinition;
class UWorld;

/**
 * Context passed to placement-requirement evaluation.
 *
 * Both the client-side placement preview and the server-authoritative place
 * command build one of these; requirements must not assume either side (the
 * client result is UX-only, server re-validation is authoritative).
 */
struct FArcBuildRequirementContext
{
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	FMassEntityHandle Instigator;
};

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
	/** Check whether the building can be placed at the given transform (during preview). */
	virtual bool CanPlace(const FTransform& InTransform, const FArcBuildRequirementContext& InContext, const UArcBuildingDefinition* InBuildDef) const { return true; }

	virtual ~FArcBuildRequirement() = default;
};

/**
 * Placement requirement: a Mass entity carrying RequiredStationTag must exist
 * within Radius of the placement location.
 *
 * The query hits UArcMassSpatialHashSubsystem's tag-indexed grid. Station
 * entity configs must carry UArcMassSpatialHashTrait with matching SpatialIndex.IndexTags
 * for the entity to land in that grid (content prerequisite).
 *
 * Client and server query independent grids: the client result is UX-only
 * (ghost tint) and may briefly false-negative until the station replicates
 * into the client's grid. Correctness rests solely on the server re-validation
 * in the place command.
 */
USTRUCT(DisplayName = "Nearby Station")
struct ARCBUILDER_API FArcBuildRequirement_NearbyStation : public FArcBuildRequirement
{
	GENERATED_BODY()

public:
	/** Index tag the station entity must be registered under in the spatial hash. */
	UPROPERTY(EditAnywhere, Category = "Requirement")
	FGameplayTag RequiredStationTag;

	/** Search radius (world units) around the placement location. */
	UPROPERTY(EditAnywhere, Category = "Requirement")
	float Radius = 1500.f;

	virtual bool CanPlace(const FTransform& InTransform, const FArcBuildRequirementContext& InContext, const UArcBuildingDefinition* InBuildDef) const override;
};
