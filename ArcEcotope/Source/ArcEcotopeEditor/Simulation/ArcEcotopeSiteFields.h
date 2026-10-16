// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Math/IntRect.h"

struct FArcEcotopeMarginedHeightGrid;
struct FArcClimateFieldSettings;
struct FArcEcotopeSiteFieldSettings;
struct FArcEcotopeSiteMetadata;

/**
 * Site field generators. Every function reads plain arrays in metres and touches no world, so a
 * test can drive one from a hand-built height grid.
 */
namespace ArcEcotopeSiteFields
{
	/** Distribution of the moisture pipeline's intermediates over the site cells, for the bake report. */
	struct FMoistureDiagnostics
	{
		/** Conservative peak bytes owned by this generator, including hydrology solve transients. */
		uint64 PeakWorkingBytes = 0;

		double TwiMin = 0.0;
		double TwiMean = 0.0;
		double TwiMax = 0.0;
		double HandMinM = 0.0;
		double HandMeanM = 0.0;
		double HandMaxM = 0.0;
		double MoistureMin = 0.0;
		double MoistureMean = 0.0;
		double MoistureMax = 0.0;

		/** Cells the depression fill raised above their sampled height, over the whole margined grid. */
		int32 FilledCellCount = 0;

		/** Cells the fill left level because another epsilon step would pass the elevation its run started from. */
		int32 SaturatedFlatCellCount = 0;

		int32 ChannelCellCount = 0;

		/** Site cells whose downslope walk reached no channel, so their moisture carries the TWI term alone. */
		int32 HandUnresolvedCellCount = 0;
	};

	/**
	 * Fills OutBaseMoisture with one value per base-level site cell in row-major order. Accumulation is
	 * the contributing area in square metres at every MARGINED cell and ChannelMask is either empty or
	 * one entry per margined cell; an empty mask leaves every cell's moisture on its TWI term alone.
	 */
	ARCECOTOPEEDITOR_API bool GenerateMoisture(
		const FArcEcotopeMarginedHeightGrid& HeightGrid,
		const FArcClimateFieldSettings& Settings,
		TConstArrayView<double> Accumulation,
		TConstArrayView<uint8> ChannelMask,
		TArray<float>& OutBaseMoisture,
		TArray<float>& OutBaseHandM,          // site-resolution, ACTOR-LOCAL metres; negative where unresolved
		FMoistureDiagnostics& OutDiagnostics,
		FText& OutError);

	/** Distribution of the sun load field over the site cells, for the bake report. */
	struct FSunLoadDiagnostics
	{
		/** Extremes of Eq. 3 over its whole (slope, folded aspect) domain at the authored latitude, used as the normalizer. */
		double RadiationMinimum = 0.0;
		double RadiationMaximum = 0.0;

		double SunLoadMin = 0.0;
		double SunLoadMean = 0.0;
		double SunLoadMax = 0.0;

		/** Means over sloped cells whose unmirrored azimuth is 225 and 45 degrees east of north, +/- 22.5; an east-west mirror collapses their difference toward zero. */
		double SouthWestMean = 0.0;
		double NorthEastMean = 0.0;
		int32 SouthWestCellCount = 0;
		int32 NorthEastCellCount = 0;

		/** Cells whose slope exceeded the 60 degree validity band of Eq. 3 and evaluated at the clamp. */
		int32 SlopeClampedCellCount = 0;

		/** Share of sampled cells too flat to carry an aspect signal; a low-relief volume clusters near the flat value legitimately. */
		double FlatCellFraction = 0.0;

		/** Cells with a valid surface. Every per-cell statistic above is taken over these; the radiation bounds are analytic and do not depend on the grid. */
		int32 SampledCellCount = 0;
	};

	/**
	 * Generates a tightly row-major base rectangle without materialising the complete sun field.  The
	 * rectangle is clipped to the site; callers retain the slope/validity source only for the lifetime
	 * of its remaining consumers.
	 */
	ARCECOTOPEEDITOR_API bool GenerateSunLoadRect(
		const FArcEcotopeSiteMetadata& Metadata,
		TConstArrayView<float> BaseSlopeDegrees,
		TConstArrayView<float> BaseAspectNormalized,
		TConstArrayView<uint8> BaseValidSurface,
		const FArcClimateFieldSettings& Settings,
		const FIntRect& BaseRect,
		TArray<float>& OutSunLoad,
		FSunLoadDiagnostics& OutDiagnostics,
		FText& OutError);

	/** Why the horizon pass returned constants instead of marching. */
	enum class EHorizonSkipReason : uint8
	{
		None,
		ZeroRadius,
		ReliefBelowMinimum,
	};

	/** Distribution of the two horizon reductions over the level cells, for the bake report. */
	struct FHorizonDiagnostics
	{
		/** Peak bytes owned by this generator; caller-owned input views are excluded. */
		uint64 PeakWorkingBytes = 0;

		EHorizonSkipReason SkipReason = EHorizonSkipReason::None;

		/** Relief of the whole margined grid, the statistic the pass is gated on. */
		double ReliefM = 0.0;

		/** The relief the pass was actually gated against, whether authored or derived. */
		double EffectiveMinReliefM = 0.0;

		/** True when EffectiveMinReliefM came from the marching geometry rather than from MinReliefM. */
		bool bMinReliefAutoDerived = false;

		/** Dimensions of the level grid the observers sample and both stored fields are indexed by; not the margined lattice actually marched, which is wider by MarginCells on each side. */
		FIntPoint LevelDimensions = FIntPoint::ZeroValue;

		/** Dimensions of the margined lattice the horizon march runs over. */
		FIntPoint CoarseDimensions = FIntPoint::ZeroValue;

		int32 AzimuthCount = 0;

		double SkyViewFactorMin = 0.0;
		double SkyViewFactorMean = 0.0;
		double SkyViewFactorMax = 0.0;

		double WindExposureMin = 0.0;
		double WindExposureMean = 0.0;
		double WindExposureMax = 0.0;

		/** Mean over sampled cells of the azimuth-weighted horizon angle that drives WindExposure. */
		double MeanHorizonAngleDegrees = 0.0;

		/** Cells where no azimuth found ground above the observer. */
		double NoBlockerWithinRadiusFraction = 0.0;

		/** Cells landing on the endpoint where the default SkyViewFactorResponse evaluates to zero. */
		int32 SkyViewFactorAtZeroCellCount = 0;

		/** Cells landing on the endpoint where the default WindExposureResponse evaluates to zero. */
		int32 WindExposureAtOneCellCount = 0;

		/** Level cells with at least one valid base child. Every per-cell statistic above is taken over these. */
		int32 SampledCellCount = 0;
	};

	/**
	 * Fills both horizon reductions for the chunk producer from one per-azimuth horizon-angle pass over
	 * the margined grid. Blocker heights are mean-pooled onto Settings.HorizonLevel and the observer
	 * keeps its base-level height.
	 */
	ARCECOTOPEEDITOR_API bool GenerateHorizonFields(
		const FArcEcotopeMarginedHeightGrid& HeightGrid,
		const FArcEcotopeSiteMetadata& Metadata,
		TConstArrayView<float> BaseSlopeDegrees,
		TConstArrayView<uint8> BaseValidSurface,
		const FArcClimateFieldSettings& Settings,
		TArray<float>& OutLevelSkyViewFactor,
		TArray<float>& OutLevelWindExposure,
		FHorizonDiagnostics& OutDiagnostics,
		FText& OutError);

	/** Distribution of the soil depth pipeline's intermediates over the site cells, for the bake report. */
	struct FSoilDepthDiagnostics
	{
		/** Supplied depth before clamping. A distribution collapsed onto one value is a Deposits map reaching no cell. */
		double PreClampDepthMinM = 0.0;
		double PreClampDepthMeanM = 0.0;
		double PreClampDepthMaxM = 0.0;

		double CapacityMin = 0.0;
		double CapacityMean = 0.0;
		double CapacityMax = 0.0;

		int32 DepthAtMinClampCellCount = 0;

		/** Mean of the slope shed multiplier, the second factor of the capacity. */
		double SlopeTermMean = 0.0;

		/** Cells at the capacity endpoints, where a FertilityResponse whose optimum sits elsewhere evaluates to zero. */
		int32 CapacityAtZeroCellCount = 0;
		int32 CapacityAtOneCellCount = 0;

		/** Cells with a valid surface. Every statistic above is taken over these, at the base level. */
		int32 SampledCellCount = 0;
	};

	/**
	 * Producer for one base rectangle, emitted immediately by the tiled bake. BaseSuppliedDepthM carries
	 * the caller's per-cell soil depth in metres; it is clamped, normalized and shed by slope into the
	 * baked fertility field, which the dynamic nutrient pool multiplies against.
	 */
	ARCECOTOPEEDITOR_API bool GenerateSoilDepthCapacityRect(
		const FArcEcotopeSiteMetadata& Metadata,
		TConstArrayView<float> BaseSlopeDegrees,
		TConstArrayView<uint8> BaseValidSurface,
		TConstArrayView<float> BaseSuppliedDepthM,
		const FArcEcotopeSiteFieldSettings& Settings,
		const FIntRect& BaseRect,
		TArray<float>& OutSoilCapacity,
		FSoilDepthDiagnostics& OutDiagnostics,
		FText& OutError);

	/**
	 * Metadata-only reduction used by global field exceptions before their owned rectangles are sliced.
	 * Coarsens by the arithmetic mean of the base cells covering each level cell; this operator is part
	 * of the stored contract, so changing it changes every authored response curve.
	 */
	ARCECOTOPEEDITOR_API bool ReduceFieldToLevel(
		const FArcEcotopeSiteMetadata& Metadata,
		TConstArrayView<float> BaseField,
		uint8 Level,
		TArray<float>& OutLevelField,
		uint64* OutPeakWorkingBytes = nullptr);
}
