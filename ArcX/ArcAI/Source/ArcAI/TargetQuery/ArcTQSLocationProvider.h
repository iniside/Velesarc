// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSLocationProvider.generated.h"

/**
 * Determines where additional reference locations come from beyond the querier.
 */
UENUM(BlueprintType)
enum class EArcTQSLocationSource : uint8
{
	/** No additional locations. Only querier location (if enabled). */
	None,
	/** Direct FVector — bindable from State Tree. */
	ReferenceLocation,
	/** All locations from QueryContext.ContextLocations. */
	ContextLocations,
	/** Custom FArcTQSLocationProvider via FInstancedStruct. */
	CustomProvider
};

/**
 * Shared configuration for resolving reference locations in spatial TQS steps.
 *
 * By default includes the querier's location. Additional locations can come from
 * a direct FVector, context locations, or a custom provider.
 * Steps call ResolveLocations() once and iterate the results.
 */
USTRUCT(BlueprintType)
struct ARCAI_API FArcTQSLocationConfig
{
	GENERATED_BODY()

	/** Include the querier's location in the resolved set. True by default. */
	UPROPERTY(EditAnywhere, Category = "Location")
	bool bIncludeQuerierLocation = true;

	/** Source for additional reference locations beyond the querier. */
	UPROPERTY(EditAnywhere, Category = "Location")
	EArcTQSLocationSource AdditionalSource = EArcTQSLocationSource::None;

	/** Direct reference location for State Tree property binding. */
	UPROPERTY(EditAnywhere, Category = "Location", meta = (EditCondition = "AdditionalSource == EArcTQSLocationSource::ReferenceLocation", EditConditionHides))
	FVector ReferenceLocation = FVector::ZeroVector;

	/** Custom provider for dynamic location resolution. */
	UPROPERTY(EditAnywhere, Category = "Location", meta = (BaseStruct = "/Script/ArcAI.ArcTQSLocationProvider", ExcludeBaseStruct, EditCondition = "AdditionalSource == EArcTQSLocationSource::CustomProvider", EditConditionHides))
	FInstancedStruct LocationProvider;

	/**
	 * Resolve all reference locations for this configuration.
	 */
	void ResolveLocations(
		const FArcTQSTargetItem& Item,
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const;
};

/**
 * Base struct that resolves reference locations from the query context at runtime.
 *
 * Used by scoring/filter steps that need a configurable reference point (e.g. direction tests).
 * In State Tree inline definitions, you can property-bind a FVector directly instead of using a provider.
 * In DataAsset definitions, use a provider to resolve the location dynamically.
 *
 * Providers can return a single location via GetLocation(), or multiple locations via GetLocations().
 * Steps iterate over all returned locations and pick the best result (highest score for scorers,
 * any-pass for filters). By default GetLocations() wraps GetLocation() in a single-element array.
 */
USTRUCT(BlueprintType)
struct ARCAI_API FArcTQSLocationProvider
{
	GENERATED_BODY()

	virtual ~FArcTQSLocationProvider() = default;

	/**
	 * Resolve a single reference location. Default implementation returns querier location.
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

	/**
	 * Resolve all reference locations. Steps iterate over these and pick the best result.
	 * Default implementation wraps GetLocation() in a single-element array.
	 *
	 * Override this for providers that naturally produce multiple locations (e.g. all context locations).
	 */
	virtual void GetLocations(
		const FArcTQSTargetItem& Item,
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const
	{
		OutLocations.Reset();
		OutLocations.Add(GetLocation(Item, QueryContext));
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

/**
 * Returns ALL context locations. Steps will evaluate the item against each
 * context location and pick the best result (highest score for scorers, any-pass for filters).
 */
USTRUCT(DisplayName = "All Context Locations")
struct ARCAI_API FArcTQSLocationProvider_AllContexts : public FArcTQSLocationProvider
{
	GENERATED_BODY()

	virtual FVector GetLocation(
		const FArcTQSTargetItem& Item,
		const FArcTQSQueryContext& QueryContext) const override
	{
		// Single-location fallback: return nearest context
		if (QueryContext.ContextLocations.IsEmpty())
		{
			return QueryContext.QuerierLocation;
		}
		if (QueryContext.ContextLocations.IsValidIndex(Item.ContextIndex))
		{
			return QueryContext.ContextLocations[Item.ContextIndex];
		}
		return QueryContext.ContextLocations[0];
	}

	virtual void GetLocations(
		const FArcTQSTargetItem& Item,
		const FArcTQSQueryContext& QueryContext,
		TArray<FVector>& OutLocations) const override
	{
		OutLocations.Reset();
		if (QueryContext.ContextLocations.IsEmpty())
		{
			OutLocations.Add(QueryContext.QuerierLocation);
		}
		else
		{
			OutLocations = QueryContext.ContextLocations;
		}
	}
};
