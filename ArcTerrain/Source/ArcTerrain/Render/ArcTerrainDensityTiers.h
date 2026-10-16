// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcTerrainDensityTiers.generated.h"

class AArcTerrain;
struct FArcTerrainTileAtlasLayout;

namespace ArcTerrain
{
	inline constexpr int32 MaxTiers = 4;
}

/** ABSOLUTE texel density, not a step relative to the entry above it: LOD2 is 8x the base texel
 *  wherever it sits in HeightTiers. The enumerator IS the cumulative shift off the base raster, so at the
 *  default 100 cm texel LOD1..LOD4 read 400 / 800 / 1600 / 3200 cm per texel. */
UENUM()
enum class EArcTerrainHeightLOD : uint8
{
	LOD1 = 2,
	LOD2 = 3,
	LOD3 = 4,
	LOD4 = 5
};

USTRUCT()
struct ARCTERRAIN_API FArcTerrainHeightTier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	EArcTerrainHeightLOD LOD = EArcTerrainHeightLOD::LOD2;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", Units = "km", DisplayName = "Extent"))
	float ExtentKm = 0.f;

	UPROPERTY(VisibleAnywhere, Transient, meta = (DisplayName = "Resolved"))
	FString RowSummary;
};

struct FResolvedTier
{
	int32 CumulativeShift = 0;

	double PreviewRadiusCm = 0.0;
	double BandOuterRadiusCm = 0.0;
	double BandUnloadRadiusCm = 0.0;
	// The Band pair bounds the SLICE in the atlas, the Stream pair the tile PAYLOAD in RAM: the payload
	// outlives the slice so re-admission is a re-upload instead of a bake.
	double StreamRadiusCm = 0.0;
	double StreamUnloadRadiusCm = 0.0;

	int32 SectorQuadsShift = 0;
	int32 CellSectorShift = 0;

	int32 MaxResidentSectors = 0;
};

struct FArcTerrainTierLadder
{
	TArray<FResolvedTier, TInlineAllocator<ArcTerrain::MaxTiers>> Tiers;
};

/** The enumerator IS the cumulative shift off the WEIGHT base raster (WeightTexelSizeWorld), same scale as
 *  EArcTerrainHeightLOD's first three rungs. Capped at LOD3 because ArcTerrain::MaxTiers = 4 and the base
 *  takes one. */
UENUM()
enum class EArcTerrainWeightLOD : uint8
{
	LOD1 = 2,
	LOD2 = 3,
	LOD3 = 4
};

USTRUCT()
struct ARCTERRAIN_API FArcTerrainWeightTier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	EArcTerrainWeightLOD LOD = EArcTerrainWeightLOD::LOD1;

	/** The band's WIDTH, added outward from the previous rung's outer radius; 0 ends the ladder here. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", Units = "km", DisplayName = "Extent"))
	float ExtentKm = 0.f;

	UPROPERTY(VisibleAnywhere, Transient, meta = (DisplayName = "Resolved"))
	FString RowSummary;
};

struct FResolvedWeightTier
{
	int32 CumulativeShift = 0;

	double PreviewRadiusCm = 0.0;
	double BandOuterRadiusCm = 0.0;
	double BandUnloadRadiusCm = 0.0;
	double StreamRadiusCm = 0.0;
	double StreamUnloadRadiusCm = 0.0;

	int32 SliceShift = 0;
	int32 WeightQuadsShift = 0;
	int32 CellWeightQuads = 0;

	int32 MaxResidentWeightGroups = 0;
};

struct FArcTerrainWeightLadder
{
	TArray<FResolvedWeightTier, TInlineAllocator<ArcTerrain::MaxTiers>> Tiers;
};

namespace ArcTerrain
{
	/** DERIVED, never authored: a face of 2^k texels bottoms out at LEB depth 2k, where a leaf edge is one
	 *  texel and there is nothing finer in the atlas to resolve. ArcTerrain::MaxFaceQuadsShift keeps
	 *  k <= 24, which is what holds 2k under the depth-49 exactness limit of the float32 corner decode —
	 *  an authored value above that silently breaks the corner decode, so the authored quantity is
	 *  AArcTerrain::FaceQuadsShift and this ceiling follows it. */
	ARCTERRAIN_API int32 ResolveMaxSubdivisionDepth(int32 FaceQuads);

	ARCTERRAIN_API int32 SumAuthoredResidentSectors(const AArcTerrain& Terrain);

	ARCTERRAIN_API void ResolveAuthoredHeightTierShifts(const AArcTerrain& Terrain,
		TArray<int32>& OutCumulativeShifts);

	ARCTERRAIN_API void ResolveAuthoredWeightTierShifts(const AArcTerrain& Terrain,
		int32 ResolvedWeightQuads, TArray<int32>& OutCumulativeShifts, bool bEmitWarnings = false);

	ARCTERRAIN_API FArcTerrainTierLadder ResolveTierLadder(const AArcTerrain& Terrain,
		const FArcTerrainTileAtlasLayout& Layout, bool bEmitWarnings = true);

	ARCTERRAIN_API FArcTerrainWeightLadder ResolveWeightTierLadder(const AArcTerrain& Terrain,
		const FArcTerrainTileAtlasLayout& Layout, int32 ResolvedCellWeightQuads, bool bEmitWarnings = true);
}
