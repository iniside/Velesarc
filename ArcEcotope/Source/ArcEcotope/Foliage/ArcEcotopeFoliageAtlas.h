// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Box.h"
#include "RenderGraphResources.h"
#include "RHIFwd.h"
#include "ShaderParameterMacros.h"

class AArcTerrain;
class FRDGBuilder;
class UArcTerrainChannelSet;
class UArcTerrainSubsystem;

/** Atlas slices, and therefore resident cells per patch. Fixed by the uint32 residency mask and the
 *  fixed-size per-slot origin arrays below; the shader indexes both by slot. */
inline constexpr int32 GArcEcotopeFoliageMaxCells = 16;

/** Terrain-local cm for positions, source-tile TEXELS for origins and sub-rects. Never world space,
 *  never atlas UV — the shader derives UV. */
struct FArcEcotopeFoliageAtlasSlot
{
	FIntPoint Key = FIntPoint::ZeroValue;
	FIntPoint HeightSrcOrigin = FIntPoint::ZeroValue;
	FIntPoint WeightSrcOrigin = FIntPoint::ZeroValue;

	/** Row-major index in Slots; the shader recomputes it from (Key - OriginCell), so the two must agree. */
	int32 Slice = INDEX_NONE;
};

/** The full row-major cell rectangle covering a patch's bounds. Every cell of the rectangle gets a
 *  slot whether or not it is resident — residency is stamped into a bitmask, not into the layout. */
struct FArcEcotopeFoliageAtlasLayout
{
	TArray<FArcEcotopeFoliageAtlasSlot> Slots;

	FIntPoint HeightSubRect = FIntPoint::ZeroValue;
	FIntPoint WeightSubRect = FIntPoint::ZeroValue;

	FIntPoint OriginCell = FIntPoint::ZeroValue;
	FIntPoint CellCount = FIntPoint::ZeroValue;

	/** Cells per axis before the MaxCells shrink — what the bounds actually span. */
	FIntPoint RequestedCellCount = FIntPoint::ZeroValue;

	/** MaxCells shrank the rectangle, so part of the patch is not atlased at all. */
	bool bCellRectClamped = false;

	/** A padded sub-rect was slid back inside its tile; lossless, the window still covers the slot. */
	bool bSubRectClamped = false;
};

/**
 * One cell's copy source, uploaded from ArcTerrain tile BYTES — ArcTerrain hands out no textures.
 *
 * The ref is written by the upload render command and read by BuildTerrainAtlas, both on the render
 * thread and in that order, so FIFO command order is what makes it valid by the time it is registered.
 * The shared box is what carries it back out of a command the game thread does not wait on.
 */
struct FArcEcotopeFoliageTileTexture
{
	FTextureRHIRef TextureRHI;
};

using FArcEcotopeFoliageTileTexturePtr = TSharedPtr<FArcEcotopeFoliageTileTexture, ESPMode::ThreadSafe>;

/** Counted refs plus POD, captured whole on the game thread. Never an FTextureResource* — see
 *  ArcTerrainDataInterface.h's FSlot for the use-after-free that rule was bought with. */
struct FArcEcotopeFoliageTerrainData
{
	struct FSlot
	{
		FArcEcotopeFoliageTileTexturePtr HeightTile;
		FArcEcotopeFoliageTileTexturePtr WeightTile;

		/** The slice index of a given global channel differs cell to cell. */
		int32 WeightSourceSlice = INDEX_NONE;

		FIntPoint HeightSrcOrigin = FIntPoint::ZeroValue;
		FIntPoint WeightSrcOrigin = FIntPoint::ZeroValue;
		bool bResident = false;
	};

	TArray<FSlot> Slots;

	FIntPoint AtlasOriginCell = FIntPoint::ZeroValue;
	FIntPoint AtlasCellCount = FIntPoint::ZeroValue;
	FIntPoint HeightSubRect = FIntPoint(1, 1);
	FIntPoint WeightSubRect = FIntPoint(1, 1);

	/** Terrain-local cm extrema across the cells this atlas resolved, for the patch's Z bounds. */
	float LocalHeightMinCm = 0.0f;
	float LocalHeightMaxCm = 0.0f;

	/** LWC split of the terrain transform: the matrices are relative to LWCTile. */
	FMatrix44f WorldToTerrainLocal = FMatrix44f::Identity;
	FMatrix44f TerrainLocalToWorld = FMatrix44f::Identity;
	FVector3f TerrainInvNonUniformScale = FVector3f::OneVector;
	FVector3f LWCTile = FVector3f::ZeroVector;

	float CellSize = 1.0f;
	float HeightTileRes = 4.0f;
	float WeightTileRes = 4.0f;

	/** Terrain-local cm per source texel (CellSize / (TileRes - 3)) — the divisor of the normal's
	 *  central difference, never its tap offset. */
	float TexelToLocal = 1.0f;

	/** Global channel index the layer filter samples; INDEX_NONE = no filter. */
	int32 LayerChannel = INDEX_NONE;
};

/** Loose HLSL globals of Shaders/Private/ArcEcotopeFoliageTerrain.ush, 1:1 by name. Included (not
 *  nested) into a kernel's parameter struct, so the member names reach HLSL unprefixed. */
BEGIN_SHADER_PARAMETER_STRUCT(FArcEcotopeFoliageTerrainParameters, ARCECOTOPE_API)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, ArcTerrainHeightAtlas)
	SHADER_PARAMETER_SAMPLER(SamplerState, ArcTerrainHeightAtlasSampler)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, ArcTerrainWeightAtlas)
	SHADER_PARAMETER_SAMPLER(SamplerState, ArcTerrainWeightAtlasSampler)
	SHADER_PARAMETER(uint32, ArcTerrainResidencyMask)
	SHADER_PARAMETER(FMatrix44f, ArcTerrainWorldToLocal)
	SHADER_PARAMETER(FMatrix44f, ArcTerrainLocalToWorld)
	SHADER_PARAMETER(FVector3f, ArcTerrainInvNonUniformScale)
	SHADER_PARAMETER(FVector3f, ArcTerrainLWCTile)
	SHADER_PARAMETER(FIntPoint, ArcTerrainAtlasOriginCell)
	SHADER_PARAMETER(FIntPoint, ArcTerrainAtlasCellCount)
	SHADER_PARAMETER(FVector2f, ArcTerrainHeightSubRectInv)
	SHADER_PARAMETER(FVector2f, ArcTerrainWeightSubRectInv)
	// FVector4f, not FVector2f: SHADER_PARAMETER_ARRAY's element type carries
	// static_assert((sizeof(T) % 16) == 0), so an FVector2f array does not compile. Only .xy/.z read.
	SHADER_PARAMETER_ARRAY(FVector4f, ArcTerrainHeightSrcOriginPerSlot, [GArcEcotopeFoliageMaxCells])
	SHADER_PARAMETER_ARRAY(FVector4f, ArcTerrainWeightSrcOriginPerSlot, [GArcEcotopeFoliageMaxCells])
	SHADER_PARAMETER(float, ArcTerrainCellSize)
	SHADER_PARAMETER(float, ArcTerrainHeightTileRes)
	SHADER_PARAMETER(float, ArcTerrainWeightTileRes)
	SHADER_PARAMETER(float, ArcTerrainTexelToLocal)
	SHADER_PARAMETER(int32, ArcTerrainLayerChannel)
END_SHADER_PARAMETER_STRUCT()

namespace ArcEcotopeFoliageAtlas
{

/**
 * LocalBounds is TERRAIN-LOCAL. The cell rectangle is edge-inclusive to match
 * ArcTerrain::CellsIntersectingLocalBounds, so residency and slots key against one another.
 *
 * Sub-rects are padded 2 texels per side — one for an edge sample's bilinear footprint, one for the
 * normal's central-difference tap — then unified, since one Texture2DArray has one size.
 */
ARCECOTOPE_API FArcEcotopeFoliageAtlasLayout ComputeAtlasLayout(const FBox& LocalBounds, double CellSize,
	int32 HeightTileRes, int32 WeightTileRes, int32 MaxCells);

/** Cell-local fraction [0,1] -> continuous texel coordinate in a (TileRes = Quads + 3) apron tile:
 *  the inner grid spans texels [1, 1 + Quads], so fraction 0 sits at texel centre 1.5. */
ARCECOTOPE_API float WorldToTileTexel(float Frac, int32 TileRes);

/**
 * TRI-STATE layer-filter resolution, GAME THREAD only (the shader never sees an FName):
 *   - NAME_None                                  -> INDEX_NONE, bOutAuthoringError false. Filter off.
 *   - name set, ChannelSet null / name not found -> INDEX_NONE, bOutAuthoringError TRUE.
 *   - resolved                                   -> the channel's stable global index.
 */
ARCECOTOPE_API int32 ResolveLayerFilterChannel(FName LayerChannel, const UArcTerrainChannelSet* ChannelSet,
	bool& bOutAuthoringError);

/**
 * GAME THREAD. False means "this patch gets no foliage": nothing sampleable in range, an unresolvable
 * LayerChannel, or no cell in range carrying it.
 *
 * With a named LayerChannel a cell missing that channel is left NON-RESIDENT, not merely unfiltered —
 * a null weight section means "weightless OR not resolved yet", and unfiltered would paint it whole.
 */
ARCECOTOPE_API bool BuildTerrainData(AArcTerrain& Terrain, const FBox& WorldBounds, FName LayerChannel,
	FArcEcotopeFoliageTerrainData& OutData);

/** RENDER THREAD: clears each atlas it creates — the weight atlas only when some slot carries weight —
 *  then copies up to two slices per resident cell. Empty slots keep the cleared slice; an absent atlas
 *  binds a system texture, never a null SRV. */
ARCECOTOPE_API void BuildTerrainAtlas(FRDGBuilder& GraphBuilder, const FArcEcotopeFoliageTerrainData& Data,
	FArcEcotopeFoliageTerrainParameters& OutParameters);

} // namespace ArcEcotopeFoliageAtlas
