#pragma once

#include "../../../Include/RmlUi/Core/StyleTypes.h"
#include "FontTypes.h"

namespace Rml {

class FontFaceHandleDefault;

class FontFace {
public:
	FontFace(FontFaceHandleFreetype face, Span<const byte> face_data, int face_index, Style::FontStyle style, Style::FontWeight weight);
	~FontFace();

	Style::FontStyle GetStyle() const;
	Style::FontWeight GetWeight() const;

	/// Returns a handle for positioning and rendering this face at the given size.
	/// @param[in] size The size of the desired handle, in points.
	/// @return The font handle.
	FontFaceHandleDefault* GetHandle(int size);

	/// Releases resources owned by sized font faces, including their textures and rendered glyphs.
	void ReleaseFontResources();

private:
	Style::FontStyle style;
	Style::FontWeight weight;

	// Key is font size
	using HandleMap = UnorderedMap<int, UniquePtr<FontFaceHandleDefault>>;
	HandleMap handles;

	FontFaceHandleFreetype face;
	Span<const byte> face_data;
	int face_index;
};

} // namespace Rml
