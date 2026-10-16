// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTypes.h"
#include "Composite/ArcTerrainStampMask.h"
#include "Tiles/ArcTerrainSourceCache.h"

class FArcTerrainSourceCache;
class UArcTerrainStampComponent;

/**
 * CPU evaluation of a cell's stamp stack. Runs Shaders/ArcTerrainComposite.usf (ComputeStamp plus
 * CompositeHeightPS / CompositeWeightPS) formula for formula: same patch transform, same half-texel UV
 * map, same edge/circle masks, and per target the reconstruction and accumulation that target uses —
 * bicubic Catmull-Rom (engine Texture2DSampleBicubic) with per-BlendMode height accumulation including
 * SmoothMin/SmoothMax, bilinear with the two quantised RGBA8 blend passes for weights.
 *
 * THE SAMPLE GRID IS A PARAMETER (FCellRaster), supplied by the caller: same field, reconstructed at
 * whatever density and origin the consumer needs.
 *
 * NOT BIT-IDENTICAL TO THE GPU, and not trying to be. The shader demotes the stamp-rebased texel
 * index to float before scaling it to patch-local (ArcTerrainComposite.usf: DFDemote(DFSubtract(...))
 * * InTexelToLocal); this keeps that one chain in double, so the CPU sample position is the MORE
 * accurate of the two and the surfaces differ sub-mm. Chasing bit-parity by demoting to float here
 * would trade real accuracy for a number that matches. The only place the divergence can exceed
 * sub-mm is a HARD stamp edge (EdgeFalloffBand == 0 and no circle mask), where a sample landing on
 * the boundary can fall inside on one side and outside on the other — one texel of the cell's grid.
 * Weights carry a second, coarser tolerance: D3D specifies blending into a UNORM target as correctly
 * rounded within a tolerance rather than bit-exact, so a composited weight texel may differ by 1 LSB.
 */
namespace ArcTerrainCpuComposite
{
	/** Everything one stamp pass needs to turn a raster position into a masked source value — the
	 *  ComputeStamp half of the shader, identical for height and weight passes. */
	struct FStampPlacement
	{
		FArcTerrainSourceCpuFloatsPtr Source;

		FArcTerrainSourceCpuFloatsPtr MaskSource;

		float EncodingScale = 1.0f;
		float ZeroInEncoding = 0.0f;

		/** Terrain-local (z = 0) point mapping to patch-local origin; the stamp rebase the shader does
		 *  in double-float. */
		FVector2D PatchOriginTerrainLocal = FVector2D::ZeroVector;

		/** Upper-left 2x2 linear block of LocalToPatch, row-vector convention (mul(v, M)). */
		double LinearM00 = 1.0;
		double LinearM01 = 0.0;
		double LinearM10 = 0.0;
		double LinearM11 = 1.0;

		FVector2f CoverageXY = FVector2f::ZeroVector;
		FVector2f UVScale = FVector2f(1.0f, 1.0f);
		FVector2f UVBias = FVector2f::ZeroVector;

		ArcTerrainStampMask::FMaskParams Mask;
	};

	/** One height-contributing pass of one stamp, in composite order. A stamp emits its height source
	 *  first, then one op per carve layer (always Additive — the carve contract). */
	struct FStampHeightOp
	{
		FStampPlacement Placement;

		EArcTerrainBlendMode BlendMode = EArcTerrainBlendMode::Additive;
		float MinMaxSoftness = 0.0f;

		float ValueScale = 1.0f;
		float ValueBias = 0.0f;
	};

	/** One weight-contributing pass of one stamp, in composite order: one (stamp, layer) pair painting
	 *  one plane. The shader's per-channel value scale/bias are fixed at 1/0 for weight passes
	 *  (BuildCompositeJob), so the decoded source value IS the weight. */
	struct FStampWeightOp
	{
		FStampPlacement Placement;

		/** Index into FCellWeightEval::PlaneChannels — the plane this pass blends into. */
		int32 PlaneIndex = INDEX_NONE;
	};

	/**
	 * The grid EvaluateCellHeights samples: SamplesPerSide^2 samples spaced TexelToLocal apart, with
	 * sample (0, 0) at global texel index TexelOrigin. Two rasters at the SAME TexelToLocal that share
	 * a global index sample the same terrain-local position — the property the composite's apron
	 * relies on; it does not hold across rasters of different density (collision, section, weight are
	 * disjoint index spaces).
	 */
	struct FCellRaster
	{
		FIntPoint TexelOrigin = FIntPoint::ZeroValue;

		float TexelToLocal = 0.0f;

		int32 SamplesPerSide = 0;
	};

	/** The collision grid: no apron, Quads + 1 samples from global index Key * Quads, so the grid spans
	 *  exactly one PATCH of the collision lattice. Key and PatchSize are that lattice's — the cell grid
	 *  is not an input, which is what lets a patch be smaller or larger than a cell. */
	FCellRaster MakeCollisionRaster(FArcTerrainCollisionKey Key, int32 Quads, double PatchSize);

	/** The baked SECTION grid: the composite tile itself — QuadsPerCell + 3 samples from global index
	 *  Key * QuadsPerCell - 1, the 1-texel apron whose texels reuse the neighbour's global indices
	 *  (Shaders/ArcTerrainComposite.usf apron convention), spaced CellSize / QuadsPerCell. */
	FCellRaster MakeSectionRaster(FArcTerrainCellKey Key, int32 QuadsPerCell, double CellSize);

	/** The WEIGHT tile grid: the same apron convention as the section tile at the terrain's independent
	 *  weight density (ResolveWeightQuads) — WeightQuads + 3 samples from global index
	 *  Key * WeightQuads - 1, spaced CellSize / WeightQuads. The GPU derives both from one
	 *  ComputeGlobalTexelOrigin / (TileResolution - 3) pair (ArcTerrainCompositeShaders.cpp
	 *  FillStampParameters), which is why the weight seam texel is shared across cells. */
	FCellRaster MakeWeightRaster(FArcTerrainCellKey Key, int32 WeightQuads, double CellSize);

	/** Everything EvaluateCellHeights needs for one cell, resolved on the game thread. */
	struct FCellHeightEval
	{
		TArray<FStampHeightOp> Ops;

		/** Heights start here, matching the composite's clear of the height chain. */
		float BaseHeightCm = 0.0f;

		FCellRaster Raster;

		/** False when a covering stamp's height contribution could not be resolved on the CPU: a raster
		 *  that did not survive cook, or a transient decode/provider failure. The evaluation would be
		 *  missing that stamp, so the caller must fall back to the baked payload rather than apply it. */
		bool bComplete = true;
	};

	/**
	 * Resolve SortedStamps (GatherSortedStamps order — the composite order) into ordered height ops.
	 * GAME THREAD ONLY: reads GetSourceDescriptor and the CPU source cache.
	 *
	 * A stamp that contributes no height at all is not an incompleteness — an unstamped or weight-only
	 * cell evaluates to BaseHeightCm, which is what the composite produces for it.
	 */
	ARCTERRAIN_API void GatherCellHeightOps(FArcTerrainSourceCache& SourceCache,
		const FArcTerrainConfig& Config, const FTransform& TerrainTransform, const FCellRaster& Raster,
		float BaseHeightCm, TConstArrayView<UArcTerrainStampComponent*> SortedStamps,
		FCellHeightEval& OutEval);

	/** Row-major Eval.Raster.SamplesPerSide^2 terrain-local heights in cm; empty when SamplesPerSide < 2.
	 *  Pure: no UObject, no RHI, no game-thread requirement. */
	ARCTERRAIN_API void EvaluateCellHeights(const FCellHeightEval& Eval, TArray<float>& OutHeights);

	/** Everything EvaluateCellWeightPlanes needs for one cell, resolved on the game thread. */
	struct FCellWeightEval
	{
		TArray<FStampWeightOp> Ops;

		FCellRaster Raster;

		/** Plane index -> global channel index, ascending. Supplied by the caller: the set of planes is
		 *  the cell's present-channel set, which a layer whose source failed to resolve still belongs to
		 *  (it composites as an all-zero plane, exactly as on the GPU). */
		TArray<int32> PlaneChannels;

		/** False when a covering layer's weight source could not be resolved on the CPU. Its plane would
		 *  be missing that layer's contribution while every key still matched, so the caller must not
		 *  persist or cache the result. */
		bool bComplete = true;
	};

	/**
	 * Resolve SortedStamps (GatherSortedStamps order — the composite order) into ordered weight ops, one
	 * per (stamp, Desc.Layers pair) that resolves to a channel with a plane. GAME THREAD ONLY: reads
	 * GetSourceDescriptor and the CPU source cache.
	 */
	void GatherCellWeightOps(FArcTerrainSourceCache& SourceCache, const FArcTerrainConfig& Config,
		const FTransform& TerrainTransform, const FCellRaster& Raster,
		TConstArrayView<int32> PlaneChannels,
		TConstArrayView<UArcTerrainStampComponent*> SortedStamps, FCellWeightEval& OutEval);

	/**
	 * One row-major Eval.Raster.SamplesPerSide^2 uint8 plane per Eval.PlaneChannels entry; empty when
	 * SamplesPerSide < 2 or no plane was asked for. Pure: no UObject, no RHI, no game-thread requirement.
	 *
	 * REQUANTISES TO 8 BITS TWICE PER OP, which is what the GPU's two hardware-blend passes into the
	 * RGBA8 tile do (ArcTerrainComposite.usf CompositeWeightPS + the WeightMultiply/WeightAdd blend
	 * states): D <- q8(D * (1 - Mask)) then D <- q8(D + saturate(Value) * Mask). A single float lerp
	 * accumulated across ops diverges from it wherever two stamps overlap on one channel.
	 */
	void EvaluateCellWeightPlanes(const FCellWeightEval& Eval, TArray<TArray<uint8>>& OutPlanes);
}
