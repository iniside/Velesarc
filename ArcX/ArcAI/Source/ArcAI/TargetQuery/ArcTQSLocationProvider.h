// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSLocationProvider.generated.h"

/**
 * Base struct that resolves a reference location from the query context at runtime.
 *
 * Used by scoring/filter steps that need a configurable reference point (e.g. direction tests).
 * In State Tree inline definitions, you can property-bind a FVector directly instead of using a provider.
 * In DataAsset definitions, use a provider to resolve the location dynamically.
 *
 * The provider can return either a single location or a per-item location (e.g. nearest context).
 */
USTRUCT(BlueprintType)
struct ARCAI_API FArcTQSLocationProvider
{
	GENERATED_BODY()

	virtual ~FArcTQSLocationProvider() = default;

	/**
	 * Resolve a reference location. Default implementation returns querier location.
	 *
	 * @param Item - the item being evaluated (for per-item providers like NearestContext)
	 * @param QueryContext - full query context
	 * @return the resolved reference location
	 */
	virtual FVector GetLocation(
		const FArcTQSTargetItem& Item,
		const FArcTQSQueryContext& QueryContext) const
	{
		return QueryContext.QuerierLocation;
	}
};

/**
 * Returns the querier's own location. Default behavior.
 */
USTRUCT(DisplayName = "Querier Location")
struct ARCAI_API FArcTQSLocationProvider_Querier : public FArcTQSLocationProvider
{
	GENERATED_BODY()

	virtual FVector GetLocation(
		const FArcTQSTargetItem& Item,
		const FArcTQSQueryContext& QueryContext) const override
	{
		return QueryContext.QuerierLocation;
	}
};

/**
 * Returns the context location nearest to the item being evaluated.
 * Useful when generators ran around multiple context points and you want
 * each item scored relative to its "owning" context.
 */
USTRUCT(DisplayName = "Nearest Context Location")
struct ARCAI_API FArcTQSLocationProvider_NearestContext : public FArcTQSLocationProvider
{
	GENERATED_BODY()

	virtual FVector GetLocation(
		const FArcTQSTargetItem& Item,
		const FArcTQSQueryContext& QueryContext) const override;
};

/**
 * Returns a specific context location by index.
 * Falls back to querier location if index is out of range.
 */
USTRUCT(DisplayName = "Context Location by Index")
struct ARCAI_API FArcTQSLocationProvider_ContextByIndex : public FArcTQSLocationProvider
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Provider", meta = (ClampMin = 0))
	int32 ContextIndex = 0;

	virtual FVector GetLocation(
		const FArcTQSTargetItem& Item,
		const FArcTQSQueryContext& QueryContext) const override;
};
