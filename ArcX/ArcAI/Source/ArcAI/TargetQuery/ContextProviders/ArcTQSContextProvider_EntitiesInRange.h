// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"
#include "TargetQuery/ArcTQSContextProvider.h"
#include "ArcTQSContextProvider_EntitiesInRange.generated.h"

/**
 * Context provider that finds mass entities within range using the spatial hash.
 * Each entity's location becomes a context point that generators run around.
 *
 * Supports indexed grid queries via gameplay tags, Mass fragments, Mass tags,
 * and combination indices — matching the spatial hash insertion pattern.
 *
 * Example: find all resource nodes (indexed by tag) near the querier, then
 * generate a grid around each one and score positions.
 */
USTRUCT(DisplayName = "Entities In Range")
struct ARCAI_API FArcTQSContextProvider_EntitiesInRange : public FArcTQSContextProvider
{
	GENERATED_BODY()

	// Search radius around querier location
	UPROPERTY(EditAnywhere, Category = "Context", meta = (ClampMin = 0.0))
	float Radius = 5000.0f;

	// --- Index filtering (optional) ---
	// When all index fields are empty, the main (unfiltered) grid is queried.

	// Gameplay tags to query indexed grids by. Each tag is queried separately.
	UPROPERTY(EditAnywhere, Category = "Context|Index")
	FGameplayTagContainer IndexTags;

	// Mass fragment types to query indexed grids by (e.g. FArcSmartObjectOwnerFragment).
	UPROPERTY(EditAnywhere, Category = "Context|Index", meta = (BaseStruct = "/Script/MassEntity.MassFragment"))
	TArray<FInstancedStruct> IndexFragments;

	// Mass tag types to query indexed grids by (e.g. FArcMassSightPerceivableTag).
	UPROPERTY(EditAnywhere, Category = "Context|Index", meta = (BaseStruct = "/Script/MassEntity.MassTag"))
	TArray<FInstancedStruct> IndexMassTags;

	// Combination indices — each entry produces a single combined hash key.
	UPROPERTY(EditAnywhere, Category = "Context|Index")
	TArray<FArcMassSpatialHashCombinationIndex> CombinationIndices;

	// When true, all individual tags/fragments/mass tags are combined into a single key
	// (CombinationIndices are ignored). When false, each produces a separate query.
	UPROPERTY(EditAnywhere, Category = "Context|Index")
	bool bCombineAllIndices = false;

	virtual void GenerateContextLocations(
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const override;
};
