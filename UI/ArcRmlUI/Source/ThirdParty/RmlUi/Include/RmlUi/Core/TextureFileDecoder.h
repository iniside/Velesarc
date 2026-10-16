#pragma once

#include "Header.h"
#include "Types.h"

namespace Rml {

using TextureFileDecoder = Function<bool(const String& source, Vector2i& dimensions, Vector<byte>& pixels)>;
RMLUICORE_API void SetTextureFileDecoder(TextureFileDecoder decoder);

} // namespace Rml
