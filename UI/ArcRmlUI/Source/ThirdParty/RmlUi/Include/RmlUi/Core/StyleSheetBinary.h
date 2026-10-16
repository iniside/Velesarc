#pragma once

#include "Debug.h"
#include "Header.h"
#include "Span.h"
#include "Types.h"

namespace Rml {

class StyleSheetContainer;

using CompiledStyleSheetReader = Function<bool(const String& compiled_path, Vector<byte>& out_body)>;
RMLUICORE_API void SetCompiledStyleSheetReader(CompiledStyleSheetReader reader);

RMLUICORE_API uint64_t GetCompiledRegistryHash();

RMLUICORE_API bool WriteStyleSheetBinary(const StyleSheetContainer& container, Vector<byte>& out_body, String& out_error);

RMLUICORE_API UniquePtr<StyleSheetContainer> ReadStyleSheetBinary(Span<const byte> body, const String& source_path, String& out_error);

} // namespace Rml
