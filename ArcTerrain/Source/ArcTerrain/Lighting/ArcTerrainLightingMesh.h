// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Math/Box.h"

#include "ArcTerrainTypes.h"

struct FArcTerrainSurfaceCellCache;

namespace ArcTerrainLightingMesh
{
	struct FMesh
	{
		TArray<FVector3f> Positions;
		TArray<FVector3f> Normals;
		TArray<FVector3f> Tangents;
		TArray<FVector3f> Bitangents;
		TArray<FVector2f> UV0;
		TArray<FVector2f> UV1;
		TArray<FVector2f> UV2;
		TArray<uint32> Indices;
		FBox3f LocalBounds = FBox3f(ForceInit);
	};

	enum class ESkirtEdge : uint8
	{
		NegX = 0,
		PosX = 1,
		NegY = 2,
		PosY = 3,
	};

	constexpr int32 GSkirtEdgeCount = 4;

	constexpr int32 GLightingHeightQuads = 64;
	constexpr int32 GLightingWeightQuads = 64;

	static_assert(FMath::IsPowerOfTwo(GLightingHeightQuads) && FMath::IsPowerOfTwo(GLightingWeightQuads),
		"The lighting rasters' seam values must agree bit-for-bit between neighbouring cells");

	constexpr int32 GHeightfieldResolution = 64;
	constexpr float GHeightfieldScaleZ = 100.0f;

	ARCTERRAIN_API double HeightfieldQuadToLocal(double CellSize);

	ARCTERRAIN_API void BuildCellMesh(
		const FArcTerrainSurfaceCellCache& Cell,
		double CellSize,
		float ErrorToleranceCm,
		const float (&SkirtDepthCm)[GSkirtEdgeCount],
		FMesh& Out);

	ARCTERRAIN_API void BuildFlatCellMesh(
		float HeightCm,
		double CellSize,
		const float (&SkirtDepthCm)[GSkirtEdgeCount],
		FMesh& Out);

	struct FFarBlockSource
	{
		FArcTerrainCellKey Key = FArcTerrainCellKey::ZeroValue;
		bool bFlat = true;
		float FlatHeightCm = 0.0f;

		/** Only when !bFlat; row-major (SourceQuads+3)^2. */
		TArray<float> HeightsWithApron;
	};

	/** One RTIN tree per cell, never one over the block: bisection needs a 2^k+1 square, and the
	 *  per-vertex error pyramid of a K-cell block grows as (K*SourceQuads+1)^2. */
	ARCTERRAIN_API void BuildFarBlockMesh(
		TConstArrayView<FFarBlockSource> Sources,
		int32 SourceQuads,
		double CellSize,
		FArcTerrainCellKey BlockOriginCell,
		float ErrorToleranceCm,
		float SkirtDepthCm,
		float SinkCm,
		FMesh& Out);

	/** Material height raster: float32 cm, symmetric vertex-lattice downsample, output apron included. */
	ARCTERRAIN_API void ResampleHeightPlane(
		TConstArrayView<float> HeightsWithApron,
		int32 SrcQuads,
		TArray<float>& OutFloats);

	/** Material weight raster: one channel, uint8, symmetric vertex-lattice downsample, output apron included. */
	ARCTERRAIN_API void ResampleWeightPlane(
		TConstArrayView<uint8> BytesWithApron,
		int32 SrcQuads,
		TArray<uint8>& OutBytes);

	ARCTERRAIN_API void EncodePackedHeight(
		TConstArrayView<float> HeightsWithApron,
		int32 Quads,
		TArray<uint8>& OutRG);
}
