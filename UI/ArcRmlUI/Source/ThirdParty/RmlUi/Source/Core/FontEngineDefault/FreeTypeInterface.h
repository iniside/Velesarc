#pragma once

#include "../../../Include/RmlUi/Core/FontMetrics.h"
#include "FontTypes.h"

namespace Rml {

namespace FreeType {

	// Initialize FreeType library.
	bool Initialise();
	// Shutdown FreeType library.
	void Shutdown();

	// Returns a sorted list of available font variations for the font face located in memory.
	bool GetFaceVariations(Span<const byte> data, Vector<FaceVariation>& out_face_variations, int face_index);

	// Loads a FreeType face from memory, 'source' is only used for logging.
	FontFaceHandleFreetype LoadFace(Span<const byte> data, const String& source, int face_index, int named_instance_index = 0);

	// Releases the FreeType face.
	bool ReleaseFace(FontFaceHandleFreetype face);

	// Retrieves the font family, style and weight of the given font face. Use nullptr to ignore a property.
	void GetFaceStyle(FontFaceHandleFreetype face, String* font_family, Style::FontStyle* style, Style::FontWeight* weight);

	// Initializes a face for a given font size, sets the font face metrics and returns the horizontal scale used by GetAdvance.
	bool InitialiseFaceHandle(FontFaceHandleFreetype face, int font_size, FontMetrics& metrics, FontAdvanceScale& out_advance_scale);

	// Returns the glyph index of the character in the face, or zero when the face has no glyph for it.
	uint32_t GetGlyphIndex(FontFaceHandleFreetype face, Character character);

	// Returns the advance of the glyph at the scale of a face handle, read from the font tables without loading the glyph.
	int GetAdvance(FontFaceHandleFreetype face, uint32_t glyph_index, const FontAdvanceScale& advance_scale);

	// Builds the procedural glyph drawn for characters that no face has.
	FontGlyph CreateReplacementGlyph(int font_size);
	int GetReplacementAdvance(int font_size);

	// Loads and renders the characters through a FreeType library and face of its own over the face data. Safe to call from any thread.
	void RenderGlyphs(Span<const byte> face_data, int face_index, int font_size, const Vector<Character>& characters, FontGlyphMap& out_glyphs);

	// Returns the kerning between two characters.
	// 'font_size' value of zero assumes the font size is already set on the face, and skips this step for performance reasons.
	int GetKerning(FontFaceHandleFreetype face, int font_size, Character lhs, Character rhs);

	// Returns true if the font face has kerning.
	bool HasKerning(FontFaceHandleFreetype face);

} // namespace FreeType
} // namespace Rml
