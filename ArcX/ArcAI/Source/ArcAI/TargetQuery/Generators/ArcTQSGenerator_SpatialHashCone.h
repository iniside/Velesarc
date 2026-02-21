// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "ArcTQSGenerator_SpatialHashCone.generated.h"

/**
 * Generates Mass Entities from the spatial hash subsystem using a cone query
 * originating from each context location in the querier's forward direction.
 *
 * Index filtering works identically to the Sphere generator — see its documentation.
 */
USTRUCT(DisplayName = "Spatial Hash Cone")
struct ARCAI_API FArcTQSGenerator_SpatialHashCone : public FArcTQSGenerator
{
	GENERATED_BODY()

	// Length of the cone from apex to base
	UPROPERTY(EditAnywhere, Category = "Generator", meta = (ClampMin = 0.0))
	float Length = 5000.0f;

	// Half angle of the cone in degrees
	UPROPERTY(EditAnywhere, Category = "Generator", meta = (ClampMin = 0.0, ClampMax = 180.0))
	float HalfAngleDegrees = 45.0f;

	// Gameplay tags to use as individual index keys for filtered queries
	UPROPERTY(EditAnywhere, Category = "Generator|Index")
	FGameplayTagContainer IndexTags;

	// Mass fragment types to use as individual index keys
	UPROPERTY(EditAnywhere, Category = "Generator|Index", meta = (BaseStruct = "/Script/MassEntity.MassFragment"))
	TArray<FInstancedStruct> IndexFragments;

	// Mass tag types to use as individual index keys
	UPROPERTY(EditAnywhere, Category = "Generator|Index", meta = (BaseStruct = "/Script/MassEntity.MassTag"))
	TArray<FInstancedStruct> IndexMassTags;

	// Combination indices — each entry combines its tags/fragments/mass tags into a single hash key
	UPROPERTY(EditAnywhere, Category = "Generator|Index")
	TArray<FArcMassSpatialHashCombinationIndex> CombinationIndices;

	// If true, combine all individual tags/fragments/mass tags into a single hash key
	UPROPERTY(EditAnywhere, Category = "Generator|Index")
	bool bCombineAllIndices = false;

	virtual void GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const override;
};
