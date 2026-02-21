// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "ArcTQSGenerator_SpatialHashBox.generated.h"

/**
 * Generates Mass Entities from the spatial hash subsystem using an axis-aligned box query
 * centered on each context location.
 *
 * Index filtering works identically to the Sphere generator — see its documentation.
 */
USTRUCT(DisplayName = "Spatial Hash Box")
struct ARCAI_API FArcTQSGenerator_SpatialHashBox : public FArcTQSGenerator
{
	GENERATED_BODY()

	// Half extents of the axis-aligned box
	UPROPERTY(EditAnywhere, Category = "Generator", meta = (ClampMin = 0.0))
	FVector HalfExtents = FVector(2500.0f, 2500.0f, 500.0f);

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
