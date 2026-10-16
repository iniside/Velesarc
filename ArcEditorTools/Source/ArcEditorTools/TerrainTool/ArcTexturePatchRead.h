// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TerrainTool/ArcTexturePatchAsset.h" // EArcTextureSourceChannel

class UTexture2D;
struct FArcBakedHeightField;

/**
 * Free-function reader that decodes a single UTexture2D channel into a raster FArcBakedHeightField,
 * shared by the Texture Patch Stamp modifier's height and weight reads (only scale/zero differ).
 *
 * Editor-only lossless source read via UE::AssetUtils::ReadTexture. The output plugs straight into
 * ArcHeightmapGraph::ToField, so its row-major (Dims.X inner) layout matches the op's sampling.
 */
namespace ArcTexturePatchRead
{
	/**
	 * Decode Texture's SourceChannel into OutField. field value = (texel - ZeroInEncoding) * EncodingScale.
	 *
	 * Returns false (and logs a warning) on: null texture, failed read, or a texture whose smaller
	 * dimension is < 2 (ArcHeightmapGraph::ToField check()s Dims >= 2 and would otherwise crash).
	 * Also warns (but still succeeds) when the source is 8-bit sRGB (gamma-warped values) or when
	 * Alpha is requested from a single-channel source (constant 1.0).
	 */
	bool ReadTextureToBakedField(
		UTexture2D* Texture,
		EArcTextureSourceChannel SourceChannel,
		double EncodingScale,
		double ZeroInEncoding,
		FArcBakedHeightField& OutField);
}
