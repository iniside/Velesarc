// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/ArcTexturePatchRead.h"

#include "AssetUtils/Texture2DUtil.h"
#include "Engine/Texture2D.h"
#include "Image/ImageBuilder.h"
#include "Image/ImageDimensions.h"
#include "TerrainTool/Graph/ArcHeightmapGraph.h"  // FArcBakedHeightField

// Unique, file-scoped helper namespace (project C++ style).
namespace ArcTexturePatchReadLocals_7f2a
{
	DEFINE_LOG_CATEGORY_STATIC(LogArcTexturePatchRead, Log, All);

#if WITH_EDITOR
	/** Single-channel source formats fill the alpha channel with a constant 1.0 on read. */
	bool IsSingleChannelSourceFormat(ETextureSourceFormat Format)
	{
		return Format == TSF_G8 || Format == TSF_G16 || Format == TSF_R16F || Format == TSF_R32F;
	}

	/** 8-bit source formats that get gamma-linearized when SRGB is enabled. */
	bool IsEightBitSourceFormat(ETextureSourceFormat Format)
	{
		return Format == TSF_G8 || Format == TSF_BGRA8 || Format == TSF_BGRE8;
	}
#endif // WITH_EDITOR
}

bool ArcTexturePatchRead::ReadTextureToBakedField(
	UTexture2D* Texture,
	EArcTextureSourceChannel SourceChannel,
	double EncodingScale,
	double ZeroInEncoding,
	FArcBakedHeightField& OutField)
{
	using namespace ArcTexturePatchReadLocals_7f2a;

	OutField.Reset();

	if (Texture == nullptr)
	{
		UE_LOG(LogArcTexturePatchRead, Warning, TEXT("ArcTexturePatchRead: null texture — nothing to read."));
		return false;
	}

#if WITH_EDITOR
	if (Texture->Source.IsValid())
	{
		const ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();
		if (Texture->SRGB && IsEightBitSourceFormat(SourceFormat))
		{
			UE_LOG(LogArcTexturePatchRead, Warning,
				TEXT("ArcTexturePatchRead: texture '%s' is 8-bit with SRGB enabled — source values are gamma-linearized, warping heights/weights. Use a 16-bit (G16) or non-sRGB texture."),
				*Texture->GetName());
		}
		if (SourceChannel == EArcTextureSourceChannel::Alpha && IsSingleChannelSourceFormat(SourceFormat))
		{
			UE_LOG(LogArcTexturePatchRead, Warning,
				TEXT("ArcTexturePatchRead: texture '%s' is single-channel — its Alpha channel reads a constant 1.0, so the field will be flat."),
				*Texture->GetName());
		}
	}
#endif // WITH_EDITOR

	UE::Geometry::TImageBuilder<FVector4f> Image;
	if (!UE::AssetUtils::ReadTexture(Texture, Image, /*bPreferPlatformData*/ false))
	{
		UE_LOG(LogArcTexturePatchRead, Warning,
			TEXT("ArcTexturePatchRead: failed to read texture '%s'."), *Texture->GetName());
		return false;
	}

	const UE::Geometry::FImageDimensions& Dims = Image.GetDimensions();
	const int32 Width = static_cast<int32>(Dims.GetWidth());
	const int32 Height = static_cast<int32>(Dims.GetHeight());
	if (Width < 2 || Height < 2)
	{
		UE_LOG(LogArcTexturePatchRead, Warning,
			TEXT("ArcTexturePatchRead: texture '%s' is %dx%d — must be at least 2x2 to build a field."),
			*Texture->GetName(), Width, Height);
		return false;
	}

	const int32 ChannelIdx = static_cast<int32>(SourceChannel);
	const int64 Num = Dims.Num();

	// FImageDimensions::GetIndex is Y * Width + X (row-major, X inner), matching FArcBakedHeightField.
	OutField.Dims = FIntPoint(Width, Height);
	OutField.Values.SetNumUninitialized(static_cast<int32>(Num));
	for (int64 Idx = 0; Idx < Num; ++Idx)
	{
		const FVector4f& Pixel = Image.GetPixel(Idx);
		const double Texel = static_cast<double>(Pixel[ChannelIdx]);
		OutField.Values[static_cast<int32>(Idx)] = static_cast<float>((Texel - ZeroInEncoding) * EncodingScale);
	}

	return true;
}
