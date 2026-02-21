// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "ArcTQSGenerator_SpatialHashSphere.generated.h"

/**
 * Generates Mass Entities from the spatial hash subsystem using a sphere query
 * around each context location. Supports indexed grid queries via gameplay tags,
 * Mass fragments, and Mass tags.
 *
 * Index filtering:
 *   - GameplayTags, MassFragments, and MassTags each produce separate hash keys by default.
 *   - CombinationIndices allow combining multiple tags/fragments/mass tags into a single
 *     hash key, matching how entities are inserted via FArcMassSpatialHashCombinationIndex.
 *   - When bCombineAllIndices is true, all individual tags/fragments/mass tags are combined
 *     into a single hash key (CombinationIndices are ignored in this mode).
 *   - When no index keys are specified, queries the main (unfiltered) grid.
 */
USTRUCT(DisplayName = "Spatial Hash Sphere")
struct ARCAI_API FArcTQSGenerator_SpatialHashSphere : public FArcTQSGenerator
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Generator", meta = (ClampMin = 0.0))
	float Radius = 5000.0f;

	// Gameplay tags to use as individual index keys for filtered queries
	UPROPERTY(EditAnywhere, Category = "Generator|Index")
	FGameplayTagContainer IndexTags;

	// Mass fragment types to use as individual index keys
	UPROPERTY(EditAnywhere, Category = "Generator|Index", meta = (BaseStruct = "/Script/MassEntity.MassFragment"))
	TArray<FInstancedStruct> IndexFragments;

	// Mass tag types to use as individual index keys
	UPROPERTY(EditAnywhere, Category = "Generator|Index", meta = (BaseStruct = "/Script/MassEntity.MassTag"))
	TArray<FInstancedStruct> IndexMassTags;

	// Combination indices — each entry combines its tags/fragments/mass tags into a single hash key.
	// Mirrors how entities are inserted via FArcMassSpatialHashCombinationIndex on the trait.
	UPROPERTY(EditAnywhere, Category = "Generator|Index")
	TArray<FArcMassSpatialHashCombinationIndex> CombinationIndices;

	// If true, combine all individual tags/fragments/mass tags into a single hash key.
	// CombinationIndices are ignored in this mode.
	UPROPERTY(EditAnywhere, Category = "Generator|Index")
	bool bCombineAllIndices = false;

	virtual void GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const override;
};
