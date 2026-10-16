// Copyright Lukasz Baran. All Rights Reserved.

#include "Composite/ArcTerrainCompositeShaders.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainModule.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalRenderResources.h"
#include "Math/DoubleFloat.h"
#include "PixelShaderUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RHIStaticStates.h"
#include "SystemTextures.h"
#include "TextureResource.h"

bool FArcTerrainCompositeHeightPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FArcTerrainCompositeHeightPS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("COMPOSITE_HEIGHT"), 1);
}

bool FArcTerrainCompositeWeightPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FArcTerrainCompositeWeightPS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("COMPOSITE_WEIGHT"), 1);
}

bool FArcTerrainNormalizeWeightsPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FArcTerrainNormalizeWeightsPS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("COMPOSITE_NORMALIZE"), 1);
	OutEnvironment.SetDefine(TEXT("MAX_WEIGHT_ARRAYS"), MaxWeightArrays);
	// CPU and GPU must agree on the base channel's (array, component) split with no compile error or
	// assert if they diverge — inject the C++ constant instead of a second HLSL literal.
	OutEnvironment.SetDefine(TEXT("ARC_TERRAIN_BASE_CHANNEL"), ArcTerrain::GBaseChannelIndex);
}

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCompositeHeightPS,
	"/Plugin/ArcTerrain/ArcTerrainComposite.usf", "CompositeHeightPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArcTerrainCompositeWeightPS,
	"/Plugin/ArcTerrain/ArcTerrainComposite.usf", "CompositeWeightPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArcTerrainNormalizeWeightsPS,
	"/Plugin/ArcTerrain/ArcTerrainComposite.usf", "NormalizeWeightsPS", SF_Pixel);

namespace ArcTerrainCompositeRenderPrivate
{
	using namespace ArcTerrainComposite;

	void FillStampParameters(FArcTerrainCompositeStampParameters& Out, const FStampPass& Pass,
		const FCellCompositeJob& Job)
	{
		// Target-aware texel domain (weightmap-decouple Stage 1 Step 1): weight passes composite at the
		// WEIGHT tile's resolution/origin, height passes at the height tile's. Both spacings drop the
		// 1-texel apron (divisor is Quads = TileResolution - 3, the inner grid spanning the cell), so the
		// .usf stays parameterized purely off InTexelOrigin/InTexelToLocal and needs no change.
		if (Pass.Target == FStampPass::ETarget::Weight)
		{
			Out.InTexelOrigin = Job.WeightTexelOrigin;
			Out.InTexelToLocal = Job.CellSize / static_cast<float>(Job.WeightTileResolution - 3);
		}
		else
		{
			Out.InTexelOrigin = Job.TexelOrigin;
			// Texel spacing = CellSize / QuadsPerCell, INDEPENDENT of the 1-texel apron: the tile carries
			// N+3 texels (Quads = TileResolution - 3) but the inner grid still spans exactly QuadsPerCell
			// quads across the cell, so the divisor is QuadsPerCell, not (TileResolution - 1).
			Out.InTexelToLocal = Job.CellSize / static_cast<float>(Job.TileResolution - 3);
		}
		// DF stamp rebase (infinite-plane Step 2): the stamp's patch origin in TEXEL-INDEX units at
		// THIS pass's density — divided by the SAME float InTexelToLocal the shader multiplies
		// RelTexel by (as a double, so the quotient is near-exact), then hi/lo split. Deterministic
		// double math on cell-independent inputs -> identical bits for the same stamp in adjacent
		// cells, which the shared-edge bit-identity argument requires.
		const FVector2d OriginTexel =
			Pass.PatchOriginTerrainLocal / static_cast<double>(Out.InTexelToLocal);
		const FDFVector2 OriginTexelDF(OriginTexel);
		Out.InStampOriginTexelHigh = OriginTexelDF.High;
		Out.InStampOriginTexelLow = OriginTexelDF.Low;
		Out.InLocalToPatch = Pass.LocalToPatch;
		Out.InCoverageXY = Pass.CoverageXY;
		Out.InUVScale = Pass.UVScale;
		Out.InUVBias = Pass.UVBias;
		Out.InChannelDot = Pass.ChannelDot;
		Out.InEncodingScale = Pass.EncodingScale;
		Out.InZeroInEncoding = Pass.ZeroInEncoding;
		Out.InValueScale = Pass.ValueScale;
		Out.InValueBias = Pass.ValueBias;
		Out.InEdgeFalloffBand = Pass.EdgeFalloffBand;
		Out.InEdgeSharpness = Pass.EdgeSharpness;
		Out.InCornerRadius = Pass.CornerRadius;
		Out.InCircleMask = Pass.bCircleMask;
		Out.InCircleCenterOffset = Pass.CircleCenterOffset;
		Out.InCircleRadius = Pass.CircleRadius;
		Out.InCircleFalloffBand = Pass.CircleFalloffBand;
		Out.InSourceTexture = Pass.Source->TextureRHI;
		// SF_Bilinear is REQUIRED by both reconstruction paths: the weight path samples it directly,
		// and the height path's 5-tap bicubic Catmull-Rom (Texture2DSampleBicubic) is built out of
		// bilinear taps — do not switch to SF_Point without rewriting the shader to a 16-tap kernel.
		Out.InSourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		// Callers guarantee TextureRHI validity before FillStampParameters (RenderCompositeCell skips
		// passes whose upload failed), so the size query is safe on the render thread.
		const FIntPoint SourceSize = Pass.Source->TextureRHI->GetSizeXY();
		const float SizeX = static_cast<float>(FMath::Max(SourceSize.X, 1));
		const float SizeY = static_cast<float>(FMath::Max(SourceSize.Y, 1));
		Out.InSourceTextureSize = FVector4f(SizeX, SizeY, 1.0f / SizeX, 1.0f / SizeY);

		const bool bUseMaskField = Pass.MaskSource.IsValid() && Pass.MaskSource->TextureRHI.IsValid();
		// WHITE, never black and never null: a null bound texture is a fatal
		// (EmitNullShaderParameterFatalError), and white is the multiply identity — a mistaken branch
		// leaves the stamp intact instead of erasing every one of its texels.
		Out.InMaskTexture = bUseMaskField ? Pass.MaskSource->TextureRHI : GWhiteTexture->TextureRHI;
		Out.InUseMaskField = bUseMaskField ? 1u : 0u;
	}

	static int32 GReportedWeightArrayOverflow = INDEX_NONE;
	static int32 GReportedMissingBaseWeightArray = INDEX_NONE;

	bool ShouldReportOnce(int32& ReportedKey, int32 Key)
	{
		if (ReportedKey == Key)
		{
			return false;
		}
		ReportedKey = Key;
		return true;
	}

	FRDGTextureRef RegisterTileTexture(FRDGBuilder& GraphBuilder, FTextureRenderTargetResource* Resource,
		const TCHAR* Name)
	{
		if (Resource == nullptr || !Resource->TextureRHI.IsValid())
		{
			return nullptr;
		}
		return GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Resource->TextureRHI, Name));
	}
}

void ArcTerrainComposite::RenderCompositeCell(FRHICommandListImmediate& RHICmdList, const FCellCompositeJob& Job)
{
	using namespace ArcTerrainCompositeRenderPrivate;

	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ArcTerrainCompositeCell"));

	FRDGTextureRef HeightTile = RegisterTileTexture(GraphBuilder, Job.HeightTileResource, TEXT("ArcTerrainHeightTile"));
	FRDGTextureRef ScratchA = RegisterTileTexture(GraphBuilder, Job.ScratchAResource, TEXT("ArcTerrainHeightScratchA"));
	FRDGTextureRef ScratchB = RegisterTileTexture(GraphBuilder, Job.ScratchBResource, TEXT("ArcTerrainHeightScratchB"));
	FRDGTextureRef HeightAtlas = nullptr;
	if (Job.AtlasHeightSlice != INDEX_NONE && Job.AtlasResources.IsValid()
		&& Job.AtlasResources->HeightAtlas.IsValid())
	{
		HeightAtlas = GraphBuilder.RegisterExternalTexture(Job.AtlasResources->HeightAtlas);
	}
	if (ScratchA == nullptr || ScratchB == nullptr || (HeightTile == nullptr && HeightAtlas == nullptr))
	{
		if (Job.bLogWeightVerbose)
		{
			UE_LOGF(LogArcTerrain, Log,
				"[WeightNormalize] cell=(%d,%d) arrays=0 snapshotted=0 normalized=0 presentmask=0x00 "
				"weightres=%d | SKIPPED: the composite itself bailed (scratchA=%ls scratchB=%ls "
				"heighttile=%ls heightatlas=%ls)",
				Job.CellKey.X, Job.CellKey.Y, Job.WeightTileResolution,
				ScratchA != nullptr ? TEXT("ok") : TEXT("null"),
				ScratchB != nullptr ? TEXT("ok") : TEXT("null"),
				HeightTile != nullptr ? TEXT("ok") : TEXT("null"),
				HeightAtlas != nullptr ? TEXT("ok") : TEXT("null"));
		}
		GraphBuilder.Execute();
		return;
	}

	TArray<FRDGTextureRef, TInlineAllocator<4>> WeightRTs;
	for (FTextureRenderTargetResource* WeightResource : Job.WeightRTResources)
	{
		WeightRTs.Add(RegisterTileTexture(GraphBuilder, WeightResource, TEXT("ArcTerrainWeightTile")));
	}

	// Clear first: the height scratch chain to the authored base level (an uncovered cell composites to
	// ground, not to 0), every weight tile to 0.
	AddClearRenderTargetPass(GraphBuilder, ScratchA, FLinearColor(Job.BaseHeightCm, 0.0f, 0.0f, 0.0f));
	for (FRDGTextureRef WeightRT : WeightRTs)
	{
		if (WeightRT != nullptr)
		{
			AddClearRenderTargetPass(GraphBuilder, WeightRT, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIShaderPlatform);
	// Decoupled viewports (weightmap-decouple Stage 1 Step 1): the height scratch chain renders at
	// TileResolution; the weight tiles render at WeightTileResolution (their own source-driven density).
	const FIntRect HeightViewport(0, 0, Job.TileResolution, Job.TileResolution);
	const FIntRect WeightViewport(0, 0, Job.WeightTileResolution, Job.WeightTileResolution);

	// Blend split (see plan Step 4): Additive + AlphaBlend + weights = HARDWARE blend states;
	// Min/Max = ping-pong (non-linear in dst — not expressible as a blend state).
	FRHIBlendState* HeightAdditiveBlend =
		TStaticBlendState<CW_RED, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_Zero>::GetRHI();
	FRHIBlendState* HeightAlphaBlend =
		TStaticBlendState<CW_RED, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_Zero>::GetRHI();
	FRHIBlendState* WeightMultiplyBlend =
		TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_InverseSourceColor, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
	FRHIBlendState* WeightAddBlend =
		TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();

	FRDGTextureRef HeightCurrent = ScratchA;
	FRDGTextureRef HeightOther = ScratchB;

	for (const FStampPass& Pass : Job.Passes)
	{
		// The mask follows the value source: a resolved mask whose upload failed would composite the
		// stamp unmasked, at full strength, on a tile still marked persistable.
		if (!Pass.Source.IsValid() || !Pass.Source->TextureRHI.IsValid()
			|| (Pass.MaskSource.IsValid() && !Pass.MaskSource->TextureRHI.IsValid()))
		{
			continue; // upload failed — skip, never bind a null source
		}

		if (Pass.Target == FStampPass::ETarget::Height)
		{
			const bool bMinMax =
				Pass.BlendMode == EArcTerrainBlendMode::Min || Pass.BlendMode == EArcTerrainBlendMode::Max;

			FArcTerrainCompositeHeightPS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FArcTerrainCompositeHeightPS::FMinMaxBlendDim>(bMinMax);
			TShaderMapRef<FArcTerrainCompositeHeightPS> PixelShader(ShaderMap, PermutationVector);

			FArcTerrainCompositeHeightPS::FParameters* Parameters =
				GraphBuilder.AllocParameters<FArcTerrainCompositeHeightPS::FParameters>();
			FillStampParameters(Parameters->Stamp, Pass, Job);
			Parameters->InMinMaxMode = (Pass.BlendMode == EArcTerrainBlendMode::Max) ? 1 : 0;
			Parameters->InMinMaxSoftness = Pass.MinMaxSoftness;

			if (bMinMax)
			{
				// Ping-pong: read the previous tile state, write the full blended tile to the other
				// scratch, then cycle (pattern: Landscape CycleBlendRenderTargets).
				Parameters->InHeightOutputMode = 0;
				Parameters->InExistingHeight = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(HeightCurrent));
				Parameters->RenderTargets[0] = FRenderTargetBinding(HeightOther, ERenderTargetLoadAction::ENoAction);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap,
					RDG_EVENT_NAME("ArcTerrainHeightStamp (MinMax)"), PixelShader, Parameters, HeightViewport);
				Swap(HeightCurrent, HeightOther);
			}
			else
			{
				const bool bAdditive = Pass.BlendMode == EArcTerrainBlendMode::Additive;
				Parameters->InHeightOutputMode = bAdditive ? 0 : 1;
				Parameters->InExistingHeight = nullptr;
				Parameters->RenderTargets[0] = FRenderTargetBinding(HeightCurrent, ERenderTargetLoadAction::ELoad);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap,
					RDG_EVENT_NAME("ArcTerrainHeightStamp"), PixelShader, Parameters, HeightViewport,
					bAdditive ? HeightAdditiveBlend : HeightAlphaBlend);
			}
		}
		else // Weight
		{
			if (!WeightRTs.IsValidIndex(Pass.WeightRTIndex) || WeightRTs[Pass.WeightRTIndex] == nullptr
				|| Pass.WeightChannel < 0 || Pass.WeightChannel > 3)
			{
				continue;
			}
			FRDGTextureRef WeightRT = WeightRTs[Pass.WeightRTIndex];

			FVector4f TargetChannelMask = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
			TargetChannelMask[Pass.WeightChannel] = 1.0f;

			TShaderMapRef<FArcTerrainCompositeWeightPS> PixelShader(ShaderMap);

			// Two passes implement lerp(Existing, saturate(Value), Mask) on ONE channel of the
			// RGBA8 tile: multiply (Dst *= 1-Mask) then premultiplied add (Dst += saturate(V)*Mask).
			// Value saturates BEFORE the lerp — Step 9 CPU parity must match this exact formula.
			for (uint32 WeightPassMode = 0; WeightPassMode < 2; ++WeightPassMode)
			{
				FArcTerrainCompositeWeightPS::FParameters* Parameters =
					GraphBuilder.AllocParameters<FArcTerrainCompositeWeightPS::FParameters>();
				FillStampParameters(Parameters->Stamp, Pass, Job);
				Parameters->InWeightPassMode = WeightPassMode;
				Parameters->InTargetChannelMask = TargetChannelMask;
				Parameters->RenderTargets[0] = FRenderTargetBinding(WeightRT, ERenderTargetLoadAction::ELoad);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap,
					RDG_EVENT_NAME("ArcTerrainWeightStamp (pass %u)", WeightPassMode), PixelShader,
					Parameters, WeightViewport,
					(WeightPassMode == 0) ? WeightMultiplyBlend : WeightAddBlend);
			}
		}
	}

	bool bAnyWeightArrayPresent = false;
	for (FRDGTextureRef WeightRT : WeightRTs)
	{
		bAnyWeightArrayPresent = bAnyWeightArrayPresent || (WeightRT != nullptr);
	}
	const int32 BaseArrayIndex = Job.BaseWeightArrayIndex;
	const bool bBaseWeightArrayPresent =
		WeightRTs.IsValidIndex(BaseArrayIndex) && WeightRTs[BaseArrayIndex] != nullptr;

	FString NormalizeOutcome;
	int32 NumWeightArraysSnapshotted = 0;
	int32 NumWeightArraysNormalized = 0;
	uint32 NormalizePresentMask = 0;

	if (WeightRTs.Num() > FArcTerrainNormalizeWeightsPS::MaxWeightArrays)
	{
		NormalizeOutcome = FString::Printf(
			TEXT("REFUSED: %d arrays exceed the %d the normalize shader binds"),
			WeightRTs.Num(), FArcTerrainNormalizeWeightsPS::MaxWeightArrays);
		if (ShouldReportOnce(GReportedWeightArrayOverflow, WeightRTs.Num()))
		{
			UE_LOGF(LogArcTerrain, Error,
				"ArcTerrain composite: %d weight arrays exceed the %d the normalize shader can bind — "
				"weights left unnormalized.",
				WeightRTs.Num(), FArcTerrainNormalizeWeightsPS::MaxWeightArrays);
		}
	}
	else if (bAnyWeightArrayPresent && !bBaseWeightArrayPresent)
	{
		NormalizeOutcome = FString::Printf(
			TEXT("REFUSED: base weight array %d is not bound to this cell (of %d)"),
			BaseArrayIndex, WeightRTs.Num());
		if (ShouldReportOnce(GReportedMissingBaseWeightArray, WeightRTs.Num()))
		{
			UE_LOGF(LogArcTerrain, Error,
				"ArcTerrain composite: base weight array %d is not bound of %d — global channel %d carries "
				"the base remainder that closes the per-texel sum to 1, so the normalize is refused and "
				"weights are left unnormalized. An array of -1 means the channel set cannot address the "
				"base channel.",
				BaseArrayIndex, WeightRTs.Num(), ArcTerrain::GBaseChannelIndex);
		}
	}
	else if (bBaseWeightArrayPresent)
	{
		TArray<FRDGTextureRef, TInlineAllocator<FArcTerrainNormalizeWeightsPS::MaxWeightArrays>> WeightScratches;
		WeightScratches.Init(nullptr, FArcTerrainNormalizeWeightsPS::MaxWeightArrays);
		uint32 PresentMask = 0;
		for (int32 ArrayIndex = 0; ArrayIndex < WeightRTs.Num(); ++ArrayIndex)
		{
			if (WeightRTs[ArrayIndex] == nullptr)
			{
				continue;
			}
			// Snapshot before any array is overwritten, so later passes do not sum normalized siblings.
			FRDGTextureRef Scratch = GraphBuilder.CreateTexture(
				WeightRTs[ArrayIndex]->Desc, TEXT("ArcTerrainWeightNormalizeScratch"));
			AddCopyTexturePass(GraphBuilder, WeightRTs[ArrayIndex], Scratch);
			WeightScratches[ArrayIndex] = Scratch;
			PresentMask |= 1u << static_cast<uint32>(ArrayIndex);
			++NumWeightArraysSnapshotted;
		}
		NormalizePresentMask = PresentMask;

		FRDGTextureRef BlackDummy = GSystemTextures.GetBlackDummy(GraphBuilder);
		TShaderMapRef<FArcTerrainNormalizeWeightsPS> PixelShader(ShaderMap);
		for (int32 ArrayIndex = 0; ArrayIndex < WeightRTs.Num(); ++ArrayIndex)
		{
			if (WeightRTs[ArrayIndex] == nullptr)
			{
				continue;
			}
			FArcTerrainNormalizeWeightsPS::FParameters* Parameters =
				GraphBuilder.AllocParameters<FArcTerrainNormalizeWeightsPS::FParameters>();
			Parameters->InArrayIndex = static_cast<uint32>(ArrayIndex);
			Parameters->InNumArrays = static_cast<uint32>(WeightRTs.Num());
			Parameters->InPresentMask = PresentMask;
			for (int32 SlotIndex = 0; SlotIndex < FArcTerrainNormalizeWeightsPS::MaxWeightArrays; ++SlotIndex)
			{
				Parameters->InScratch[SlotIndex] =
					(WeightScratches[SlotIndex] != nullptr) ? WeightScratches[SlotIndex] : BlackDummy;
			}
			Parameters->RenderTargets[0] =
				FRenderTargetBinding(WeightRTs[ArrayIndex], ERenderTargetLoadAction::ENoAction);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap,
				RDG_EVENT_NAME("ArcTerrainNormalizeWeights (array %d)", ArrayIndex), PixelShader,
				Parameters, WeightViewport);
			++NumWeightArraysNormalized;
		}
		NormalizeOutcome = TEXT("ok");
	}
	else
	{
		NormalizeOutcome = TEXT("SKIPPED: no weight array is bound to this cell");
	}

	if (Job.bLogWeightVerbose)
	{
		UE_LOGF(LogArcTerrain, Log,
			"[WeightNormalize] cell=(%d,%d) arrays=%d snapshotted=%d normalized=%d presentmask=0x%02x "
			"weightres=%d | %ls",
			Job.CellKey.X, Job.CellKey.Y, WeightRTs.Num(), NumWeightArraysSnapshotted,
			NumWeightArraysNormalized, NormalizePresentMask, Job.WeightTileResolution, *NormalizeOutcome);
	}

	// Final: the composited height lives in the current scratch — publish it into the atlas slice, and
	// into the staging tile when the caller asked for a readable copy.
	if (HeightAtlas != nullptr)
	{
		const FIntPoint DestOrigin = Job.AtlasHeightDestOrigin;
		// D3D12 reports an out-of-bounds copy box only at command-list Close, as a bare E_INVALIDARG with
		// no validation-layer message, so a mismatched slice/sub-rect has to be refused here.
		if (ensureMsgf(Job.AtlasHeightSlice < static_cast<int32>(HeightAtlas->Desc.ArraySize)
				&& DestOrigin.X >= 0 && DestOrigin.Y >= 0
				&& DestOrigin.X + Job.TileResolution <= HeightAtlas->Desc.Extent.X
				&& DestOrigin.Y + Job.TileResolution <= HeightAtlas->Desc.Extent.Y,
			TEXT("ArcTerrain composite: slice %d rect (%d,%d)+%d^2 outside the %dx%d x%d height atlas — refused"),
			Job.AtlasHeightSlice, DestOrigin.X, DestOrigin.Y, Job.TileResolution,
			HeightAtlas->Desc.Extent.X, HeightAtlas->Desc.Extent.Y, HeightAtlas->Desc.ArraySize))
		{
			FRHICopyTextureInfo CopyInfo;
			CopyInfo.Size = FIntVector(Job.TileResolution, Job.TileResolution, 1);
			CopyInfo.DestPosition = FIntVector(DestOrigin.X, DestOrigin.Y, 0);
			CopyInfo.DestSliceIndex = static_cast<uint32>(Job.AtlasHeightSlice);
			CopyInfo.NumSlices = 1;
			AddCopyTexturePass(GraphBuilder, HeightCurrent, HeightAtlas, CopyInfo);
			ArcTerrainTileAtlasRender::InvalidateHeightCellBounds(*Job.AtlasResources,
				Job.AtlasHeightSlice, Job.CellKey);
			ArcTerrainTileAtlasRender::RecomputeHeightSliceBounds(*Job.AtlasResources);
		}
		GraphBuilder.SetTextureAccessFinal(HeightAtlas, ERHIAccess::SRVMask);
	}

	// ONE policy for every texture these graphs write, stated rather than transitioned: SRVMask is
	// already FRDGViewableResource::DefaultEpilogueAccess, and it is what makes the atlas legal to bind
	// as a raw SHADER_PARAMETER_TEXTURE outside RDG (the vertex factory) and the tiles legal to sample
	// or read back. FArcTerrainTileAtlas::ExecuteFill sets the same on its side.
	if (HeightTile != nullptr)
	{
		AddCopyTexturePass(GraphBuilder, HeightCurrent, HeightTile);
		GraphBuilder.SetTextureAccessFinal(HeightTile, ERHIAccess::SRVMask);
	}
	for (FRDGTextureRef WeightRT : WeightRTs)
	{
		if (WeightRT != nullptr)
		{
			GraphBuilder.SetTextureAccessFinal(WeightRT, ERHIAccess::SRVMask);
		}
	}

	GraphBuilder.Execute();
}
