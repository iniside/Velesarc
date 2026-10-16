// Copyright Lukasz Baran. All Rights Reserved.

#include "Stamps/ArcTerrainTexturePatchModifier.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainModule.h"
#include "Engine/Texture2D.h"
#include "Tiles/ArcTerrainSourceCache.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "PrimitiveDrawingUtils.h"
#endif

namespace ArcTexPatchModifier_Local
{
#if WITH_EDITOR
	// Duplicate of ArcTerrainPatchStamp_Local::GetSourceDims (bit-identical): the unique
	// file-scoped namespace rule bars sharing it without a common header.
	FIntPoint GetSourceDims(const UTexture2D* Texture)
	{
		return Texture != nullptr && Texture->Source.IsValid()
			? FIntPoint(Texture->Source.GetSizeX(), Texture->Source.GetSizeY())
			: FIntPoint::ZeroValue;
	}

	struct FDataMapBinding
	{
		EArcTerrainDataMap Map;
		FArcTerrainDataMapSource UArcTerrainTexturePatchModifier::* Member;
	};

	constexpr FDataMapBinding DataMapBindings[] =
	{
		{EArcTerrainDataMap::Deposits, &UArcTerrainTexturePatchModifier::Deposits},
		{EArcTerrainDataMap::Wear, &UArcTerrainTexturePatchModifier::Wear},
		{EArcTerrainDataMap::Flow, &UArcTerrainTexturePatchModifier::Flow},
		{EArcTerrainDataMap::Soil, &UArcTerrainTexturePatchModifier::Soil},
		{EArcTerrainDataMap::Scree, &UArcTerrainTexturePatchModifier::Scree},
		{EArcTerrainDataMap::RockMap, &UArcTerrainTexturePatchModifier::RockMap},
		{EArcTerrainDataMap::FlowMapX, &UArcTerrainTexturePatchModifier::FlowMapX},
		{EArcTerrainDataMap::FlowMapY, &UArcTerrainTexturePatchModifier::FlowMapY},
		{EArcTerrainDataMap::Rivers, &UArcTerrainTexturePatchModifier::Rivers},
	};

	static_assert(static_cast<int32>(UE_ARRAY_COUNT(DataMapBindings)) == static_cast<int32>(EArcTerrainDataMap::Count),
		"DataMapBindings must carry exactly one property per EArcTerrainDataMap value.");

	constexpr bool AreBindingsIndexedByEnum()
	{
		for (int32 Index = 0; Index < static_cast<int32>(EArcTerrainDataMap::Count); ++Index)
		{
			if (DataMapBindings[Index].Map != static_cast<EArcTerrainDataMap>(Index))
			{
				return false;
			}
		}
		return true;
	}

	static_assert(AreBindingsIndexedByEnum(),
		"DataMapBindings must be listed in EArcTerrainDataMap order, or a mask is bound to the wrong property.");

	void ValidateDataMapSource(const UArcTerrainTexturePatchModifier& Modifier, EArcTerrainDataMap Map,
		const FArcTerrainDataMapSource& Source)
	{
		if (Source.Texture == nullptr || !Source.Texture->Source.IsValid())
		{
			return;
		}

		const FString MaskName = StaticEnum<EArcTerrainDataMap>()->GetNameStringByValue(static_cast<int64>(Map));
		const ETextureSourceFormat SourceFormat = Source.Texture->Source.GetFormat();

		if (!FArcTerrainSourceCache::IsDecodableSourceFormat(SourceFormat))
		{
			UE_LOGF(LogArcTerrain, Warning,"%ls: data map '%ls' texture '%ls' has source format %d, which the terrain source decode cannot read — the mask contributes nothing. Re-import as G8/G16/BGRA8/RGBA16/R16F/RGBA16F/R32F/RGBA32F (a .hdr import lands as TSF_BGRE8 and is unreadable).",
				*Modifier.GetPathName(), *MaskName, *Source.Texture->GetName(), static_cast<int32>(SourceFormat));
			// TSF_BGRE8 is 8-bit AND undecodable: the advisory warnings below would describe a decode
			// that never runs.
			return;
		}
		if (Source.Texture->SRGB && FArcTerrainSourceCache::IsEightBitSourceFormat(SourceFormat))
		{
			UE_LOGF(LogArcTerrain, Warning,"%ls: data map '%ls' texture '%ls' is 8-bit with SRGB enabled — the terrain source decode does not gamma-linearize, it reads the raw normalized bytes, but the texture was authored for gamma-corrected display, so those raw bytes may not be the linear values the mask needs. Use a 16-bit (G16) or non-sRGB texture.",
				*Modifier.GetPathName(), *MaskName, *Source.Texture->GetName());
		}
		if (Source.SourceChannel == EArcTerrainSourceChannel::A && FArcTerrainSourceCache::IsSingleChannelSourceFormat(SourceFormat))
		{
			UE_LOGF(LogArcTerrain, Warning,"%ls: data map '%ls' texture '%ls' is single-channel — the terrain source decode ignores the requested channel for single-channel formats, so Alpha reads the same values as the default channel; the only cost is a duplicate cache entry for the same raster.",
				*Modifier.GetPathName(), *MaskName, *Source.Texture->GetName());
		}
	}

	FVector PatchPoint(const FVector& Center, const FVector& AxisX, const FVector& AxisY, double X, double Y)
	{
		return Center + AxisX * X + AxisY * Y;
	}

	void DrawRoundedRect(FPrimitiveDrawInterface* PDI, const FVector& Center, const FVector& AxisX,
		const FVector& AxisY, double HalfX, double HalfY, double CornerRadius, const FLinearColor& Color)
	{
		const double R = FMath::Clamp(CornerRadius, 0.0, FMath::Min(HalfX, HalfY));
		const double SX = HalfX - R;
		const double SY = HalfY - R;

		PDI->DrawLine(PatchPoint(Center, AxisX, AxisY, -SX, -HalfY), PatchPoint(Center, AxisX, AxisY, SX, -HalfY), Color, SDPG_World);
		PDI->DrawLine(PatchPoint(Center, AxisX, AxisY, HalfX, -SY), PatchPoint(Center, AxisX, AxisY, HalfX, SY), Color, SDPG_World);
		PDI->DrawLine(PatchPoint(Center, AxisX, AxisY, SX, HalfY), PatchPoint(Center, AxisX, AxisY, -SX, HalfY), Color, SDPG_World);
		PDI->DrawLine(PatchPoint(Center, AxisX, AxisY, -HalfX, SY), PatchPoint(Center, AxisX, AxisY, -HalfX, -SY), Color, SDPG_World);

		if (R <= UE_KINDA_SMALL_NUMBER)
		{
			return;
		}

		// DrawArc takes DEGREES (it multiplies by UE_PI/180 internally), unlike DrawCircle.
		DrawArc(PDI, PatchPoint(Center, AxisX, AxisY, SX, SY), AxisX, AxisY, 0.0f, 90.0f, R, 12, Color, SDPG_World);
		DrawArc(PDI, PatchPoint(Center, AxisX, AxisY, -SX, SY), AxisX, AxisY, 90.0f, 180.0f, R, 12, Color, SDPG_World);
		DrawArc(PDI, PatchPoint(Center, AxisX, AxisY, -SX, -SY), AxisX, AxisY, 180.0f, 270.0f, R, 12, Color, SDPG_World);
		DrawArc(PDI, PatchPoint(Center, AxisX, AxisY, SX, -SY), AxisX, AxisY, 270.0f, 360.0f, R, 12, Color, SDPG_World);
	}
#endif // WITH_EDITOR
}

bool UArcTerrainTexturePatchModifier::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
#if WITH_EDITOR
	const bool bHasHeight = (HeightTexture != nullptr && HeightTexture->Source.IsValid());
	if (bHasHeight)
	{
		OutDesc.Kind = EArcStampSourceKind::Texture;
		OutDesc.ContentId = HeightTexture->Source.GetId();
		OutDesc.Texture = HeightTexture;
		OutDesc.SourceDims = ArcTexPatchModifier_Local::GetSourceDims(HeightTexture);
		OutDesc.SourceChannel = HeightSourceChannel;
		OutDesc.ZeroInEncoding = HeightZeroInEncoding;
		OutDesc.bDeriveZeroFromBorder = bAutoHeightZero;
		OutDesc.EncodingScale = HeightEncodingScale;
	}
	else
	{
		OutDesc.Kind = EArcStampSourceKind::None; // weight-only: header sanctions null height
	}

	for (int32 WeightIndex = 0; WeightIndex < WeightLayers.Num(); ++WeightIndex)
	{
		const FArcTerrainPatchDirectWeight& Weight = WeightLayers[WeightIndex];
		// Parity with UArcTerrainPatchStamp: an entry without a texture OR without a target
		// channel is dropped entirely — no weight pass and no carve.
		if (Weight.Texture == nullptr || Weight.TargetChannel.IsNone())
		{
			if (ArcTerrainDiag::IsWeightCompositeVerbose())
			{
				UE_LOGF(LogArcTerrain, Warning,"'%ls' WeightLayers[%d] is dropped and contributes nothing: %ls%ls. The composite never "
						 "sees this layer, so no weight-composite diagnostic will mention it either.",
					*GetPathName(), WeightIndex,
					Weight.Texture == nullptr ? TEXT("no texture") : TEXT(""),
					Weight.TargetChannel.IsNone()
						? (Weight.Texture == nullptr ? TEXT(" and no target channel") : TEXT("no target channel"))
						: TEXT(""));
			}
			continue;
		}

		// No Source.IsValid() guard here (unlike the height branch): an invalid source yields
		// a default ContentId + SourceDims=Zero and the subsystem drops the layer in
		// ResolveSource — 1:1 with ArcTerrainPatchStamp.cpp DirectWeights.
		FArcStampLayerDesc Layer;
		Layer.Kind = EArcStampSourceKind::Texture;
		Layer.ContentId = Weight.Texture->Source.GetId();
		Layer.Texture = Weight.Texture;
		Layer.SourceDims = ArcTexPatchModifier_Local::GetSourceDims(Weight.Texture);
		Layer.SourceChannel = Weight.SourceChannel;
		Layer.EncodingScale = Weight.EncodingScale;
		Layer.ZeroInEncoding = Weight.ZeroInEncoding;
		Layer.bContributeToHeight = Weight.bContributeToHeight;
		Layer.HeightAmplitude = Weight.HeightAmplitude;

		OutDesc.Layers.Emplace(Weight.TargetChannel, MoveTemp(Layer));
	}

	switch (MaskShape)
	{
	case EArcTerrainStampMaskShape::Box:
		OutDesc.EdgeFalloffBand = EdgeFalloffBand;
		OutDesc.EdgeSharpness = EdgeSharpness;
		OutDesc.CornerRadius = CornerRadius;
		OutDesc.bCircleMask = false;
		OutDesc.CircleCenterOffset = FVector2D::ZeroVector;
		OutDesc.CircleRadius = 0.0f;
		OutDesc.CircleFalloffBand = 0.0f;
		break;

	case EArcTerrainStampMaskShape::Circle:
		// The footprint rect stays a hard clip: the masks multiply, so an edge band here would
		// darken the rim twice.
		OutDesc.EdgeFalloffBand = 0.0f;
		OutDesc.EdgeSharpness = 1.0f;
		OutDesc.CornerRadius = 0.0f;
		OutDesc.bCircleMask = true;
		OutDesc.CircleCenterOffset = CircleCenterOffset;
		OutDesc.CircleRadius = CircleRadius;
		OutDesc.CircleFalloffBand = CircleFalloffBand;
		break;

	case EArcTerrainStampMaskShape::None:
	default:
		OutDesc.EdgeFalloffBand = 0.0f;
		OutDesc.EdgeSharpness = 1.0f;
		OutDesc.CornerRadius = 0.0f;
		OutDesc.bCircleMask = false;
		OutDesc.CircleCenterOffset = FVector2D::ZeroVector;
		OutDesc.CircleRadius = 0.0f;
		OutDesc.CircleFalloffBand = 0.0f;
		break;
	}

	return bHasHeight || OutDesc.Layers.Num() > 0;
#else
	return false;
#endif
}

#if WITH_EDITOR
TArray<TPair<EArcTerrainDataMap, const FArcTerrainDataMapSource*>> UArcTerrainTexturePatchModifier::GetDataMapSources() const
{
	TArray<TPair<EArcTerrainDataMap, const FArcTerrainDataMapSource*>> Sources;
	Sources.Reserve(UE_ARRAY_COUNT(ArcTexPatchModifier_Local::DataMapBindings));
	for (const ArcTexPatchModifier_Local::FDataMapBinding& Binding : ArcTexPatchModifier_Local::DataMapBindings)
	{
		Sources.Emplace(Binding.Map, &(this->*Binding.Member));
	}
	return Sources;
}

void UArcTerrainTexturePatchModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Super is the single invalidation routing point (re-home on AffectedTerrain, else NotifyStampChanged).
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	for (const TPair<EArcTerrainDataMap, const FArcTerrainDataMapSource*>& Entry : GetDataMapSources())
	{
		ArcTexPatchModifier_Local::ValidateDataMapSource(*this, Entry.Key, *Entry.Value);
	}
}

void UArcTerrainTexturePatchModifier::DrawStampVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) const
{
	const AArcTerrain* Terrain = AffectedTerrain.Get();
	if (Terrain == nullptr)
	{
		return;
	}

	const FBox TerrainLocalBounds = ComputeBounds();
	if (!TerrainLocalBounds.IsValid)
	{
		return;
	}
	const FMatrix TerrainToWorld = Terrain->GetTerrainTransform().ToMatrixWithScale();
	DrawWireBox(PDI, TerrainToWorld, TerrainLocalBounds, FLinearColor::Green, SDPG_World);

	if (MaskShape == EArcTerrainStampMaskShape::None)
	{
		return;
	}

	const FMatrix LocalToWorld = GetComponentTransform().ToMatrixWithScale();
	// Not normalized: the axis length carries the component scale, which is what turns the unscaled
	// patch-local cm of the mask properties into their world size.
	const FVector AxisX = LocalToWorld.GetScaledAxis(EAxis::X);
	const FVector AxisY = LocalToWorld.GetScaledAxis(EAxis::Y);
	const FVector2D Half = CoverageXY * 0.5;
	const FLinearColor OuterColor = FLinearColor::Yellow;
	const FLinearColor InnerColor = FLinearColor(0.35f, 0.35f, 0.0f);

	if (MaskShape == EArcTerrainStampMaskShape::Box)
	{
		const FVector BoxCenter = LocalToWorld.TransformPosition(FVector::ZeroVector);
		ArcTexPatchModifier_Local::DrawRoundedRect(PDI, BoxCenter, AxisX, AxisY, Half.X, Half.Y, CornerRadius, OuterColor);

		const double Band = EdgeFalloffBand;
		const double InnerHalfX = Half.X - Band;
		const double InnerHalfY = Half.Y - Band;
		if (Band > UE_KINDA_SMALL_NUMBER && InnerHalfX > UE_KINDA_SMALL_NUMBER && InnerHalfY > UE_KINDA_SMALL_NUMBER)
		{
			const double ClampedRadius = FMath::Clamp(static_cast<double>(CornerRadius), 0.0, FMath::Min(Half.X, Half.Y));
			ArcTexPatchModifier_Local::DrawRoundedRect(PDI, BoxCenter, AxisX, AxisY, InnerHalfX, InnerHalfY,
				FMath::Max(ClampedRadius - Band, 0.0), InnerColor);
		}
		return;
	}

	const FVector CircleCenter = LocalToWorld.TransformPosition(FVector(CircleCenterOffset.X, CircleCenterOffset.Y, 0.0));
	if (CircleRadius > 0.0f)
	{
		DrawCircle(PDI, CircleCenter, AxisX, AxisY, OuterColor, CircleRadius, 48, SDPG_World);

		const double InnerRadius = static_cast<double>(CircleRadius) - CircleFalloffBand;
		if (InnerRadius > UE_KINDA_SMALL_NUMBER)
		{
			DrawCircle(PDI, CircleCenter, AxisX, AxisY, InnerColor, InnerRadius, 48, SDPG_World);
		}
		return;
	}

	DrawCircle(PDI, CircleCenter, AxisX * Half.X, AxisY * Half.Y, OuterColor, 1.0, 48, SDPG_World);

	// Approximation: the shader measures the inscribed-ellipse band along each ray, so its inner
	// contour is a uniformly scaled ellipse only when the footprint is square.
	const double InnerScale = 1.0 - static_cast<double>(CircleFalloffBand)
		/ FMath::Max(FMath::Min(Half.X, Half.Y), static_cast<double>(UE_KINDA_SMALL_NUMBER));
	if (InnerScale > UE_KINDA_SMALL_NUMBER)
	{
		DrawCircle(PDI, CircleCenter, AxisX * Half.X * InnerScale, AxisY * Half.Y * InnerScale, InnerColor, 1.0, 48, SDPG_World);
	}
}
#endif // WITH_EDITOR
