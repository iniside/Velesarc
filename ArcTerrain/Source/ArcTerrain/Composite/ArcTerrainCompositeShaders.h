// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "ArcTerrainTypes.h"
#include "Render/ArcTerrainTileAtlas.h"
#include "Tiles/ArcTerrainSourceCache.h"

class FTextureRenderTargetResource;

/**
 * Per-stamp parameters shared by the height and weight composite pixel shaders
 * (Shaders/ArcTerrainComposite.usf). Names match the loose HLSL globals 1:1.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainCompositeStampParameters, ARCTERRAIN_API)
	SHADER_PARAMETER(FIntPoint, InTexelOrigin)             // GLOBAL texel index (exact int2) of the tile's texel (0,0), which is the APRON texel: CellKey * QuadsPerCell - 1
	SHADER_PARAMETER(float, InTexelToLocal)                // CellSize / QuadsPerCell (= CellSize / (TileResolution - 3)) — apron-independent texel spacing
	SHADER_PARAMETER(FMatrix44f, InLocalToPatch)           // terrain-local -> stamp patch-local (row-vector mul); the shader applies ONLY the linear block — translation is folded into InStampOriginTexel*
	SHADER_PARAMETER(FVector2f, InStampOriginTexelHigh)    // DF (hi, lo) split of the stamp's patch origin in TEXEL-INDEX units at this pass's density:
	SHADER_PARAMETER(FVector2f, InStampOriginTexelLow)     // (-L^-1 * T) / InTexelToLocal, computed in double from InLocalToPatch (FillStampParameters)
	SHADER_PARAMETER(FVector2f, InCoverageXY)              // stamp footprint extent (cm, patch-local)
	SHADER_PARAMETER(FVector2f, InUVScale)                 // half-texel map: footprint UV -> texel-centered UV
	SHADER_PARAMETER(FVector2f, InUVBias)
	SHADER_PARAMETER(FVector4f, InChannelDot)              // source channel selector (dot mask)
	SHADER_PARAMETER(float, InEncodingScale)               // value = (texel.ch - Zero) * Scale * ValueScale + ValueBias
	SHADER_PARAMETER(float, InZeroInEncoding)
	SHADER_PARAMETER(float, InValueScale)
	SHADER_PARAMETER(float, InValueBias)
	SHADER_PARAMETER(float, InEdgeFalloffBand)
	SHADER_PARAMETER(float, InEdgeSharpness)
	SHADER_PARAMETER(float, InCornerRadius)
	SHADER_PARAMETER(uint32, InCircleMask)
	SHADER_PARAMETER(FVector2f, InCircleCenterOffset)
	SHADER_PARAMETER(float, InCircleRadius)
	SHADER_PARAMETER(float, InCircleFalloffBand)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, InSourceTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InSourceSampler)
	SHADER_PARAMETER(FVector4f, InSourceTextureSize)       // xy = source texel dims, zw = 1/dims — feeds the height path's bicubic Catmull-Rom (Texture2DSampleBicubic)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, InMaskTexture)
	SHADER_PARAMETER(uint32, InUseMaskField)
END_SHADER_PARAMETER_STRUCT()

/**
 * Height composite PS. Two permutations:
 *  - MIN_MAX_BLEND=0 — hardware-blend path: Additive outputs (delta*mask, mask) under BF_One/BF_One;
 *    AlphaBlend outputs (absolute, mask) under BF_SourceAlpha/BF_InverseSourceAlpha.
 *  - MIN_MAX_BLEND=1 — ping-pong path: reads the previous tile state (InExistingHeight SRV), writes
 *    Lerp(Existing, SmoothMin/Max(Existing, Target, Softness), Mask) opaquely to the other scratch.
 */
class FArcTerrainCompositeHeightPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCompositeHeightPS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCompositeHeightPS, FGlobalShader);

public:
	class FMinMaxBlendDim : SHADER_PERMUTATION_BOOL("MIN_MAX_BLEND");
	using FPermutationDomain = TShaderPermutationDomain<FMinMaxBlendDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcTerrainCompositeStampParameters, Stamp)
		SHADER_PARAMETER(uint32, InHeightOutputMode) // 0 = premultiplied additive, 1 = absolute + mask alpha
		SHADER_PARAMETER(uint32, InMinMaxMode)       // 0 = SmoothMin, 1 = SmoothMax
		SHADER_PARAMETER(float, InMinMaxSoftness)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, InExistingHeight)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * Weight composite PS: lerp(Existing, saturate(Value), Mask) per terrain channel — NOTE the value
 * is saturated BEFORE the lerp (not saturate(lerp(E, V, M)): for E=0, M=0.5, V=2 this yields 0.5,
 * not 1.0). This is the shipped semantic; the Step 9 CPU parity reference MUST use exactly
 * lerp(Existing, saturate(Value), Mask). Split into TWO hardware-blend passes so an arbitrary
 * channel subset of an RGBA8 tile (including its alpha channel) blends independently:
 *  pass 0 (multiply): outputs ChannelMask * Mask under BF_Zero / BF_InverseSourceColor(+Alpha)
 *                     -> Dst_ch *= (1 - Mask) on affected channels, others untouched;
 *  pass 1 (add):      outputs ChannelMask * saturate(Value) * Mask under BF_One / BF_One
 *                     -> Dst_ch += saturate(Value) * Mask.
 */
class FArcTerrainCompositeWeightPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCompositeWeightPS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCompositeWeightPS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcTerrainCompositeStampParameters, Stamp)
		SHADER_PARAMETER(uint32, InWeightPassMode)   // 0 = mask-multiply, 1 = premultiplied add
		SHADER_PARAMETER(FVector4f, InTargetChannelMask)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

class FArcTerrainNormalizeWeightsPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainNormalizeWeightsPS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainNormalizeWeightsPS, FGlobalShader);

public:
	static constexpr int32 MaxWeightArrays = 6;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, InArrayIndex)
		SHADER_PARAMETER(uint32, InNumArrays)
		SHADER_PARAMETER(uint32, InPresentMask)
		SHADER_PARAMETER_RDG_TEXTURE_ARRAY(Texture2D, InScratch, [MaxWeightArrays])
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

namespace ArcTerrainComposite
{
	/** One fullscreen stamp pass of a cell composite, fully resolved on the game thread. */
	struct FStampPass
	{
		enum class ETarget : uint8
		{
			Height,
			Weight
		};

		ETarget Target = ETarget::Height;

		/** Weight target: which RGBA8 tile and which of its 4 channels. */
		int32 WeightRTIndex = INDEX_NONE;
		int32 WeightChannel = INDEX_NONE;

		/** Height blend. Carve layers force Additive regardless of the stamp's mode. */
		EArcTerrainBlendMode BlendMode = EArcTerrainBlendMode::Additive;
		float MinMaxSoftness = 0.0f;

		/** GPU source (cache entry; TextureRHI set by the earlier upload command). */
		FArcTerrainSourceTexturePtr Source;

		FArcTerrainSourceTexturePtr MaskSource;

		/** Decode + placement: value = (texel.ch - Zero) * Scale * ValueScale + ValueBias. */
		FVector4f ChannelDot = FVector4f(1.0f, 0.0f, 0.0f, 0.0f);
		float EncodingScale = 1.0f;
		float ZeroInEncoding = 0.0f;
		float ValueScale = 1.0f;
		float ValueBias = 0.0f;

		/** Geometry: terrain-local -> patch-local + footprint + half-texel sample map. */
		FMatrix44f LocalToPatch = FMatrix44f::Identity;
		/** The terrain-local (z=0 plane) point that maps to patch-local origin: -L^-1 * T over
		 *  LocalToPatch's upper-left 2x2 block L and xy translation T, solved in DOUBLE from the
		 *  float matrix entries (infinite-plane Step 2). FillStampParameters divides it by the
		 *  pass's texel spacing and splits hi/lo into InStampOriginTexel* — the DF stamp rebase
		 *  that replaces the shader's old large-minus-large float cancellation. Identical for the
		 *  same stamp across neighbouring cells (input is the cell-independent StampToTerrain),
		 *  which the shared-edge bit-identity argument relies on. */
		FVector2d PatchOriginTerrainLocal = FVector2d::ZeroVector;
		FVector2f CoverageXY = FVector2f::ZeroVector;
		FVector2f UVScale = FVector2f(1.0f, 1.0f);
		FVector2f UVBias = FVector2f::ZeroVector;

		/** Falloff masks. */
		float EdgeFalloffBand = 0.0f;
		float EdgeSharpness = 1.0f;
		float CornerRadius = 0.0f;
		uint32 bCircleMask = 0;
		FVector2f CircleCenterOffset = FVector2f::ZeroVector;
		float CircleRadius = 0.0f;
		float CircleFalloffBand = 0.0f;
	};

	/** A full cell recomposite: clear -> ordered stamp passes -> final height copy into the atlas slice
	 *  and/or the caller's staging tile. */
	struct FCellCompositeJob
	{
		/** THE HEIGHT DESTINATION: the cell's sub-rect of AtlasHeightSlice, at AtlasHeightDestOrigin.
		 *  Null resources / INDEX_NONE slice = the cell's sector holds no slice, so this composite only
		 *  fills HeightTileResource (the modal sync/bake paths) and publishes nothing. */
		TSharedPtr<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe> AtlasResources;
		int32 AtlasHeightSlice = INDEX_NONE;
		FIntPoint AtlasHeightDestOrigin = FIntPoint::ZeroValue;

		/** Optional pooled R32f staging tile receiving the same composited height — the readable copy the
		 *  synchronous paths (PIE-entry collision bootstrap, region bake) need. Null on the tick path. */
		FTextureRenderTargetResource* HeightTileResource = nullptr;

		TArray<FTextureRenderTargetResource*> WeightRTResources;

		/** Array holding the base lane (ArcTerrain::ResolveBaseChannelIndex / 4), INDEX_NONE when the
		 *  terrain's channel set cannot address the base — the normalize is then refused, not retargeted. */
		int32 BaseWeightArrayIndex = INDEX_NONE;

		/** Two pooled R32f scratch tiles: the height ping-pong chain (hardware blends render into the
		 *  current one; Min/Max reads current, writes the other, swaps); the final state is copied out
		 *  of the current one. */
		FTextureRenderTargetResource* ScratchAResource = nullptr;
		FTextureRenderTargetResource* ScratchBResource = nullptr;

		/** Tile texel count per axis = QuadsPerCell + 3: the (QuadsPerCell + 1) inner texels plus a
		 *  1-texel APRON on every side. The apron texel carries the NEIGHBOUR's value by construction
		 *  (same global texel index -> bit-identical value), giving edge-gradient consumers (VF normals,
		 *  PS-side taps) a valid +-1 texel neighbourhood at the cell border. */
		int32 TileResolution = 0;

		/** Global texel index of the tile's texel (0,0) — the bottom-left APRON texel = CellKey *
		 *  QuadsPerCell - 1 (inner texel 0 sits at global CellKey * QuadsPerCell). Texels are addressed
		 *  GLOBALLY so the border texel shared by adjacent cells has the SAME integer index in both
		 *  composites -> identical canonical int->DF conversion and stamp rebase (ComputeStamp) ->
		 *  bit-identical shared-edge (and apron) values, exact arbitrarily far from origin. */
		FIntPoint TexelOrigin = FIntPoint::ZeroValue;
		float CellSize = 0.0f;

		/** FArcTerrainHeightOptions::BaseHeightCm — the value the height chain starts from, so a cell
		 *  no stamp covers composites to the authored ground level rather than to 0. */
		float BaseHeightCm = 0.0f;

		/** Weight-tile texel count per axis = WeightQuads + 3 (1-texel apron per side), DECOUPLED from
		 *  the height TileResolution above (weightmap-decouple Stage 1 Step 1): weight passes composite
		 *  at the source-weightmap density, not the mesh quad count. Height passes render under
		 *  Viewport(0,0,TileResolution,TileResolution); weight passes under
		 *  Viewport(0,0,WeightTileResolution,WeightTileResolution). */
		int32 WeightTileResolution = 0;

		/** Global texel index of the WEIGHT tile's texel (0,0) — the bottom-left APRON texel =
		 *  CellKey * WeightQuads - 1 (inner texel 0 at global CellKey * WeightQuads). Independent of the
		 *  height TexelOrigin so the weight seam shares the border weight-texel GLOBAL index across
		 *  adjacent cells -> bit-identical composited value (plan I1). WeightQuads is uniform per terrain
		 *  and need NOT be power-of-two. */
		FIntPoint WeightTexelOrigin = FIntPoint::ZeroValue;

		TArray<FStampPass> Passes;

		/** DIAGNOSTICS ONLY: the cell this job composites, and arc.Terrain.Weights.VerboseComposite read on
		 *  the GAME thread and carried in — the cvar is game-thread-only and this job runs on the render
		 *  thread (same rule as ExecuteFill's bLogVerbose). */
		FArcTerrainCellKey CellKey = FArcTerrainCellKey::ZeroValue;
		bool bLogWeightVerbose = false;
	};

	/** Render-thread entry: builds a local FRDGBuilder over the job and executes it
	 *  (pattern: LandscapeEditLayers.cpp — RegisterExternalTexture(CreateRenderTarget(...)),
	 *  AllocParameters, FPixelShaderUtils::AddFullscreenPass, SetTextureAccessFinal, Execute). */
	ARCTERRAIN_API void RenderCompositeCell(FRHICommandListImmediate& RHICmdList, const FCellCompositeJob& Job);
}
