// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSGenerator.generated.h"

/**
 * Base struct for TQS generators. Generators produce the initial pool of target items.
 * Stored as FInstancedStruct in the query definition.
 *
 * The query context contains an array of ContextLocations. Generators run once per
 * context location, accumulating items from all of them. Override GenerateItemsAroundLocation()
 * to produce items around a single center point.
 *
 * For generators that don't operate around a center point (e.g. explicit lists),
 * override GenerateItems() directly instead.
 */
USTRUCT(BlueprintType)
struct ARCAI_API FArcTQSGenerator
{
	GENERATED_BODY()

	virtual ~FArcTQSGenerator() = default;

	/**
	 * Generate items for all context locations. Default implementation calls
	 * GenerateItemsAroundLocation() once per context location.
	 *
	 * Override this directly for generators that don't use context locations
	 * (e.g. explicit lists, perception-based).
	 */
	virtual void GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const;

	/**
	 * Generate items around a single center location.
	 * Called once per context location by the default GenerateItems() implementation.
	 *
	 * @param CenterLocation - the context location to generate around
	 * @param QueryContext - full query context (querier info, world, entity manager)
	 * @param OutItems - array to append generated items to
	 */
	virtual void GenerateItemsAroundLocation(
		const FVector& CenterLocation,
		const FArcTQSQueryContext& QueryContext,
		TArray<FArcTQSTargetItem>& OutItems) const {}
};
