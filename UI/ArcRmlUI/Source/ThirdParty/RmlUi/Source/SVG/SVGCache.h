#pragma once

#include "../../Include/RmlUi/Core/Texture.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

class Element;
class Geometry;

namespace SVG {

	struct SVGKey;
	struct SVGTexture;

	struct SVGData : NonCopyMoveable {
		SVGData(Geometry& geometry, Texture texture, SVGTexture& texture_entry, const SVGKey& cache_key);
		~SVGData();

		bool IsReady() const;
		bool HasZeroSize() const;
		bool GetIntrinsicDimensions(Vector2f& dimensions) const;

		Geometry& geometry;
		Texture texture;
		SVGTexture& texture_entry;
		const SVGKey& cache_key;
	};

	class SVGCache {
	public:
		enum SourceType {
			File = 1, /// The source is a file path.
			Data = 2  /// The source is raw SVG data.
		};
		static void Initialize();
		static void Shutdown();

		/// Returns a handle to SVG data matching the parameters - creates new data if none is found.
		/// @param[in] source_id Key to be used for caching the SVG data.
		/// @param[in] source SVG source data or Path to a file containing the SVG source data (type specified by source_type).
		/// @param[in] source_type If source refers to a file or is the SVG source data itself.
		/// @param[in] element Element for which to calculate the dimensions and color; its 'color' is the SVG's currentColor.
		/// @param[in] crop_to_content Crop the rendered SVG to its contents.
		/// @param[in] dimensions The size in pixels to render the SVG at.
		/// @param[in] stroke_width Replaces the root element's stroke-width when not empty.
		/// @param[in] require_intrinsic Parse the source on the calling thread when the intrinsic size is not known yet.
		/// @return A handle to the SVG data, with automatic reference counting.
		static SharedPtr<SVGData> GetHandle(const String& source_id, const String& source, SourceType source_type, Element* element,
			const bool crop_to_content, Vector2f dimensions, const String& stroke_width, bool require_intrinsic);
	};

} // namespace SVG
} // namespace Rml
