// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSGenerator.generated.h"

/**
 * Base struct for TQS generators. Generators produce the initial pool of target items.
 * Stored as FInstancedStruct in the query definition.
 *
 * Override GenerateItems() to populate OutItems with targets from spatial queries,
 * perception data, explicit lists, grid patterns, etc.
 */
USTRUCT(BlueprintType)
struct ARCAI_API FArcTQSGenerator
{
	GENERATED_BODY()

	virtual ~FArcTQSGenerator() = default;

	/**
	 * Generate initial target items. Called once at query start.
	 * @param QueryContext - provides querier entity, location, world, entity manager
	 * @param OutItems - array to populate with target items
	 */
	virtual void GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const {}
};
