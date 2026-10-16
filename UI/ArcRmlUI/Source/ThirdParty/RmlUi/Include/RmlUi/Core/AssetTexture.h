#pragma once

#include "Header.h"
#include "Types.h"

namespace Rml {

class RenderInterface;

using AssetTextureToken = uintptr_t;

struct AssetTextureRequest {
	TextureFileIndex index = TextureFileIndex::Invalid;
	unsigned int generation = 0;
};

using AssetTextureRequester = Function<void(const String& asset_path, AssetTextureRequest request)>;

RMLUICORE_API bool IsAssetTextureSource(const String& source);
RMLUICORE_API void SetAssetTextureRequester(AssetTextureRequester requester);
RMLUICORE_API void InstallAssetTexture(RenderInterface* render_interface, AssetTextureRequest request, Vector2i dimensions, AssetTextureToken token);
RMLUICORE_API void FailAssetTexture(RenderInterface* render_interface, AssetTextureRequest request);

} // namespace Rml
