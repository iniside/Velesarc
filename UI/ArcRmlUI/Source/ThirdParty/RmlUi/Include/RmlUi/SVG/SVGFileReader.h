#pragma once

#include "../Core/Header.h"
#include "../Core/Types.h"

namespace Rml {
namespace SVG {

	/// Reads the bytes of an SVG file; called from async jobs on any thread, so it must be thread-safe.
	using FileReader = Function<bool(const String& path, String& data)>;
	RMLUICORE_API void SetFileReader(FileReader reader);

} // namespace SVG
} // namespace Rml
