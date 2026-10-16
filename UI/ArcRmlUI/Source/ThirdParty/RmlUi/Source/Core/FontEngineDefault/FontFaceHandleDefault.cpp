#include "FontFaceHandleDefault.h"
#include "../ArcPerfInternal.h"
#include "../../../Include/RmlUi/Core/Log.h"
#include "../../../Include/RmlUi/Core/Profiling.h"
#include "../../../Include/RmlUi/Core/StringUtilities.h"
#include "../../../Include/RmlUi/Core/StyleTypes.h"
#include "../TextureLayout.h"
#include "FontFaceLayer.h"
#include "FontProvider.h"
#include "FreeTypeInterface.h"
#include <algorithm>
#include <numeric>

namespace Rml {

static Vector<FontFaceHandleDefault*> handles_with_glyph_requests;

static constexpr char32_t KerningCache_AsciiSubsetBegin = 32;
static constexpr char32_t KerningCache_AsciiSubsetLast = 126;

FontFaceHandleDefault::FontFaceHandleDefault()
{
	base_layer = nullptr;
	metrics = {};
	ft_face = 0;
}

FontFaceHandleDefault::~FontFaceHandleDefault()
{
	if (!glyph_requests.empty())
		handles_with_glyph_requests.erase(std::find(handles_with_glyph_requests.begin(), handles_with_glyph_requests.end(), this));

	glyphs.clear();
	layers.clear();
}

bool FontFaceHandleDefault::Initialize(FontFaceHandleFreetype face, Span<const byte> in_face_data, int in_face_index, int font_size)
{
	ft_face = face;
	face_data = in_face_data;
	face_index = in_face_index;

	RMLUI_ASSERTMSG(layer_configurations.empty(), "Initialize must only be called once.");

	if (!FreeType::InitialiseFaceHandle(ft_face, font_size, metrics, advance_scale))
		return false;

	has_kerning = FreeType::HasKerning(ft_face);

	return true;
}

void FontFaceHandleDefault::EnsureBaseLayer()
{
	if (base_layer)
		return;

	glyphs.emplace(Character::Replacement, FreeType::CreateReplacementGlyph(metrics.size));

	base_layer = GetOrCreateLayer(nullptr);
	layer_configurations.push_back(LayerConfiguration{base_layer});
	is_layers_dirty = false;
}

const FontMetrics& FontFaceHandleDefault::GetFontMetrics() const
{
	return metrics;
}

const FontGlyphMap& FontFaceHandleDefault::GetGlyphs() const
{
	return glyphs;
}

int FontFaceHandleDefault::GetStringWidth(StringView string, const TextShapingContext& text_shaping_context, Character prior_character)
{
	RMLUI_ZoneScoped;

	bool has_set_size = false;
	const bool is_kerning_enabled = IsKerningEnabled(text_shaping_context);
	int width = 0;
	for (StringIteratorU8 it_string(string); it_string; ++it_string)
	{
		CharacterEntry entry;
		if (!GetCharacter(*it_string, entry))
			continue;

		// Adjust the cursor for the kerning between this character and the previous one.
		if (is_kerning_enabled)
			width += GetKerning(prior_character, entry.glyph_character, has_set_size);

		// Adjust the cursor for this character's advance.
		width += entry.advance;
		width += (int)text_shaping_context.letter_spacing;

		prior_character = entry.glyph_character;
	}

	return Math::Max(width, 0);
}

int FontFaceHandleDefault::GenerateLayerConfiguration(const FontEffectList& font_effects)
{
	if (font_effects.empty())
		return 0;

	EnsureBaseLayer();

	// Check each existing configuration for a match with this arrangement of effects.
	int configuration_index = 1;
	for (; configuration_index < (int)layer_configurations.size(); ++configuration_index)
	{
		const LayerConfiguration& configuration = layer_configurations[configuration_index];

		// Check the size is correct. For a match, there should be one layer in the configuration
		// plus an extra for the base layer.
		if (configuration.size() != font_effects.size() + 1)
			continue;

		// Check through each layer, checking it was created by the same effect as the one we're
		// checking.
		size_t effect_index = 0;
		for (size_t i = 0; i < configuration.size(); ++i)
		{
			// Skip the base layer ...
			if (configuration[i]->GetFontEffect() == nullptr)
				continue;

			// If the ith layer's effect doesn't match the equivalent effect, then this
			// configuration can't match.
			if (configuration[i]->GetFontEffect() != font_effects[effect_index].get())
				break;

			// Check the next one ...
			++effect_index;
		}

		if (effect_index == font_effects.size())
			return configuration_index;
	}

	// No match, so we have to generate a new layer configuration.
	layer_configurations.push_back(LayerConfiguration());
	LayerConfiguration& layer_configuration = layer_configurations.back();

	bool added_base_layer = false;

	for (size_t i = 0; i < font_effects.size(); ++i)
	{
		if (!added_base_layer && font_effects[i]->GetLayer() == FontEffect::Layer::Front)
		{
			layer_configuration.push_back(base_layer);
			added_base_layer = true;
		}

		FontFaceLayer* new_layer = GetOrCreateLayer(font_effects[i]);
		layer_configuration.push_back(new_layer);
	}

	// Add the base layer now if we still haven't added it.
	if (!added_base_layer)
		layer_configuration.push_back(base_layer);

	return (int)(layer_configurations.size() - 1);
}

bool FontFaceHandleDefault::GenerateLayerTexture(Vector<byte>& texture_data, Vector2i& texture_dimensions, const FontEffect* font_effect,
	int texture_id, int handle_version) const
{
	if (handle_version != version)
	{
		RMLUI_ERRORMSG("While generating font layer texture: Handle version mismatch in texture vs font-face.");
		return false;
	}

	auto it = std::find_if(layers.begin(), layers.end(), [font_effect](const EffectLayerPair& pair) { return pair.font_effect == font_effect; });

	if (it == layers.end())
	{
		RMLUI_ERRORMSG("While generating font layer texture: Layer id not found.");
		return false;
	}

	return it->layer->GenerateTexture(texture_data, texture_dimensions, texture_id, glyphs);
}

int FontFaceHandleDefault::GenerateString(RenderManager& render_manager, TexturedMeshList& mesh_list, StringView string, const Vector2f position,
	const ColourbPremultiplied colour, const float opacity, const TextShapingContext& text_shaping_context, const int layer_configuration_index)
{
	EnsureBaseLayer();

	RMLUI_ASSERT(layer_configuration_index >= 0);
	RMLUI_ASSERT(layer_configuration_index < (int)layer_configurations.size());

	int geometry_index = 0;
	int line_width = 0;
	bool has_set_size = false;
	bool is_kerning_enabled = IsKerningEnabled(text_shaping_context);

	UpdateLayersOnDirty();

	// Fetch the requested configuration and generate the geometry for each one.
	const LayerConfiguration& layer_configuration = layer_configurations[layer_configuration_index];

	// Each texture represents one geometry.
	const int num_geometries = std::accumulate(layer_configuration.begin(), layer_configuration.end(), 0,
		[](int sum, const FontFaceLayer* layer) { return sum + layer->GetNumTextures(); });

	mesh_list.resize(num_geometries);

	for (size_t layer_index = 0; layer_index < layer_configuration.size(); ++layer_index)
	{
		FontFaceLayer* layer = layer_configuration[layer_index];

		ColourbPremultiplied layer_colour;
		if (layer == base_layer)
			layer_colour = colour;
		else
			layer_colour = layer->GetColour(opacity);

		const int num_textures = layer->GetNumTextures();
		if (num_textures == 0)
			continue;

		RMLUI_ASSERT(geometry_index + num_textures <= (int)mesh_list.size());

		line_width = 0;
		Character prior_character = Character::Null;

		// Set the mesh and textures to the geometries.
		for (int tex_index = 0; tex_index < num_textures; ++tex_index)
			mesh_list[geometry_index + tex_index].texture = layer->GetTexture(render_manager, tex_index);

		mesh_list[geometry_index].mesh.indices.reserve(string.size() * 6);
		mesh_list[geometry_index].mesh.vertices.reserve(string.size() * 4);

		for (StringIteratorU8 it_string(string); it_string; ++it_string)
		{
			CharacterEntry entry;
			if (!GetCharacter(*it_string, entry))
				continue;

			// Adjust the cursor for the kerning between this character and the previous one.
			if (is_kerning_enabled)
				line_width += GetKerning(prior_character, entry.glyph_character, has_set_size);

			ColourbPremultiplied glyph_color = layer_colour;
			// Use white vertex colors on RGB glyphs.
			if (layer == base_layer)
			{
				const FontGlyphMap::iterator it_glyph = glyphs.find(entry.glyph_character);
				if (it_glyph != glyphs.end() && it_glyph->second.color_format == ColorFormat::RGBA8)
					glyph_color = ColourbPremultiplied(layer_colour.alpha, layer_colour.alpha);
			}

			layer->GenerateGeometry(&mesh_list[geometry_index], entry.glyph_character, Vector2f(position.x + line_width, position.y), glyph_color);

			line_width += entry.advance;
			line_width += (int)text_shaping_context.letter_spacing;
			prior_character = entry.glyph_character;
		}

		geometry_index += num_textures;
	}

	return Math::Max(line_width, 0);
}

bool FontFaceHandleDefault::UpdateLayersOnDirty()
{
	bool result = false;

	// If we are dirty, regenerate all the layers and increment the version
	if (is_layers_dirty && base_layer)
	{
		is_layers_dirty = false;
		++version;

		// Regenerate all the layers.
		// Note: The layer regeneration needs to happen in the order in which the layers were created,
		// otherwise we may end up cloning a layer which has not yet been regenerated. This means trouble!
		for (auto& pair : layers)
		{
			GenerateLayer(pair.layer.get());
		}

		result = true;
	}

	return result;
}

int FontFaceHandleDefault::GetVersion() const
{
	return version;
}

void FontFaceHandleDefault::SubmitGlyphRequests()
{
	Vector<FontFaceHandleDefault*> handles;
	handles.swap(handles_with_glyph_requests);
	for (FontFaceHandleDefault* handle : handles)
		handle->LaunchGlyphJob();
}

void FontFaceHandleDefault::RequestGlyph(Character character)
{
	if (glyph_requests.empty())
		handles_with_glyph_requests.push_back(this);
	glyph_requests.push_back(character);
}

void FontFaceHandleDefault::LaunchGlyphJob()
{
	FontFaceHandleDefault* handle = this;
	const Span<const byte> data = face_data;
	const int index = face_index;
	const int size = metrics.size;
	Vector<Character> requested;
	requested.swap(glyph_requests);

	glyph_jobs.Launch([handle, data, index, size, requested = std::move(requested)]() -> AsyncInstall {
		SharedPtr<FontGlyphMap> rendered_glyphs = MakeShared<FontGlyphMap>();
		FreeType::RenderGlyphs(data, index, size, requested, *rendered_glyphs);
		return [handle, rendered_glyphs, requested]() { handle->InstallGlyphs(*rendered_glyphs, requested); };
	});
}

void FontFaceHandleDefault::InstallGlyphs(FontGlyphMap& rendered_glyphs, const Vector<Character>& requested)
{
	RMLUI_ARC_PERF_SCOPE(FontGlyphInstall);
	RMLUI_ARC_PERF_COUNT(FontGlyphBatches, 1);
	RMLUI_ARC_PERF_COUNT(FontGlyphsInstalled, (int64_t)rendered_glyphs.size());

#ifdef RMLUI_ARC_PERF
	if (ArcPerf::Enabled())
	{
		String family;
		FreeType::GetFaceStyle(ft_face, &family, nullptr, nullptr);
		int differences = 0;
		for (const FontGlyphMap::value_type& pair : rendered_glyphs)
		{
			const UnorderedMap<Character, CharacterEntry>::iterator it_entry = characters.find(pair.first);
			if (it_entry == characters.end() || it_entry->second.advance == pair.second.advance)
				continue;
			++differences;
			Log::Message(Log::LT_WARNING, "Font advance check: '%s' size %d character %u: layout advance %d, glyph advance %d.", family.c_str(),
				metrics.size, (unsigned int)pair.first, it_entry->second.advance, pair.second.advance);
		}
		Log::Message(Log::LT_INFO, "Font glyph install: '%s' size %d, %d glyphs, %d advance differences.", family.c_str(), metrics.size,
			(int)rendered_glyphs.size(), differences);
	}
#endif

	Vector<Character> installed;
	installed.reserve(rendered_glyphs.size());
	for (FontGlyphMap::value_type& pair : rendered_glyphs)
	{
		if (glyphs.emplace(pair.first, std::move(pair.second)).second)
			installed.push_back(pair.first);
	}

	Vector<Character> failed;
	for (const Character character : requested)
	{
		if (rendered_glyphs.find(character) != rendered_glyphs.end())
			continue;
		failed.push_back(character);
		const UnorderedMap<Character, CharacterEntry>::iterator it_entry = characters.find(character);
		if (it_entry != characters.end())
			it_entry->second.glyph_character = Character::Replacement;
	}

	if (!failed.empty())
	{
		String family;
		FreeType::GetFaceStyle(ft_face, &family, nullptr, nullptr);
		Log::Message(Log::LT_WARNING, "Font glyph job for '%s' size %d built no glyph for %d characters; they are drawn as the replacement glyph.",
			family.c_str(), metrics.size, (int)failed.size());
	}

	if (installed.empty() && failed.empty())
		return;

	is_layers_dirty = true;
	UpdateLayersOnDirty();

	for (FontFaceHandleDefault* dependent : fallback_dependents)
		dependent->InstallFallbackGlyphs(this, installed, failed);
}

void FontFaceHandleDefault::InstallFallbackGlyphs(const FontFaceHandleDefault* fallback, const Vector<Character>& installed,
	const Vector<Character>& failed)
{
	bool added = false;
	for (const Character character : failed)
	{
		const UnorderedMap<Character, CharacterEntry>::iterator it_entry = characters.find(character);
		if (it_entry == characters.end() || it_entry->second.source != fallback || it_entry->second.glyph_character != character)
			continue;
		it_entry->second.glyph_character = Character::Replacement;
		added = true;
	}

	for (const Character character : installed)
	{
		const UnorderedMap<Character, CharacterEntry>::iterator it_entry = characters.find(character);
		if (it_entry == characters.end() || it_entry->second.source != fallback)
			continue;

		const FontGlyphMap::const_iterator it_glyph = fallback->glyphs.find(character);
		if (it_glyph != fallback->glyphs.end() && glyphs.emplace(character, it_glyph->second.WeakCopy()).second)
			added = true;
	}

	if (!added)
		return;

	is_layers_dirty = true;
	UpdateLayersOnDirty();
}

void FontFaceHandleDefault::AddFallbackDependent(FontFaceHandleDefault* dependent)
{
	if (std::find(fallback_dependents.begin(), fallback_dependents.end(), dependent) == fallback_dependents.end())
		fallback_dependents.push_back(dependent);
}

void FontFaceHandleDefault::FillKerningPairCache()
{
	if (!has_kerning)
		return;

	for (char32_t i = KerningCache_AsciiSubsetBegin; i <= KerningCache_AsciiSubsetLast; i++)
	{
		for (char32_t j = KerningCache_AsciiSubsetBegin; j <= KerningCache_AsciiSubsetLast; j++)
		{
			const bool first_iteration = (i == KerningCache_AsciiSubsetBegin && j == KerningCache_AsciiSubsetBegin);

			// Fetch the kerning from the font face. Submit zero font size on subsequent iterations for performance reasons.
			const int kerning = FreeType::GetKerning(ft_face, first_iteration ? metrics.size : 0, Character(i), Character(j));
			if (kerning != 0)
			{
				kerning_pair_cache.emplace(AsciiPair((i << 8) | j), KerningIntType(kerning));
			}
		}
	}
}

int FontFaceHandleDefault::GetKerning(Character lhs, Character rhs, bool& has_set_size)
{
	static_assert(' ' == 32, "Only ASCII/UTF8 character set supported.");

	// Check if we have no kerning, or if we have a control character.
	if (!has_kerning || char32_t(lhs) < ' ' || char32_t(rhs) < ' ')
		return 0;

	if (!is_kerning_pair_cache_filled)
	{
		is_kerning_pair_cache_filled = true;
		FillKerningPairCache();
		has_set_size = true;
	}

	// See if the kerning pair has been cached.
	const bool lhs_in_cache = (char32_t(lhs) >= KerningCache_AsciiSubsetBegin && char32_t(lhs) <= KerningCache_AsciiSubsetLast);
	const bool rhs_in_cache = (char32_t(rhs) >= KerningCache_AsciiSubsetBegin && char32_t(rhs) <= KerningCache_AsciiSubsetLast);

	if (lhs_in_cache && rhs_in_cache)
	{
		const auto it = kerning_pair_cache.find(AsciiPair((int(lhs) << 8) | int(rhs)));

		if (it != kerning_pair_cache.end())
		{
			return it->second;
		}

		return 0;
	}

	// Fetch it from the font face instead.
	const int result = FreeType::GetKerning(ft_face, has_set_size ? 0 : metrics.size, lhs, rhs);

	// This is purely an optimization to avoid repeatedly setting the font size in FreeType, which can be a measurable performance hit.
	has_set_size = true;

	return result;
}

bool FontFaceHandleDefault::IsKerningEnabled(const TextShapingContext& text_shaping_context) const
{
	return text_shaping_context.font_kerning != Style::FontKerning::None;
}

bool FontFaceHandleDefault::GetCharacter(Character character, CharacterEntry& out_entry, bool look_in_fallback_fonts)
{
	// Don't try to render control characters
	if ((char32_t)character < (char32_t)' ')
		return false;

	const UnorderedMap<Character, CharacterEntry>::iterator it_entry = characters.find(character);
	if (it_entry != characters.end())
	{
		out_entry = it_entry->second;
		return true;
	}

	CharacterEntry entry;
	const uint32_t glyph_index = (character == Character::Replacement ? 0 : FreeType::GetGlyphIndex(ft_face, character));
	if (glyph_index != 0)
	{
		entry.advance = FreeType::GetAdvance(ft_face, glyph_index, advance_scale);
		entry.glyph_character = character;
		entry.source = this;
		RequestGlyph(character);
	}
	else if (character != Character::Replacement)
	{
		if (!look_in_fallback_fonts)
			return false;

		const int num_fallback_faces = FontProvider::CountFallbackFontFaces();
		for (int i = 0; i < num_fallback_faces; i++)
		{
			FontFaceHandleDefault* fallback_face = FontProvider::GetFallbackFontFace(i, metrics.size);
			if (!fallback_face || fallback_face == this)
				continue;

			CharacterEntry fallback_entry;
			if (!fallback_face->GetCharacter(character, fallback_entry, false) || fallback_entry.source != fallback_face ||
				fallback_entry.glyph_character != character)
				continue;

			entry.advance = fallback_entry.advance;
			entry.glyph_character = character;
			entry.source = fallback_face;
			fallback_face->AddFallbackDependent(this);

			const FontGlyphMap::const_iterator it_glyph = fallback_face->glyphs.find(character);
			if (it_glyph != fallback_face->glyphs.end() && glyphs.emplace(character, it_glyph->second.WeakCopy()).second)
				is_layers_dirty = true;
			break;
		}
	}

	if (!entry.source)
	{
		entry.advance = FreeType::GetReplacementAdvance(metrics.size);
		entry.glyph_character = Character::Replacement;
		entry.source = this;
	}

	characters.emplace(character, entry);
	out_entry = entry;
	return true;
}

FontFaceLayer* FontFaceHandleDefault::GetOrCreateLayer(const SharedPtr<const FontEffect>& font_effect)
{
	// Search for the font effect layer first, it may have been instanced before as part of a different configuration.
	const FontEffect* font_effect_ptr = font_effect.get();
	auto it =
		std::find_if(layers.begin(), layers.end(), [font_effect_ptr](const EffectLayerPair& pair) { return pair.font_effect == font_effect_ptr; });

	if (it != layers.end())
		return it->layer.get();

	// No existing effect matches, generate a new layer for the effect.
	layers.push_back(EffectLayerPair{font_effect_ptr, nullptr});
	auto& layer = layers.back().layer;

	layer = MakeUnique<FontFaceLayer>(font_effect);
	GenerateLayer(layer.get());

	return layer.get();
}

bool FontFaceHandleDefault::GenerateLayer(FontFaceLayer* layer)
{
	RMLUI_ASSERT(layer);
	const FontEffect* font_effect = layer->GetFontEffect();
	bool result = false;

	if (!font_effect)
	{
		result = layer->Generate(this);
	}
	else
	{
		// Determine which, if any, layer the new layer should copy its geometry and textures from.
		FontFaceLayer* clone = nullptr;
		bool clone_glyph_origins = true;
		String generation_key;
		size_t fingerprint = font_effect->GetFingerprint();

		if (!font_effect->HasUniqueTexture())
		{
			clone = base_layer;
			clone_glyph_origins = false;
		}
		else
		{
			auto cache_iterator = layer_cache.find(fingerprint);
			if (cache_iterator != layer_cache.end() && cache_iterator->second != layer)
				clone = cache_iterator->second;
		}

		// Create a new layer.
		result = layer->Generate(this, clone, clone_glyph_origins);

		// Cache the layer in the layer cache if it generated its own textures (ie, didn't clone).
		if (!clone)
			layer_cache[fingerprint] = layer;
	}

	return result;
}

} // namespace Rml
