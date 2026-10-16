#pragma once

#include "../../../Include/RmlUi/Core/AsyncJobInterface.h"
#include "../../../Include/RmlUi/Core/FontEffect.h"
#include "../../../Include/RmlUi/Core/FontGlyph.h"
#include "../../../Include/RmlUi/Core/FontMetrics.h"
#include "../../../Include/RmlUi/Core/Geometry.h"
#include "../../../Include/RmlUi/Core/TextShapingContext.h"
#include "../../../Include/RmlUi/Core/Texture.h"
#include "../../../Include/RmlUi/Core/Traits.h"
#include "FontTypes.h"

namespace Rml {

class FontFaceLayer;

class FontFaceHandleDefault final : public NonCopyMoveable {
public:
	FontFaceHandleDefault();
	~FontFaceHandleDefault();

	bool Initialize(FontFaceHandleFreetype face, Span<const byte> face_data, int face_index, int font_size);

	const FontMetrics& GetFontMetrics() const;

	const FontGlyphMap& GetGlyphs() const;

	/// Returns the width a string will take up if rendered with this handle.
	/// @param[in] string The string to measure.
	/// @param[in] text_shaping_context Extra parameters that provide context for text shaping.
	/// @param[in] prior_character The optionally-specified character that immediately precedes the string. This may have an impact on the string
	/// width due to kerning.
	/// @return The width, in pixels, this string will occupy if rendered with this handle.
	int GetStringWidth(StringView string, const TextShapingContext& text_shaping_context, Character prior_character = Character::Null);

	/// Generates, if required, the layer configuration for a given list of font effects.
	/// @param[in] font_effects The list of font effects to generate the configuration for.
	/// @return The index to use when generating geometry using this configuration.
	int GenerateLayerConfiguration(const FontEffectList& font_effects);
	/// Generates the texture data for a layer (for the texture database).
	/// @param[out] texture_data The generated texture data.
	/// @param[out] texture_dimensions The dimensions of the texture.
	/// @param[in] font_effect The font effect used for the layer.
	/// @param[in] texture_id The index of the texture within the layer to generate.
	/// @param[in] handle_version The version of the handle data. Function returns false if out of date.
	bool GenerateLayerTexture(Vector<byte>& texture_data, Vector2i& texture_dimensions, const FontEffect* font_effect, int texture_id,
		int handle_version) const;

	/// Generates the geometry required to render a single line of text.
	/// @param[in] render_manager The render manager responsible for rendering the string.
	/// @param[out] mesh_list A list to place the new meshes into.
	/// @param[in] string The string to render.
	/// @param[in] position The position of the baseline of the first character to render.
	/// @param[in] colour The colour to render the text.
	/// @param[in] opacity The opacity of the text, should be applied to font effects.
	/// @param[in] text_shaping_context Extra parameters that provide context for text shaping.
	/// @param[in] layer_configuration Face configuration index to use for generating string.
	/// @return The width, in pixels, of the string geometry.
	int GenerateString(RenderManager& render_manager, TexturedMeshList& mesh_list, StringView string, Vector2f position, ColourbPremultiplied colour,
		float opacity, const TextShapingContext& text_shaping_context, int layer_configuration);

	/// Version is changed whenever the layers are dirtied, requiring regeneration of string geometry.
	int GetVersion() const;

	static void SubmitGlyphRequests();

private:
	struct CharacterEntry {
		int advance = 0;
		// The character whose glyph is drawn, the replacement character when no face has a glyph for it.
		Character glyph_character = Character::Null;
		FontFaceHandleDefault* source = nullptr;
	};

	bool GetCharacter(Character character, CharacterEntry& out_entry, bool look_in_fallback_fonts = true);

	void EnsureBaseLayer();

	void RequestGlyph(Character character);
	void LaunchGlyphJob();
	void InstallGlyphs(FontGlyphMap& rendered_glyphs, const Vector<Character>& requested);
	void InstallFallbackGlyphs(const FontFaceHandleDefault* fallback, const Vector<Character>& installed, const Vector<Character>& failed);
	void AddFallbackDependent(FontFaceHandleDefault* dependent);

	// Build a kerning cache for common characters.
	void FillKerningPairCache();

	// Return the kerning for a character pair.
	int GetKerning(Character lhs, Character rhs, bool& has_set_size);

	// Returns whether kerning is enabled based on the text-shaping context.
	bool IsKerningEnabled(const TextShapingContext& text_shaping_context) const;

	// Regenerate layers if dirty, such as after adding new glyphs.
	bool UpdateLayersOnDirty();

	// Create a new layer from the given font effect if it does not already exist.
	FontFaceLayer* GetOrCreateLayer(const SharedPtr<const FontEffect>& font_effect);

	// (Re-)generate a layer in this font face handle.
	bool GenerateLayer(FontFaceLayer* layer);

	FontGlyphMap glyphs;
	UnorderedMap<Character, CharacterEntry> characters;
	Vector<Character> glyph_requests;
	Vector<FontFaceHandleDefault*> fallback_dependents;
	AsyncJobOwner glyph_jobs;

	struct EffectLayerPair {
		const FontEffect* font_effect;
		UniquePtr<FontFaceLayer> layer;
	};
	using FontLayerMap = Vector<EffectLayerPair>;
	using FontLayerCache = SmallUnorderedMap<size_t, FontFaceLayer*>;
	using LayerConfiguration = Vector<FontFaceLayer*>;
	using LayerConfigurationList = Vector<LayerConfiguration>;

	// The list of all font layers, index by the effect that instanced them.
	FontFaceLayer* base_layer;
	FontLayerMap layers;
	// Each font layer that generated geometry or textures, indexed by the font-effect's fingerprint key.
	FontLayerCache layer_cache;

	// Pre-cache kerning pairs for some ascii subset of all characters.
	using AsciiPair = uint16_t;
	using KerningIntType = int16_t;
	using KerningPairs = UnorderedMap<AsciiPair, KerningIntType>;
	KerningPairs kerning_pair_cache;

	bool has_kerning = false;
	bool is_kerning_pair_cache_filled = false;
	bool is_layers_dirty = false;
	int version = 0;

	// All configurations currently in use on this handle. New configurations will be generated as required.
	LayerConfigurationList layer_configurations;

	FontMetrics metrics;
	FontAdvanceScale advance_scale;

	FontFaceHandleFreetype ft_face;
	Span<const byte> face_data;
	int face_index = 0;
};

} // namespace Rml
