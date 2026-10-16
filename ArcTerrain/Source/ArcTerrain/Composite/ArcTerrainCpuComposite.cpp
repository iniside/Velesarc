// Copyright Lukasz Baran. All Rights Reserved.

#include "Composite/ArcTerrainCpuComposite.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainModule.h"
#include "ArcTerrainSubsystem.h"
#include "Stamps/ArcTerrainStampComponent.h"

namespace ArcTerrainCpuCompositePrivate
{
	using namespace ArcTerrainCpuComposite;

	// Bilinear tap with AM_Clamp addressing — the hardware sampler the composite binds
	// (TStaticSamplerState<SF_Bilinear, AM_Clamp, ...>) over the single-channel float raster.
	float SampleBilinearClamped(const FArcTerrainSourceCpuFloats& Raster, float U, float V)
	{
		const int32 DimX = Raster.Dims.X;
		const int32 DimY = Raster.Dims.Y;
		const float PixelX = U * static_cast<float>(DimX) - 0.5f;
		const float PixelY = V * static_cast<float>(DimY) - 0.5f;
		const float FloorX = FMath::FloorToFloat(PixelX);
		const float FloorY = FMath::FloorToFloat(PixelY);
		const float FracX = PixelX - FloorX;
		const float FracY = PixelY - FloorY;

		const int32 X0 = FMath::Clamp(static_cast<int32>(FloorX), 0, DimX - 1);
		const int32 X1 = FMath::Clamp(static_cast<int32>(FloorX) + 1, 0, DimX - 1);
		const int32 Y0 = FMath::Clamp(static_cast<int32>(FloorY), 0, DimY - 1);
		const int32 Y1 = FMath::Clamp(static_cast<int32>(FloorY) + 1, 0, DimY - 1);

		const float* Values = Raster.Values.GetData();
		const float V00 = Values[Y0 * DimX + X0];
		const float V10 = Values[Y0 * DimX + X1];
		const float V01 = Values[Y1 * DimX + X0];
		const float V11 = Values[Y1 * DimX + X1];

		const float Top = V00 + (V10 - V00) * FracX;
		const float Bottom = V01 + (V11 - V01) * FracX;
		return Top + (Bottom - Top) * FracY;
	}

	// Port of the engine's 5-tap Catmull-Rom approximation (Texture2DSampleBicubic /
	// GetBicubic2DCatmullRomSamples, Engine/Shaders/Private/TextureSampling.ush) — the reconstruction
	// the HEIGHT composite path uses. The 5 taps are BILINEAR, which is why the kernel reaches only
	// +-2 source texels and why the sampler must stay bilinear.
	float SampleBicubicCatmullRom(const FArcTerrainSourceCpuFloats& Raster, float U, float V)
	{
		const float SizeX = static_cast<float>(FMath::Max(Raster.Dims.X, 1));
		const float SizeY = static_cast<float>(FMath::Max(Raster.Dims.Y, 1));
		const float InvSizeX = 1.0f / SizeX;
		const float InvSizeY = 1.0f / SizeY;

		const float PixelCoordX = U * SizeX;
		const float PixelCoordY = V * SizeY;

		const float IuvX = FMath::FloorToFloat(PixelCoordX - 0.5f) + 1.0f;
		const float IuvY = FMath::FloorToFloat(PixelCoordY - 0.5f) + 1.0f;
		const float Fx = PixelCoordX - IuvX;
		const float Fy = PixelCoordY - IuvY;
		const float F2x = Fx * Fx;
		const float F2y = Fy * Fy;

		float UvX = (1.25f - F2x) * Fx + 0.5f;
		float UvY = (1.25f - F2y) * Fy + 0.5f;
		const float T0x = (0.25f * F2x - 0.0625f) * (1.125f - 0.5f * F2y);
		const float T0y = (0.25f * F2y - 0.0625f) * (1.125f - 0.5f * F2x);
		const float W0x = T0x - (Fx + Fx) * T0x;
		const float W3x = T0x + (Fx + Fx) * T0x;
		const float W0y = T0y - (Fy + Fy) * T0y;
		const float W3y = T0y + (Fy + Fy) * T0y;

		const float CuvX = (IuvX - 0.5f) * InvSizeX;
		const float CuvY = (IuvY - 0.5f) * InvSizeY;
		UvX *= InvSizeX;
		UvY *= InvSizeY;

		const float Weight2 = 1.0f - W0x - W3x - W0y - W3y;
		float Result = SampleBilinearClamped(Raster, CuvX - InvSizeX, CuvY + UvY) * W0x;
		Result += SampleBilinearClamped(Raster, CuvX + 2.0f * InvSizeX, CuvY + UvY) * W3x;
		Result += SampleBilinearClamped(Raster, CuvX + UvX, CuvY + UvY) * Weight2;
		Result += SampleBilinearClamped(Raster, CuvX + UvX, CuvY - InvSizeY) * W0y;
		Result += SampleBilinearClamped(Raster, CuvX + UvX, CuvY + 2.0f * InvSizeY) * W3y;
		return Result;
	}

	// Ports of ArcSmoothMin/ArcSmoothMax (ArcTerrainComposite.usf), themselves ports of
	// FMathd::SmoothMin/SmoothMax. A is the existing surface, B the stamp's target.
	float SmoothMin(float A, float B, float BlendExtent)
	{
		if (BlendExtent == 0.0f)
		{
			return FMath::Min(A, B);
		}
		const float H = (BlendExtent + B - A) * 0.5f;
		return FMath::Min(B - H * FMath::Clamp(H / BlendExtent, 0.0f, 1.0f), A);
	}

	float SmoothMax(float A, float B, float BlendExtent)
	{
		if (BlendExtent == 0.0f)
		{
			return FMath::Max(A, B);
		}
		const float H = (BlendExtent + A - B) * 0.5f;
		return FMath::Max(B + H * FMath::Clamp(H / BlendExtent, 0.0f, 1.0f), A);
	}

	// UNORM8 destination write: the RGBA8 weight tile requantises between blend passes. The graphics APIs
	// leave the tie direction unspecified (D3D bounds the conversion by a tolerance, Vulkan calls it
	// implementation-defined); the only tie this composite can reach is 0.5, where half-to-even and
	// half-away-from-zero agree.
	uint8 QuantizeUnorm8(float Value)
	{
		const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
		return static_cast<uint8>(FMath::RoundHalfToEven(Clamped * 255.0f));
	}

	// The per-texel half of ComputeStamp (ArcTerrainComposite.usf), shared by both targets: patch
	// transform, footprint test, falloff masks, source reconstruction, encoding decode. OffsetX/OffsetY
	// are the shader's PatchOffset (RelTexel * InTexelToLocal) and the caller applies the target's own
	// value scale/bias to OutValue. False = the shader's `return false` / `clip(-1)`: leave the
	// destination alone.
	bool SampleStamp(const FStampPlacement& Op, double OffsetX, double OffsetY, float HalfX, float HalfY,
		bool bBicubic, float& OutValue, float& OutMask)
	{
		// Row-vector mul against the linear block; the translation is already folded into the patch
		// origin the caller subtracted.
		const float PatchX = static_cast<float>(OffsetX * Op.LinearM00 + OffsetY * Op.LinearM10);
		const float PatchY = static_cast<float>(OffsetX * Op.LinearM01 + OffsetY * Op.LinearM11);
		if (FMath::Abs(PatchX) > HalfX || FMath::Abs(PatchY) > HalfY)
		{
			return false;
		}

		float Mask = ArcTerrainStampMask::Evaluate(Op.Mask, PatchX, PatchY, HalfX, HalfY);
		if (Mask <= 0.0f)
		{
			return false;
		}

		const float FootprintU = PatchX / Op.CoverageXY.X + 0.5f;
		const float FootprintV = PatchY / Op.CoverageXY.Y + 0.5f;
		const float SampleU = FootprintU * Op.UVScale.X + Op.UVBias.X;
		const float SampleV = FootprintV * Op.UVScale.Y + Op.UVBias.Y;

		if (Op.MaskSource.IsValid())
		{
			// saturate's order, not FMath::Clamp: Clamp(NaN) yields 1 while HLSL saturate yields 0.
			Mask *= FMath::Min(FMath::Max(SampleBilinearClamped(*Op.MaskSource, SampleU, SampleV), 0.0f), 1.0f);
			if (Mask <= 0.0f)
			{
				return false;
			}
		}

		const float Texel = bBicubic ? SampleBicubicCatmullRom(*Op.Source, SampleU, SampleV)
			: SampleBilinearClamped(*Op.Source, SampleU, SampleV);

		OutValue = (Texel - Op.ZeroInEncoding) * Op.EncodingScale;
		OutMask = Mask;
		return true;
	}

	using ArcTerrainSubsystemPrivate::ComputeSampleMap;

	// FillCommonPassFields (ArcTerrainSubsystem.cpp): the linear block plus the patch origin
	// P0 = -T * L^-1, solved in double from the same float matrix entries the shader applies.
	void FillCommonOpFields(FStampPlacement& Op, const FArcStampSourceDesc& Desc,
		const FMatrix44f& LocalToPatch, const FVector2f& CoverageXY)
	{
		Op.LinearM00 = static_cast<double>(LocalToPatch.M[0][0]);
		Op.LinearM01 = static_cast<double>(LocalToPatch.M[0][1]);
		Op.LinearM10 = static_cast<double>(LocalToPatch.M[1][0]);
		Op.LinearM11 = static_cast<double>(LocalToPatch.M[1][1]);

		const double Tx = static_cast<double>(LocalToPatch.M[3][0]);
		const double Ty = static_cast<double>(LocalToPatch.M[3][1]);
		const double Det = Op.LinearM00 * Op.LinearM11 - Op.LinearM01 * Op.LinearM10;
		if (FMath::Abs(Det) > 1.0e-30)
		{
			Op.PatchOriginTerrainLocal = FVector2D(
				(-Op.LinearM11 * Tx + Op.LinearM10 * Ty) / Det,
				(Op.LinearM01 * Tx - Op.LinearM00 * Ty) / Det);
		}
		else
		{
			Op.PatchOriginTerrainLocal = FVector2D::ZeroVector;
		}

		Op.CoverageXY = CoverageXY;
		Op.Mask.EdgeFalloffBand = Desc.EdgeFalloffBand;
		Op.Mask.EdgeSharpness = Desc.EdgeSharpness;
		Op.Mask.CornerRadius = Desc.CornerRadius;
		Op.Mask.bCircleMask = Desc.bCircleMask;
		Op.Mask.CircleCenterOffset = FVector2f(Desc.CircleCenterOffset);
		Op.Mask.CircleRadius = Desc.CircleRadius;
		Op.Mask.CircleFalloffBand = Desc.CircleFalloffBand;
	}

	FArcTerrainSourceCpuFloatsPtr ResolveCpuSource(FArcTerrainSourceCache& Cache,
		EArcStampSourceKind Kind, const FGuid& ContentId, UTexture2D* Texture,
		const TFunction<bool(TArray<float>&, FIntPoint&)>& Provider, EArcTerrainSourceChannel Channel)
	{
		switch (Kind)
		{
		case EArcStampSourceKind::Texture:
#if WITH_EDITORONLY_DATA
			return (Texture != nullptr)
				? Cache.GetOrCreateCpuFloatsFromTexture(*Texture, Channel, ContentId) : nullptr;
#else
			// No FTextureSource outside the editor: the caller reads this as an incomplete evaluation and
			// falls to the baked section, which is the cooked height authority.
			return nullptr;
#endif
		case EArcStampSourceKind::BakedField:
			return Cache.GetOrCreateCpuFloatsFromProvider(ContentId, Provider);
		default:
			return nullptr;
		}
	}

	bool IsUsableRaster(const FArcTerrainSourceCpuFloatsPtr& Source)
	{
		return Source.IsValid() && Source->Dims.X > 0 && Source->Dims.Y > 0
			&& Source->Values.Num() == Source->Dims.X * Source->Dims.Y;
	}
}

namespace ArcTerrainCpuComposite
{
	FCellRaster MakeCollisionRaster(FArcTerrainCollisionKey Key, int32 Quads, double PatchSize)
	{
		const int32 ClampedQuads = FMath::Max(Quads, 1);

		FCellRaster Raster;
		Raster.TexelOrigin = FIntPoint(
			static_cast<int32>(static_cast<int64>(Key.X) * ClampedQuads),
			static_cast<int32>(static_cast<int64>(Key.Y) * ClampedQuads));
		Raster.TexelToLocal = static_cast<float>(PatchSize) / static_cast<float>(ClampedQuads);
		Raster.SamplesPerSide = ClampedQuads + 1;
		return Raster;
	}

	FCellRaster MakeSectionRaster(FArcTerrainCellKey Key, int32 QuadsPerCell, double CellSize)
	{
		const int32 ClampedQuads = FMath::Max(QuadsPerCell, 1);

		FCellRaster Raster;
		// Widened before narrowing, as ComputeGlobalTexelOrigin does: the int32 product wraps past
		// ~+-8.4M cells and would reopen the shared-edge seam.
		Raster.TexelOrigin = FIntPoint(
			static_cast<int32>(static_cast<int64>(Key.X) * ClampedQuads - 1),
			static_cast<int32>(static_cast<int64>(Key.Y) * ClampedQuads - 1));
		Raster.TexelToLocal = static_cast<float>(CellSize) / static_cast<float>(ClampedQuads);
		Raster.SamplesPerSide = ClampedQuads + 3;
		return Raster;
	}

	FCellRaster MakeWeightRaster(FArcTerrainCellKey Key, int32 WeightQuads, double CellSize)
	{
		// Deliberately the section raster at the weight density: the shader builds both from
		// ComputeGlobalTexelOrigin(Key, Quads) and CellSize / (TileResolution - 3), so the two grids
		// differ only in Quads — a second formula here could only drift from that one.
		return MakeSectionRaster(Key, WeightQuads, CellSize);
	}

	void GatherCellHeightOps(FArcTerrainSourceCache& SourceCache, const FArcTerrainConfig& Config,
		const FTransform& TerrainTransform, const FCellRaster& Raster, float BaseHeightCm,
		TConstArrayView<UArcTerrainStampComponent*> SortedStamps, FCellHeightEval& OutEval)
	{
		using namespace ArcTerrainCpuCompositePrivate;

		OutEval.Ops.Reset();
		OutEval.BaseHeightCm = BaseHeightCm;
		OutEval.Raster = Raster;
		OutEval.bComplete = true;

		for (UArcTerrainStampComponent* Stamp : SortedStamps)
		{
			if (Stamp == nullptr)
			{
				continue;
			}

			FArcStampSourceDesc Desc;
			if (!Stamp->GetSourceDescriptor(Desc))
			{
#if !WITH_EDITOR
				// Without editor data a false descriptor is indistinguishable from a raster that did
				// not survive cook (UArcTerrainPatchStamp / UArcTerrainTexturePatchModifier compile
				// their bodies out), so the evaluation cannot claim to describe this cell.
				OutEval.bComplete = false;
#endif
				continue;
			}

			const FTransform StampToTerrain =
				Stamp->GetComponentTransform().GetRelativeTransform(TerrainTransform);
			const FMatrix44f LocalToPatch = FMatrix44f(StampToTerrain.ToInverseMatrixWithScale());
			const FVector2f CoverageXY = FVector2f(Stamp->CoverageXY);
			const float RelScaleZ = static_cast<float>(StampToTerrain.GetScale3D().Z);
			const float StampLocalZ = static_cast<float>(StampToTerrain.GetTranslation().Z);
			const EArcTerrainBlendMode BlendMode = Stamp->BlendMode;

			if (Desc.Kind != EArcStampSourceKind::None)
			{
				const FArcTerrainSourceCpuFloatsPtr Source = ResolveCpuSource(SourceCache, Desc.Kind,
					Desc.ContentId, Desc.Texture, Desc.BakedProvider, Desc.SourceChannel);
				if (!IsUsableRaster(Source))
				{
					OutEval.bComplete = false;
				}
				else
				{
					FArcTerrainSourceCpuFloatsPtr MaskSource;
					if (Desc.MaskKind != EArcStampSourceKind::None)
					{
						MaskSource = ResolveCpuSource(SourceCache, Desc.MaskKind, Desc.MaskContentId,
							Desc.MaskTexture, Desc.MaskBakedProvider, Desc.MaskSourceChannel);
						if (!IsUsableRaster(MaskSource) || MaskSource->Dims != Source->Dims)
						{
							const FIntPoint MaskDims = MaskSource.IsValid() ? MaskSource->Dims : FIntPoint::ZeroValue;
							UE_LOGF(LogArcTerrain, Warning,
								"[HeightComposite] stamp '%ls' mask DROPPED: mask dims %dx%d do not match "
								"value dims %dx%d (0x0 = mask source failed to resolve). "
								"Cell evaluation is not persistable.",
								*Stamp->GetName(), MaskDims.X, MaskDims.Y, Source->Dims.X, Source->Dims.Y);
							MaskSource.Reset();
							OutEval.bComplete = false;
						}
					}

					FStampHeightOp& Op = OutEval.Ops.AddDefaulted_GetRef();
					FillCommonOpFields(Op.Placement, Desc, LocalToPatch, CoverageXY);
					Op.Placement.Source = Source;
					Op.Placement.MaskSource = MaskSource;
					Op.BlendMode = BlendMode;
					// Softness is authored in stamp-local Z cm; heights are terrain-local.
					Op.MinMaxSoftness = Stamp->MinMaxSoftness * RelScaleZ;
					Op.Placement.EncodingScale = Desc.EncodingScale;
					// Height only: the carve ops below keep their authored zero point.
					Op.Placement.ZeroInEncoding = Desc.bDeriveZeroFromBorder ? Source->BorderMean : Desc.ZeroInEncoding;
					Op.ValueScale = RelScaleZ;
					Op.ValueBias = (BlendMode == EArcTerrainBlendMode::Additive) ? 0.0f : StampLocalZ;
					ComputeSampleMap(Source->Dims, Op.Placement.UVScale, Op.Placement.UVBias);
				}
			}

			for (const TPair<FName, FArcStampLayerDesc>& LayerPair : Desc.Layers)
			{
				const FArcStampLayerDesc& Layer = LayerPair.Value;
				if (!Layer.bContributeToHeight || Layer.HeightAmplitude == 0.0f)
				{
					continue;
				}
				// The composite drops a layer whose channel does not resolve against the terrain's
				// channel set ENTIRELY — no weight pass and no carve. Same gate, same order, so this
				// walk and ComputeCellCollisionKey agree on which carves exist.
				const int32 ChannelIndex =
					Config.ChannelSet ? Config.ChannelSet->FindChannelIndex(LayerPair.Key) : INDEX_NONE;
				if (ChannelIndex == INDEX_NONE)
				{
					continue;
				}

				const FArcTerrainSourceCpuFloatsPtr Source = ResolveCpuSource(SourceCache, Layer.Kind,
					Layer.ContentId, Layer.Texture, Layer.BakedProvider, Layer.SourceChannel);
				if (!IsUsableRaster(Source))
				{
					OutEval.bComplete = false;
					continue;
				}

				FStampHeightOp& Op = OutEval.Ops.AddDefaulted_GetRef();
				FillCommonOpFields(Op.Placement, Desc, LocalToPatch, CoverageXY);
				Op.Placement.Source = Source;
				Op.BlendMode = EArcTerrainBlendMode::Additive;
				Op.MinMaxSoftness = 0.0f;
				Op.Placement.EncodingScale = Layer.EncodingScale;
				Op.Placement.ZeroInEncoding = Layer.ZeroInEncoding;
				Op.ValueScale = Layer.HeightAmplitude * RelScaleZ;
				Op.ValueBias = 0.0f;
				ComputeSampleMap(Source->Dims, Op.Placement.UVScale, Op.Placement.UVBias);
			}
		}
	}

	void EvaluateCellHeights(const FCellHeightEval& Eval, TArray<float>& OutHeights)
	{
		using namespace ArcTerrainCpuCompositePrivate;

		const int32 Resolution = Eval.Raster.SamplesPerSide;
		if (Resolution < 2)
		{
			OutHeights.Reset();
			return;
		}

		checkSlow(Eval.Raster.TexelToLocal > 0.0f);

		OutHeights.Init(Eval.BaseHeightCm, Resolution * Resolution);

		const double TexelToLocal = static_cast<double>(Eval.Raster.TexelToLocal);
		for (const FStampHeightOp& Op : Eval.Ops)
		{
			if (!Op.Placement.Source.IsValid())
			{
				continue;
			}

			const float HalfX = Op.Placement.CoverageXY.X * 0.5f;
			const float HalfY = Op.Placement.CoverageXY.Y * 0.5f;
			// The stamp's patch origin in texel-index units at this density, divided by the SAME float
			// spacing the shader multiplies the rebased index by (FillStampParameters).
			const double OriginTexelX = Op.Placement.PatchOriginTerrainLocal.X / TexelToLocal;
			const double OriginTexelY = Op.Placement.PatchOriginTerrainLocal.Y / TexelToLocal;

			for (int32 Row = 0; Row < Resolution; ++Row)
			{
				const double RelTexelY =
					static_cast<double>(Eval.Raster.TexelOrigin.Y + Row) - OriginTexelY;
				const double OffsetY = RelTexelY * TexelToLocal;
				float* RowHeights = OutHeights.GetData() + Row * Resolution;

				for (int32 Col = 0; Col < Resolution; ++Col)
				{
					const double RelTexelX =
						static_cast<double>(Eval.Raster.TexelOrigin.X + Col) - OriginTexelX;
					const double OffsetX = RelTexelX * TexelToLocal;

					float RawValue = 0.0f;
					float Mask = 0.0f;
					if (!SampleStamp(Op.Placement, OffsetX, OffsetY, HalfX, HalfY, /*bBicubic*/ true,
						RawValue, Mask))
					{
						continue;
					}
					const float Value = RawValue * Op.ValueScale + Op.ValueBias;

					float& Height = RowHeights[Col];
					switch (Op.BlendMode)
					{
					case EArcTerrainBlendMode::Additive:
						Height += Value * Mask;
						break;
					case EArcTerrainBlendMode::AlphaBlend:
						Height = Value * Mask + Height * (1.0f - Mask);
						break;
					case EArcTerrainBlendMode::Min:
						Height = FMath::Lerp(Height, SmoothMin(Height, Value, Op.MinMaxSoftness), Mask);
						break;
					case EArcTerrainBlendMode::Max:
						Height = FMath::Lerp(Height, SmoothMax(Height, Value, Op.MinMaxSoftness), Mask);
						break;
					}
				}
			}
		}
	}

	void GatherCellWeightOps(FArcTerrainSourceCache& SourceCache, const FArcTerrainConfig& Config,
		const FTransform& TerrainTransform, const FCellRaster& Raster,
		TConstArrayView<int32> PlaneChannels,
		TConstArrayView<UArcTerrainStampComponent*> SortedStamps, FCellWeightEval& OutEval)
	{
		using namespace ArcTerrainCpuCompositePrivate;

		OutEval.Ops.Reset();
		OutEval.Raster = Raster;
		OutEval.PlaneChannels = PlaneChannels;
		OutEval.bComplete = true;

		for (UArcTerrainStampComponent* Stamp : SortedStamps)
		{
			if (Stamp == nullptr)
			{
				continue;
			}

			FArcStampSourceDesc Desc;
			if (!Stamp->GetSourceDescriptor(Desc))
			{
#if !WITH_EDITOR
				// As in GatherCellHeightOps: without editor data a false descriptor cannot be told apart
				// from a raster that did not survive cook.
				OutEval.bComplete = false;
#endif
				continue;
			}

			const FTransform StampToTerrain =
				Stamp->GetComponentTransform().GetRelativeTransform(TerrainTransform);
			const FMatrix44f LocalToPatch = FMatrix44f(StampToTerrain.ToInverseMatrixWithScale());
			const FVector2f CoverageXY = FVector2f(Stamp->CoverageXY);

			// Resolved once per stamp: height and weight ops both may need it (carve does not).
			FArcTerrainSourceCpuFloatsPtr StampMaskSource;
			if (Desc.MaskKind != EArcStampSourceKind::None)
			{
				StampMaskSource = ResolveCpuSource(SourceCache, Desc.MaskKind, Desc.MaskContentId,
					Desc.MaskTexture, Desc.MaskBakedProvider, Desc.MaskSourceChannel);
			}

			for (const TPair<FName, FArcStampLayerDesc>& LayerPair : Desc.Layers)
			{
				// A layer whose name does not resolve against the terrain's channel set does not exist;
				// one with no plane is the composite's unbound weight-RT slot. Both drop the layer
				// entirely, source resolve included (BuildCompositeJob).
				const int32 ChannelIndex =
					Config.ChannelSet ? Config.ChannelSet->FindChannelIndex(LayerPair.Key) : INDEX_NONE;
				if (ChannelIndex == INDEX_NONE)
				{
					continue;
				}
				const int32 PlaneIndex = PlaneChannels.IndexOfByKey(ChannelIndex);
				if (PlaneIndex == INDEX_NONE)
				{
					continue;
				}

				const FArcStampLayerDesc& Layer = LayerPair.Value;
				const FArcTerrainSourceCpuFloatsPtr Source = ResolveCpuSource(SourceCache, Layer.Kind,
					Layer.ContentId, Layer.Texture, Layer.BakedProvider, Layer.SourceChannel);
				if (!IsUsableRaster(Source))
				{
					OutEval.bComplete = false;
					continue;
				}

				FArcTerrainSourceCpuFloatsPtr MaskSource = StampMaskSource;
				if (Desc.MaskKind != EArcStampSourceKind::None
					&& (!IsUsableRaster(MaskSource) || MaskSource->Dims != Source->Dims))
				{
					const FIntPoint MaskDims = MaskSource.IsValid() ? MaskSource->Dims : FIntPoint::ZeroValue;
					UE_LOGF(LogArcTerrain, Warning,
						"[WeightComposite] stamp '%ls' channel '%ls' (index %d) mask DROPPED: mask dims "
						"%dx%d do not match value dims %dx%d (0x0 = mask source failed to resolve). "
						"Cell evaluation is not persistable.",
						*Stamp->GetName(), *LayerPair.Key.ToString(), ChannelIndex,
						MaskDims.X, MaskDims.Y, Source->Dims.X, Source->Dims.Y);
					MaskSource.Reset();
					OutEval.bComplete = false;
				}

				FStampWeightOp& Op = OutEval.Ops.AddDefaulted_GetRef();
				FillCommonOpFields(Op.Placement, Desc, LocalToPatch, CoverageXY);
				Op.Placement.Source = Source;
				Op.Placement.MaskSource = MaskSource;
				Op.Placement.EncodingScale = Layer.EncodingScale;
				Op.Placement.ZeroInEncoding = Layer.ZeroInEncoding;
				Op.PlaneIndex = PlaneIndex;
				ComputeSampleMap(Source->Dims, Op.Placement.UVScale, Op.Placement.UVBias);
			}
		}
	}

	void EvaluateCellWeightPlanes(const FCellWeightEval& Eval, TArray<TArray<uint8>>& OutPlanes)
	{
		using namespace ArcTerrainCpuCompositePrivate;

		OutPlanes.Reset();

		const int32 Resolution = Eval.Raster.SamplesPerSide;
		if (Resolution < 2 || Eval.PlaneChannels.Num() == 0)
		{
			return;
		}

		checkSlow(Eval.Raster.TexelToLocal > 0.0f);

		OutPlanes.SetNum(Eval.PlaneChannels.Num());
		for (TArray<uint8>& Plane : OutPlanes)
		{
			// The composite clears every weight tile to 0 before the first stamp.
			Plane.Init(0, Resolution * Resolution);
		}

		const double TexelToLocal = static_cast<double>(Eval.Raster.TexelToLocal);
		for (const FStampWeightOp& Op : Eval.Ops)
		{
			if (!Op.Placement.Source.IsValid() || !OutPlanes.IsValidIndex(Op.PlaneIndex))
			{
				continue;
			}

			uint8* Plane = OutPlanes[Op.PlaneIndex].GetData();
			const float HalfX = Op.Placement.CoverageXY.X * 0.5f;
			const float HalfY = Op.Placement.CoverageXY.Y * 0.5f;
			const double OriginTexelX = Op.Placement.PatchOriginTerrainLocal.X / TexelToLocal;
			const double OriginTexelY = Op.Placement.PatchOriginTerrainLocal.Y / TexelToLocal;

			for (int32 Row = 0; Row < Resolution; ++Row)
			{
				const double RelTexelY =
					static_cast<double>(Eval.Raster.TexelOrigin.Y + Row) - OriginTexelY;
				const double OffsetY = RelTexelY * TexelToLocal;
				uint8* RowWeights = Plane + Row * Resolution;

				for (int32 Col = 0; Col < Resolution; ++Col)
				{
					const double RelTexelX =
						static_cast<double>(Eval.Raster.TexelOrigin.X + Col) - OriginTexelX;
					const double OffsetX = RelTexelX * TexelToLocal;

					float RawValue = 0.0f;
					float Mask = 0.0f;
					if (!SampleStamp(Op.Placement, OffsetX, OffsetY, HalfX, HalfY, /*bBicubic*/ false,
						RawValue, Mask))
					{
						continue;
					}

					// The two hardware-blend passes, each landing in UNORM8 before the next reads it:
					// pass 0 (BF_Zero / BF_InverseSourceColor) then pass 1 (BF_One / BF_One), with the
					// VALUE saturated before the mask multiply (not the blended result).
					uint8& Dst = RowWeights[Col];
					const uint8 Multiplied =
						QuantizeUnorm8(static_cast<float>(Dst) / 255.0f * (1.0f - Mask));
					Dst = QuantizeUnorm8(static_cast<float>(Multiplied) / 255.0f
						+ FMath::Clamp(RawValue, 0.0f, 1.0f) * Mask);
				}
			}
		}

		const int32 BasePlane = Eval.PlaneChannels.IndexOfByKey(ArcTerrain::GBaseChannelIndex);
		if (BasePlane == INDEX_NONE)
		{
			return;
		}

		const int32 PlaneCount = OutPlanes.Num();
		const int32 TexelCount = Resolution * Resolution;
		// Outer over texels, inner over planes — inverted against the rest of this function because the
		// sum that closes a texel to 1 is cross-plane.
		for (int32 Texel = 0; Texel < TexelCount; ++Texel)
		{
			float SumOthers = 0.0f;
			for (int32 PlaneIndex = 0; PlaneIndex < PlaneCount; ++PlaneIndex)
			{
				if (PlaneIndex != BasePlane)
				{
					SumOthers += static_cast<float>(OutPlanes[PlaneIndex][Texel]) / 255.0f;
				}
			}

			const float Base = FMath::Clamp(1.0f - SumOthers, 0.0f, 1.0f);
			const float Total = FMath::Max(SumOthers, 1.0f);
			for (int32 PlaneIndex = 0; PlaneIndex < PlaneCount; ++PlaneIndex)
			{
				const float Value = (PlaneIndex == BasePlane)
					? Base
					: static_cast<float>(OutPlanes[PlaneIndex][Texel]) / 255.0f;
				OutPlanes[PlaneIndex][Texel] = QuantizeUnorm8(Value / Total);
			}
		}
	}
}
