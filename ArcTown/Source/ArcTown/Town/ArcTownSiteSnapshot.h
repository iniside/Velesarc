// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcTownSiteSnapshot.generated.h"

/**
 * Terrain surface frozen under a town volume, expressed in the sampled terrain's local frame.
 * The per-sample arrays carry no editor-visible flags: FPropertyRowGenerator builds one row per
 * element and expands eagerly, so exposing them stalls the editor on selection.
 */
USTRUCT()
struct ARCTOWN_API FArcTownSiteSnapshot
{
	GENERATED_BODY()

	/** Upper bound on GridDims.X * GridDims.Y. Exceeding it is an error, never a silent downsample. */
	static constexpr int64 MaxSampleCount = 1000000;

	static constexpr int32 MinMarginCells = 1;

	/** FGuid of the AArcTerrain actor the samples came from. */
	UPROPERTY()
	FGuid TerrainGuid;

	UPROPERTY()
	FTransform TerrainToWorld;

	/** The MARGINED grid: the site rect grown by MarginCells on every side. */
	UPROPERTY()
	FIntPoint GridDims = FIntPoint::ZeroValue;

	UPROPERTY()
	double SampleSpacing = 200.0;

	UPROPERTY()
	int32 MarginCells = 0;

	UPROPERTY()
	float RiversChannelThreshold = 0.5f;

	UPROPERTY()
	FVector2D OriginLocal = FVector2D::ZeroVector;

	/** Row-major, indexed X + Y * GridDims.X. */
	UPROPERTY()
	TArray<float> Heights;

	/** Parallel to Heights, or empty when normals were not sampled. */
	UPROPERTY()
	TArray<FVector3f> Normals;

	UPROPERTY()
	TArray<uint8> ChannelMask;

	UPROPERTY()
	uint64 SnapshotHash = 0;

	FIntPoint GetSiteDims() const;

	FVector2D GetSiteOriginLocal() const;

	int32 SnapshotIndex(int32 SiteX, int32 SiteY) const;

	uint64 ComputeHash() const;

	/**
	 * True only for a snapshot no sample pass ever touched. Callers skip validation on an empty
	 * snapshot, so a partly written one - dimensions without samples, or a negative dimension -
	 * must report non-empty and reach IsValid() instead of passing as absent.
	 */
	bool IsEmpty() const;

	/** Returned as int64 because the product of two int32 dimensions overflows int32. */
	int64 GetSampleCount() const;

	/**
	 * Everything a consumer may assume about a non-empty snapshot: a terrain identity to resolve, a
	 * lattice, a TerrainToWorld that is NaN-free, normalized and not zero-scaled, and finite samples
	 * throughout. Scans every sample, so it is a validation gate rather than a per-query test.
	 */
	bool IsValid() const;
};

namespace ArcTownSiteSampling
{
	/** True when a world XY falls on the sampled lattice, which SampleGroundZ silently clamps to instead. */
	ARCTOWN_API bool IsInsideSampledSite(const FArcTownSiteSnapshot& Snapshot, const FVector2D& WorldXY);

	ARCTOWN_API bool ClampIntoSampledSite(const FArcTownSiteSnapshot& Snapshot, FVector2D& InOutWorldXY);

	/** Bilinear world Z of the sampled surface under a world XY, clamped to the site lattice. */
	ARCTOWN_API bool SampleGroundZ(const FArcTownSiteSnapshot& Snapshot, const FVector2D& WorldXY, double& OutWorldZ);
}
