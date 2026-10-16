#include "FontProvider.h"
#include "../../../Include/RmlUi/Core/Core.h"
#include "../../../Include/RmlUi/Core/FileInterface.h"
#include "../../../Include/RmlUi/Core/Log.h"
#include "../../../Include/RmlUi/Core/Math.h"
#include "../../../Include/RmlUi/Core/StringUtilities.h"
#include "../ComputeProperty.h"
#include "FontFace.h"
#include "FontFamily.h"
#include "FreeTypeInterface.h"
#include <algorithm>

namespace Rml {

static FontProvider* g_font_provider = nullptr;

FontProvider::FontProvider()
{
	RMLUI_ASSERT(!g_font_provider);
}

FontProvider::~FontProvider()
{
	RMLUI_ASSERT(g_font_provider == this);
}

bool FontProvider::Initialise()
{
	RMLUI_ASSERT(!g_font_provider);
	if (!FreeType::Initialise())
		return false;
	g_font_provider = new FontProvider;
	return true;
}

void FontProvider::Shutdown()
{
	RMLUI_ASSERT(g_font_provider);
	delete g_font_provider;
	g_font_provider = nullptr;
	FreeType::Shutdown();
}

FontProvider& FontProvider::Get()
{
	RMLUI_ASSERT(g_font_provider);
	return *g_font_provider;
}

FontFaceHandleDefault* FontProvider::GetFontFaceHandle(const String& family, Style::FontStyle style, Style::FontWeight weight, int size)
{
	RMLUI_ASSERTMSG(family == StringUtilities::ToLower(family), "Font family name must be converted to lowercase before entering here.");

	static const String generic_families[] = {"serif", "sans-serif", "monospace", "cursive", "fantasy", "system-ui", "ui-serif", "ui-sans-serif",
		"ui-monospace", "ui-rounded", "math", "emoji", "fangsong"};

	FontFamilyMap& families = Get().font_families;

	StringList family_names;
	{
		char quote = 0;
		size_t quote_begin = 0;
		size_t item_begin = 0;
		for (size_t i = 0; i < family.size(); i++)
		{
			const char c = family[i];
			if (quote)
			{
				if (c == quote)
					quote = 0;
			}
			else if (c == '"' || c == '\'')
			{
				quote = c;
				quote_begin = i;
			}
			else if (c == ',')
			{
				family_names.push_back(StringUtilities::StripWhitespace(StringView(family, item_begin, i - item_begin)));
				item_begin = i + 1;
			}
		}

		if (quote)
		{
			String unclosed_item = family.substr(item_begin, quote_begin - item_begin) + family.substr(quote_begin + 1);
			family_names.push_back(String(1, quote) + StringUtilities::StripWhitespace(unclosed_item) + String(1, quote));
		}
		else
		{
			family_names.push_back(StringUtilities::StripWhitespace(StringView(family, item_begin, family.size() - item_begin)));
		}
	}

	const UnorderedMap<String, String>& generic_font_families = Get().generic_font_families;

	for (String& family_name : family_names)
	{
		if (family_name.size() >= 2 && (family_name.front() == '"' || family_name.front() == '\'') && family_name.back() == family_name.front())
		{
			family_name = family_name.substr(1, family_name.size() - 2);
		}
		else
		{
			StringList words;
			StringUtilities::ExpandString(words, family_name, ' ', true);
			family_name.clear();
			StringUtilities::JoinString(family_name, words, ' ');

			auto it_generic = generic_font_families.find(family_name);
			if (it_generic != generic_font_families.end())
			{
				auto it_mapped = families.find(it_generic->second);
				if (it_mapped != families.end())
					return it_mapped->second->GetFaceHandle(style, weight, size);
			}

			if (std::find(std::begin(generic_families), std::end(generic_families), family_name) != std::end(generic_families))
			{
				if (FontFaceHandleDefault* fallback_face = GetFallbackFontFace(0, size))
					return fallback_face;
				continue;
			}
		}

		auto it = families.find(family_name);
		if (it != families.end())
			return it->second->GetFaceHandle(style, weight, size);
	}

	return nullptr;
}

int FontProvider::CountFallbackFontFaces()
{
	return (int)Get().fallback_font_faces.size();
}

FontFaceHandleDefault* FontProvider::GetFallbackFontFace(int index, int font_size)
{
	auto& faces = FontProvider::Get().fallback_font_faces;

	if (index >= 0 && index < (int)faces.size())
		return faces[index]->GetHandle(font_size);

	return nullptr;
}

void FontProvider::SetGenericFontFamilies(const UnorderedMap<String, String>& generic_families, const String& fallback_family)
{
	FontProvider& provider = Get();
	provider.generic_font_families.clear();
	for (const auto& generic_family : generic_families)
		provider.generic_font_families[StringUtilities::ToLower(generic_family.first)] = StringUtilities::ToLower(generic_family.second);
	provider.fallback_font_family = StringUtilities::ToLower(fallback_family);
}

void FontProvider::ReleaseFontResources()
{
	RMLUI_ASSERT(g_font_provider);
	for (auto& name_family : g_font_provider->font_families)
		name_family.second->ReleaseFontResources();
}

bool FontProvider::LoadFontFace(const String& file_name, int face_index, bool fallback_face, Style::FontWeight weight)
{
	return LoadFontFace(file_name, face_index, {}, Style::FontStyle::Normal, weight, fallback_face);
}

bool FontProvider::LoadFontFace(const String& file_name, int face_index, const String& font_family, Style::FontStyle style, Style::FontWeight weight,
	bool fallback_face)
{
	FileInterface* file_interface = GetFileInterface();
	FileHandle handle = file_interface->Open(file_name);

	if (!handle)
	{
		Log::Message(Log::LT_ERROR, "Failed to load font face from %s, could not open file.", file_name.c_str());
		return false;
	}

	size_t length = file_interface->Length(handle);

	auto buffer_ptr = UniquePtr<byte[]>(new byte[length]);
	byte* buffer = buffer_ptr.get();
	file_interface->Read(buffer, length, handle);
	file_interface->Close(handle);

	bool result = Get().LoadFontFace({buffer, length}, face_index, fallback_face, std::move(buffer_ptr), file_name, font_family, style, weight);

	return result;
}

bool FontProvider::LoadFontFace(Span<const byte> data, int face_index, const String& font_family, Style::FontStyle style, Style::FontWeight weight,
	bool fallback_face)
{
	const String source = "memory";

	bool result = Get().LoadFontFace(data, face_index, fallback_face, nullptr, source, font_family, style, weight);

	return result;
}

bool FontProvider::LoadFontFace(Span<const byte> data, int face_index, bool fallback_face, UniquePtr<byte[]> face_memory, const String& source,
	String font_family, Style::FontStyle style, Style::FontWeight weight)
{
	using Style::FontWeight;

	Vector<FaceVariation> face_variations;
	if (!FreeType::GetFaceVariations(data, face_variations, face_index))
	{
		Log::Message(Log::LT_ERROR, "Failed to load font face from '%s': Invalid or unsupported font face file format.", source.c_str());
		return false;
	}

	Vector<FaceVariation> load_variations;
	if (face_variations.empty())
	{
		load_variations.push_back(FaceVariation{Style::FontWeight::Auto, 0, 0});
	}
	else
	{
		// Iterate through all the face variations and pick the ones to load. The list is already sorted by (weight, width). When weight is set to
		// 'auto' we load all the weights of the face. However, we only want to load one width for each weight.
		for (auto it = face_variations.begin(); it != face_variations.end();)
		{
			if (weight != FontWeight::Auto && it->weight != weight)
			{
				++it;
				continue;
			}

			// We don't currently have any way for users to select widths, so we search for a regular (medium) value here.
			constexpr int search_width = 100;
			const FontWeight current_weight = it->weight;

			int best_width_distance = Math::Absolute((int)it->width - search_width);
			auto it_best_width = it;

			// Search forward to find the best 'width' with the same weight.
			for (++it; it != face_variations.end(); ++it)
			{
				if (it->weight != current_weight)
					break;

				const int width_distance = Math::Absolute((int)it->width - search_width);
				if (width_distance < best_width_distance)
				{
					best_width_distance = width_distance;
					it_best_width = it;
				}
			}

			load_variations.push_back(*it_best_width);
		}
	}

	if (load_variations.empty())
	{
		Log::Message(Log::LT_ERROR, "Failed to load font face from '%s': Could not locate face with weight %d.", source.c_str(), (int)weight);
		return false;
	}

	for (const FaceVariation& variation : load_variations)
	{
		FontFaceHandleFreetype ft_face = FreeType::LoadFace(data, source, face_index, variation.named_instance_index);
		if (!ft_face)
			return false;

		if (font_family.empty())
			FreeType::GetFaceStyle(ft_face, &font_family, &style, nullptr);
		if (weight == FontWeight::Auto)
			FreeType::GetFaceStyle(ft_face, nullptr, nullptr, &weight);

		const FontWeight variation_weight = (variation.weight == FontWeight::Auto ? weight : variation.weight);
		const String font_face_description = GetFontFaceDescription(font_family, style, variation_weight);

		const int ft_face_index = (variation.named_instance_index << 16) | face_index;
		const FontFaceLoadResult result =
			AddFace(ft_face, data, ft_face_index, font_family, style, variation_weight, fallback_face, std::move(face_memory));
		switch (result)
		{
		case FontFaceLoadResult::Success:
			Log::Message(Log::LT_INFO, "Loaded font face %s from '%s'.", font_face_description.c_str(), source.c_str());
			break;
		case FontFaceLoadResult::Duplicate:
			Log::Message(Log::LT_INFO, "Font face %s from '%s' already loaded, proceeding.", font_face_description.c_str(), source.c_str());
			break;
		case FontFaceLoadResult::Error:
			Log::Message(Log::LT_ERROR, "Failed to load font face %s from '%s'.", font_face_description.c_str(), source.c_str());
			return false;
		}
	}

	return true;
}

auto FontProvider::AddFace(FontFaceHandleFreetype face, Span<const byte> face_data, int face_index, const String& family, Style::FontStyle style,
	Style::FontWeight weight, bool fallback_face, UniquePtr<byte[]> face_memory) -> FontFaceLoadResult
{
	if (family.empty() || weight == Style::FontWeight::Auto)
		return FontFaceLoadResult::Error;

	String family_lower = StringUtilities::ToLower(family);
	FontFamily* font_family = nullptr;
	auto it = font_families.find(family_lower);
	if (it != font_families.end())
	{
		font_family = (FontFamily*)it->second.get();
	}
	else
	{
		auto font_family_ptr = MakeUnique<FontFamily>(family_lower);
		font_family = font_family_ptr.get();
		font_families[family_lower] = std::move(font_family_ptr);
	}

	const auto [result, face_ptr] = font_family->AddFace(face, face_data, face_index, style, weight, std::move(face_memory));
	if (result != FontFaceLoadResult::Success)
		return result;

	if (face_ptr && (fallback_face || (!fallback_font_family.empty() && family_lower == fallback_font_family)))
	{
		auto it_fallback_face = std::find(fallback_font_faces.begin(), fallback_font_faces.end(), face_ptr);
		if (it_fallback_face == fallback_font_faces.end())
		{
			auto FallbackRank = [](const FontFace* fallback) {
				return (fallback->GetStyle() == Style::FontStyle::Normal ? 0 : 1000) + Math::Absolute((int)fallback->GetWeight() - 400);
			};
			const int rank = FallbackRank(face_ptr);
			auto it_insert = std::find_if(fallback_font_faces.begin(), fallback_font_faces.end(),
				[&](const FontFace* fallback) { return FallbackRank(fallback) > rank; });
			fallback_font_faces.insert(it_insert, face_ptr);
		}
	}

	return result;
}

} // namespace Rml
