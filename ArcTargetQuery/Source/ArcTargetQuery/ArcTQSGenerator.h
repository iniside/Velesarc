// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "StructUtils/PropertyBag.h"
#include "ArcTQSGenerator.generated.h"

/**
 * Base struct for TQS generators. Generators produce the initial pool of target items.
 * Stored as FInstancedStruct in the query definition.
 */
USTRUCT(BlueprintType)
struct ARCTARGETQUERY_API FArcTQSGenerator
{
	GENERATED_BODY()

	virtual ~FArcTQSGenerator() = default;

	virtual void GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const {}

	/** Declare the property bag schema for items this generator produces.
	  * Used at StateTree compile time to set up output bindings.
	  * Override in generators that store ItemData. */
	virtual void GetOutputSchema(TArray<FPropertyBagPropertyDesc>& OutDescs) const {}

	virtual EArcTQSWorldRequirements GetWorldRequirements() const { return EArcTQSWorldRequirements::None; }

	UPROPERTY()
	FGuid ID;
};
