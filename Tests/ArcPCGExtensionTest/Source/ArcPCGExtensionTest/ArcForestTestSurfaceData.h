// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Data/PCGVolumeData.h"

#include "ArcForestTestSurfaceData.generated.h"

/**
 * Minimal test-only samplable spatial data for the headless forest-sim PCG smoke test.
 *
 * WHY A SUBCLASS: FPCGForestSimElement pre-bakes suitability by calling SurfaceIn->ProjectPoint at a
 * query transform whose Z is the execution bounds' Max.Z. The base UPCGSpatialData::ProjectPoint
 * falls back to SamplePoint, and PCGHelpers::IsInsideBounds is Z-MAX-EXCLUSIVE, so a stock
 * UPCGVolumeData rejects every query sitting exactly on its top face -> suitability 0 everywhere ->
 * zero seeds -> an empty forest that would make the smoke test vacuous. A real landscape is not an
 * option (its cache is game-thread-only and not spawnable in a lean CQTest). This surface therefore
 * overrides SamplePoint to be planar and Z-agnostic: any XY inside the footprint is fully suitable
 * (Density 1) and projects onto a flat Z, exactly the "simple synthetic surface that samples safely"
 * the smoke test needs, with no landscape/RHI/cache dependency.
 */
UCLASS()
class UArcForestTestSurfaceData : public UPCGVolumeData
{
	GENERATED_BODY()

public:
	/** Flat surface elevation (world cm) reported for every projected sample. */
	UPROPERTY()
	float FlatSurfaceZ = 0.0f;

	/**
	 * OPTIONAL analytic surface shape: world XY (cm) -> surface Z (cm). Unbound (the default) keeps the
	 * historical flat behaviour and reports FlatSurfaceZ everywhere.
	 *
	 * WHY THIS EXISTS: the derived terrain-morphology habitat fields
	 * (ArcForestTerrainFields::Derive) are finite differences of the node's baked surface HEIGHT grid,
	 * so a flat surface produces slope 0 / exposure 0.5 / a constant elevation in every cell — three
	 * fields with no spatial structure at all, against which a slope- or elevation-keyed species
	 * response is provably a no-op and any placement assertion would be vacuous. A caller-supplied
	 * height function is what gives those fields shaped input, and it stays ANALYTIC so the expected
	 * gradient direction is derivable on paper rather than read back off a fixture.
	 *
	 * Deliberately NOT a UPROPERTY — TFunction is not a reflected type. It captures no UObject in any
	 * fixture here, so it adds no GC reference this class would have to trace.
	 */
	TFunction<float(float X, float Y)> HeightFunction;

	/**
	 * Optional footprint override for the XY containment test in SamplePoint, INDEPENDENT of the
	 * volume Bounds used by GetBounds(). Falls back to GetBounds() when left invalid (default), which
	 * preserves every existing caller unchanged.
	 *
	 * WHY THIS EXISTS: PCGScatterNearPointsTests.cpp needs to exercise the node's invalid-bounds
	 * fallback (Surface->GetBounds().IsValid == false), which requires this fixture to NEVER call
	 * Initialize() (so GetBounds() stays invalid). But SamplePoint's own footprint check also reads
	 * GetBounds() by default, so an un-Initialize'd fixture would reject every query and make the
	 * fallback path indistinguishable from "everything missed". This override decouples the two.
	 */
	UPROPERTY()
	FBox FootprintOverride = FBox(EForceInit::ForceInit);

	void SetFootprintOverride(const FBox& InFootprint) { FootprintOverride = InFootprint; }

	/**
	 * When true, SamplePoint reports the QUERY's own Z (the incoming transform's location Z) as the
	 * surface height, instead of FlatSurfaceZ / HeightFunction. Added so a test can observe which Z a
	 * caller actually passed in. Defaults to false, which preserves the original Z-agnostic behaviour
	 * every other caller (the forest-sim smoke tests) relies on.
	 */
	UPROPERTY()
	bool bHonorQueryZ = false;

	/**
	 * Planar, Z-agnostic sample: full suitability for any XY within the footprint, projected onto
	 * HeightFunction(X, Y) when one is supplied and onto FlatSurfaceZ otherwise. Ignores the query Z
	 * entirely so a top-face query still succeeds — this is CONDITIONAL on bHonorQueryZ == false (the
	 * default); when bHonorQueryZ is true, the query's own Z is reported instead (see its declaration).
	 */
	virtual bool SamplePoint(
		const FTransform& InTransform,
		const FBox& InBounds,
		FPCGPoint& OutPoint,
		UPCGMetadata* OutMetadata) const override;
};
