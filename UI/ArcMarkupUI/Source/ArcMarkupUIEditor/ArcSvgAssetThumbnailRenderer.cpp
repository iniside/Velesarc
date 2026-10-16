/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcSvgAssetThumbnailRenderer.h"

// nanosvg — include only; the Nanosvg module's nanosvg_implementation.cpp already
// defines NANOSVG_IMPLEMENTATION and NANOSVGRAST_IMPLEMENTATION for the entire build.
// Do NOT redefine them here or the linker will see duplicate symbols.
// Note: Nanosvg.Build.cs adds PublicDefinitions "NSVG_USE_BGRA=1", so the rasterizer
// emits BGRA bytes — we therefore upload into PF_B8G8R8A8, not PF_R8G8B8A8.
THIRD_PARTY_INCLUDES_START
#include <nanosvg.h>
#include <nanosvgrast.h>
THIRD_PARTY_INCLUDES_END

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Texture2D.h"
#include "Svg/ArcSvgAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSvgAssetThumbnailRenderer)

namespace ArcSvgAssetThumbnailRenderer_Private
{
	// Matches the DPI constant used by FSlateSVGRasterizer (SlateSVGRasterizer.cpp).
	static const float SVG_DPI = 96.0f;

	// Bytes per pixel — BGRA (4 channels), matching the NSVG_USE_BGRA=1 public define.
	static const int32 SVG_BPP = 4;

	/**
	 * Rasterize InSvgData into a flat BGRA pixel buffer of InWidth * InHeight * 4 bytes.
	 * Returns an empty array if parsing fails or the SVG has no geometry.
	 *
	 * nsvgParse mutates its input buffer, so we work on a private NUL-terminated copy.
	 * Mirrors the rasterization sequence in FSlateSVGRasterizer::RasterizeSVGFromFile.
	 */
	TArray<uint8> RasterizeSvg(const TArray<uint8>& InSvgData, uint32 InWidth, uint32 InHeight)
	{
		TArray<uint8> PixelData;

		// Build a NUL-terminated mutable copy; nsvgParse tokenises in-place.
		TArray<uint8> MutableBuffer;
		MutableBuffer.Reserve(InSvgData.Num() + 1);
		MutableBuffer.Append(InSvgData);
		MutableBuffer.Add(0);

		NSVGimage* Image = nsvgParse(reinterpret_cast<char*>(MutableBuffer.GetData()), "px", SVG_DPI);
		if (Image == nullptr)
		{
			return PixelData;
		}

		if (Image->width <= 0.0f || Image->height <= 0.0f)
		{
			nsvgDelete(Image);
			return PixelData;
		}

		const float ScaleX = static_cast<float>(InWidth) / Image->width;
		const float ScaleY = static_cast<float>(InHeight) / Image->height;
		const int32 Stride = static_cast<int32>(InWidth) * SVG_BPP;

		PixelData.AddUninitialized(static_cast<int32>(InWidth) * static_cast<int32>(InHeight) * SVG_BPP);

		NSVGrasterizer* Rasterizer = nsvgCreateRasterizer();
		nsvgRasterizeFull(Rasterizer, Image, 0, 0, ScaleX, ScaleY,
		                  PixelData.GetData(), static_cast<int32>(InWidth), static_cast<int32>(InHeight), Stride);

		nsvgDeleteRasterizer(Rasterizer);
		nsvgDelete(Image);

		return PixelData;
	}
}

bool UArcSvgAssetThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	return Cast<UArcSvgAsset>(Object) != nullptr;
}

EThumbnailRenderFrequency UArcSvgAssetThumbnailRenderer::GetThumbnailRenderFrequency(UObject* Object) const
{
	return EThumbnailRenderFrequency::OnAssetSave;
}

void UArcSvgAssetThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
                                          FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UArcSvgAsset* Svg = Cast<UArcSvgAsset>(Object);
	if (Svg == nullptr || Svg->SvgData.IsEmpty())
	{
		return;
	}

	TArray<uint8> PixelData = ArcSvgAssetThumbnailRenderer_Private::RasterizeSvg(Svg->SvgData, Width, Height);
	if (PixelData.IsEmpty())
	{
		return;
	}

	// PF_B8G8R8A8 matches the BGRA channel order nanosvg emits when NSVG_USE_BGRA=1.
	UTexture2D* Texture = UTexture2D::CreateTransient(static_cast<int32>(Width), static_cast<int32>(Height), PF_B8G8R8A8);
	if (Texture == nullptr)
	{
		return;
	}

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	uint8* MipData = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	FMemory::Memcpy(MipData, PixelData.GetData(), PixelData.Num());
	Mip.BulkData.Unlock();
	Texture->UpdateResource();

	// UpdateResource() enqueues the RT resource-init command; FCanvasTileItem::Draw
	// enqueues the draw command immediately after — the render thread processes them in
	// submission order, so the resource is initialised before the draw executes.
	FCanvasTileItem Tile(FVector2D(static_cast<float>(X), static_cast<float>(Y)),
	                     Texture->GetResource(),
	                     FVector2D(static_cast<float>(Width), static_cast<float>(Height)),
	                     FLinearColor::White);
	Tile.BlendMode = SE_BLEND_Translucent;
	Tile.Draw(Canvas);
}
