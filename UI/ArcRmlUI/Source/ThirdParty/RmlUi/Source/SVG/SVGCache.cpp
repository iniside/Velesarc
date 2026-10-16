#include "SVGCache.h"
#include "../Core/ArcPerfInternal.h"
#include "../../Include/RmlUi/Core/AsyncJobInterface.h"
#include "../../Include/RmlUi/Core/CallbackTexture.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Core.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/ElementDocument.h"
#include "../../Include/RmlUi/Core/Geometry.h"
#include "../../Include/RmlUi/Core/MeshUtilities.h"
#include "../../Include/RmlUi/Core/RenderManager.h"
#include "../../Include/RmlUi/Core/SystemInterface.h"
#include "../../Include/RmlUi/Core/Texture.h"
#include "../../Include/RmlUi/Core/Utilities.h"
#include "../../Include/RmlUi/SVG/SVGFileReader.h"
#include "../Core/ControlledLifetimeResource.h"
#include <algorithm>
#include <lunasvg.h>

#ifdef RMLUI_SVG_DEBUG
	#define RMLUI_SVG_DEBUG_LOG(...) Rml::Log::Message(Rml::Log::LT_DEBUG, __VA_ARGS__)
#else
	#define RMLUI_SVG_DEBUG_LOG(...)
#endif

namespace Rml {
namespace SVG {
	struct SVGKey {
		String source_id;
		Vector2i dimensions;
		bool crop_to_content;
		ColourbPremultiplied colour;
		Colourb current_colour;
		String stroke_width;

		friend bool operator==(const SVGKey& lhs, const SVGKey& rhs)
		{
			return lhs.source_id == rhs.source_id && lhs.dimensions == rhs.dimensions && lhs.crop_to_content == rhs.crop_to_content &&
				lhs.colour == rhs.colour && lhs.current_colour == rhs.current_colour && lhs.stroke_width == rhs.stroke_width;
		}
	};
} // namespace SVG
} // namespace Rml

namespace std {
template <>
struct hash<::Rml::SVG::SVGKey> {
	size_t operator()(const ::Rml::SVG::SVGKey& key) const noexcept
	{
		size_t hash = 0;
		Rml::Utilities::HashCombine(hash, key.source_id);
		Rml::Utilities::HashCombine(hash, key.dimensions.x);
		Rml::Utilities::HashCombine(hash, key.dimensions.y);
		Rml::Utilities::HashCombine(hash, key.crop_to_content);
		static_assert(sizeof(uint32_t) == sizeof(key.colour), "Expecting color to be 4 bytes");
		Rml::Utilities::HashCombine(hash, *reinterpret_cast<const uint32_t*>(&key.colour[0]));
		static_assert(sizeof(uint32_t) == sizeof(key.current_colour), "Expecting current color to be 4 bytes");
		Rml::Utilities::HashCombine(hash, *reinterpret_cast<const uint32_t*>(&key.current_colour[0]));
		Rml::Utilities::HashCombine(hash, key.stroke_width);
		return hash;
	}
};
} // namespace std

namespace Rml {
namespace SVG {

	static SharedPtr<SVGData> GetHandle(RenderManager& render_manager, String source_id, const String* source_data, Vector2i dimensions,
		bool crop_to_content, ColourbPremultiplied colour, Colourb current_colour, const String& stroke_width, bool require_intrinsic);
	static void ReleaseHandle(SVGData* handle);

	struct SVGDocument;

	struct SVGGeometry {
		ColourbPremultiplied colour;
		UniquePtr<Geometry> geometry;
	};

	struct SVGTexture : NonCopyMoveable {
		SVGDocument* document = nullptr;
		Vector2i render_dimensions;
		bool crop_to_content = false;
		Colourb current_colour;
		String stroke_width;
		CallbackTexture texture;
		// List of geometries using this texture, one entry for each unique color.
		Vector<SVGGeometry> geometries;
		bool crop_dimensions_known = false;
		Vector2f crop_dimensions;
		bool ready = false;
		lunasvg::Bitmap bitmap;
		AsyncJobOwner job_owner;
	};

	struct SVGDocument {
		SharedPtr<const String> source;
		bool intrinsic_known = false;
		Vector2f intrinsic_dimensions;
		bool colour_known = false;
		bool follows_current_colour = false;
		// List of textures using this document, one entry for each unique render dimension plus meta-data.
		Vector<UniquePtr<SVGTexture>> textures;
	};

	struct SVGCacheData {
		// A list of SVG documents mapped by their path. Owns all geometry and textures needed for rendering.
		UnorderedMap<String, UniquePtr<SVGDocument>> documents;

		// Handles are reference-counted lookup keys and views into the SVG document resources. Handles are responsible
		// for cleaning up the resources in the documents, when nothing refers to them any longer. When a handle is
		// destroyed, it also removes itself from the handle map.
		StableUnorderedMap<SVGKey, WeakPtr<SVGData>> handles;
	};

	static ControlledLifetimeResource<SVGCacheData> svg_cache_data;
	static FileReader file_reader;

	struct ParsedSVG {
		UniquePtr<lunasvg::Document> document;
		bool follows_current_colour = false;
		String file_stroke_width;
		Vector2f intrinsic_dimensions;
	};

	enum class RasterStatus { ReadFailed, ParseFailed, RenderFailed, Rendered };

	struct RasterResult {
		RasterStatus status = RasterStatus::ReadFailed;
		SharedPtr<const String> read_source;
		bool parsed = false;
		bool follows_current_colour = false;
		Vector2f intrinsic_dimensions;
		Vector2f crop_dimensions;
		lunasvg::Bitmap bitmap;
	};

	SVGData::SVGData(Geometry& geometry, Texture texture, SVGTexture& texture_entry, const SVGKey& cache_key) :
		geometry(geometry), texture(texture), texture_entry(texture_entry), cache_key(cache_key)
	{}

	SVGData::~SVGData()
	{
		ReleaseHandle(this);
	}

	bool SVGData::IsReady() const
	{
		return texture_entry.ready;
	}

	bool SVGData::HasZeroSize() const
	{
		return texture_entry.render_dimensions.x <= 0 || texture_entry.render_dimensions.y <= 0;
	}

	bool SVGData::GetIntrinsicDimensions(Vector2f& dimensions) const
	{
		if (texture_entry.crop_to_content)
		{
			if (!texture_entry.crop_dimensions_known)
				return false;
			dimensions = texture_entry.crop_dimensions;
			return true;
		}

		const SVGDocument& document = *texture_entry.document;
		if (!document.intrinsic_known)
			return false;
		dimensions = document.intrinsic_dimensions;
		return true;
	}

	void SetFileReader(FileReader reader)
	{
		file_reader = std::move(reader);
	}

	static Vector<UniquePtr<SVGTexture>>::iterator FindSVGTexture(SVGDocument& doc, Vector2i dimensions, bool crop_to_content, Colourb current_colour,
		const String& stroke_width)
	{
		return std::find_if(doc.textures.begin(), doc.textures.end(), [&](const UniquePtr<SVGTexture>& entry) {
			return entry->render_dimensions == dimensions && entry->crop_to_content == crop_to_content && entry->current_colour == current_colour &&
				entry->stroke_width == stroke_width;
		});
	}

	static bool FindCropDimensions(const SVGDocument& doc, const String& stroke_width, Vector2f& crop_dimensions)
	{
		for (const UniquePtr<SVGTexture>& entry : doc.textures)
		{
			if (entry->crop_to_content && entry->crop_dimensions_known && entry->stroke_width == stroke_width)
			{
				crop_dimensions = entry->crop_dimensions;
				return true;
			}
		}
		return false;
	}

	static Colourb GetKeyColour(const SVGDocument& doc, Vector2i dimensions, bool crop_to_content, Colourb current_colour, const String& stroke_width)
	{
		if (!doc.colour_known || doc.follows_current_colour)
			return current_colour;

		for (const UniquePtr<SVGTexture>& entry : doc.textures)
		{
			if (entry->render_dimensions == dimensions && entry->crop_to_content == crop_to_content && entry->stroke_width == stroke_width)
				return entry->current_colour;
		}
		return Colourb(0, 0, 0, 0);
	}

	// lunasvg resolves currentColor to the inherited 'color' and inherits 'stroke-width' from the root (svglayoutstate.cpp:11,413,420).
	static void ApplyInstanceAttributes(lunasvg::Document* svg_document, bool follows_current_colour, Colourb current_colour,
		const String& stroke_width, const String& file_stroke_width)
	{
		lunasvg::Element root = svg_document->documentElement();
		if (follows_current_colour)
			root.setAttribute("color",
				CreateString("#%02x%02x%02x%02x", current_colour.red, current_colour.green, current_colour.blue, current_colour.alpha));
		root.setAttribute("stroke-width", stroke_width.empty() ? file_stroke_width : stroke_width);
	}

	static Vector<SVGGeometry>::iterator FindSVGGeometry(SVGTexture& per_size_data, const ColourbPremultiplied colour)
	{
		return std::find_if(per_size_data.geometries.begin(), per_size_data.geometries.end(),
			[&](const SVGGeometry& data) { return data.colour == colour; });
	}

	static bool ParseSVG(const String& svg_data, ParsedSVG& parsed)
	{
		// We use a reset-release approach here in case clients use a non-std unique_ptr (lunasvg uses std::unique_ptr). We also use
		// loadFromData(char*, size_t) instead of loadFromData(std::string) in case clients use a non-std string.
		parsed.document.reset(lunasvg::Document::loadFromData(svg_data.data(), svg_data.size()).release());
		if (!parsed.document)
			return false;

		const lunasvg::Element root = parsed.document->documentElement();
		parsed.follows_current_colour = (svg_data.find("currentColor") != String::npos && !root.hasAttribute("color"));
		parsed.file_stroke_width = root.getAttribute("stroke-width");
		parsed.intrinsic_dimensions.x = Math::Max(float(parsed.document->width()), 1.0f);
		parsed.intrinsic_dimensions.y = Math::Max(float(parsed.document->height()), 1.0f);
		return true;
	}

	static Vector2f GetCropDimensions(const ParsedSVG& parsed, Colourb current_colour, const String& stroke_width)
	{
		ApplyInstanceAttributes(parsed.document.get(), parsed.follows_current_colour, current_colour, stroke_width, parsed.file_stroke_width);
		const lunasvg::Box smallest_fit = parsed.document->boundingBox();
		return Vector2f(static_cast<float>(smallest_fit.w), static_cast<float>(smallest_fit.h));
	}

	static bool ParseIntrinsic(SVGDocument& doc, const String& source_id, bool crop_to_content, Colourb current_colour, const String& stroke_width,
		Vector2f& crop_dimensions)
	{
		if (!doc.source)
		{
			String file_data;
			if (source_id.empty() || !file_reader || !file_reader(source_id, file_data))
			{
				Log::Message(Rml::Log::Type::LT_WARNING, "Could not load SVG file %s", source_id.c_str());
				return false;
			}
			doc.source = MakeShared<const String>(std::move(file_data));
		}

		RMLUI_ARC_PERF_COUNT(SvgDocumentsParsed, 1);
		ParsedSVG parsed;
		if (!ParseSVG(*doc.source, parsed))
		{
			Log::Message(Rml::Log::Type::LT_WARNING, "Could not load SVG data for item %s", source_id.c_str());
			return false;
		}

		doc.intrinsic_known = true;
		doc.intrinsic_dimensions = parsed.intrinsic_dimensions;
		doc.colour_known = true;
		doc.follows_current_colour = parsed.follows_current_colour;
		if (crop_to_content)
			crop_dimensions = GetCropDimensions(parsed, current_colour, stroke_width);
		return true;
	}

	static void Rasterize(const String& source_id, const SharedPtr<const String>& source, Vector2i dimensions, bool crop_to_content,
		Colourb current_colour, const String& stroke_width, RasterResult& result)
	{
		const String* svg_data = source.get();
		if (!svg_data)
		{
			String file_data;
			if (source_id.empty() || !file_reader || !file_reader(source_id, file_data))
			{
				result.status = RasterStatus::ReadFailed;
				return;
			}
			result.read_source = MakeShared<const String>(std::move(file_data));
			svg_data = result.read_source.get();
		}

		ParsedSVG parsed;
		if (!ParseSVG(*svg_data, parsed))
		{
			result.status = RasterStatus::ParseFailed;
			return;
		}
		result.parsed = true;
		result.follows_current_colour = parsed.follows_current_colour;
		result.intrinsic_dimensions = parsed.intrinsic_dimensions;

		lunasvg::Document& svg_document = *parsed.document;
		ApplyInstanceAttributes(&svg_document, parsed.follows_current_colour, current_colour, stroke_width, parsed.file_stroke_width);
		lunasvg::Bitmap bitmap;
		if (crop_to_content)
		{
			const lunasvg::Box smallest_fit = svg_document.boundingBox();
			result.crop_dimensions = Vector2f(static_cast<float>(smallest_fit.w), static_cast<float>(smallest_fit.h));

			lunasvg::Matrix matrix(dimensions.x / svg_document.width(), 0, 0, dimensions.y / svg_document.height(), 0, 0);
			matrix.scale(svg_document.width() / smallest_fit.w, svg_document.height() / smallest_fit.h);
			matrix.translate(-smallest_fit.x, -smallest_fit.y);

			bitmap = lunasvg::Bitmap(dimensions.x, dimensions.y);
			bitmap.clear(0x00000000);
			svg_document.render(bitmap, matrix);
		}
		else
		{
			bitmap = svg_document.renderToBitmap(dimensions.x, dimensions.y);
		}

		if (!bitmap.valid() || !bitmap.data())
		{
			result.status = RasterStatus::RenderFailed;
			return;
		}

		// Swap red and blue channels, assuming LunaSVG v2.3.2 or newer, to convert to RmlUi's expected RGBA-ordering.
		const size_t bitmap_byte_size = bitmap.width() * bitmap.height() * 4;
		uint8_t* bitmap_data = bitmap.data();
		for (size_t i = 0; i < bitmap_byte_size; i += 4)
			std::swap(bitmap_data[i], bitmap_data[i + 2]);

		result.bitmap = std::move(bitmap);
		result.status = RasterStatus::Rendered;
	}

	static void InstallRaster(SVGTexture& entry, const String& source_id, RasterResult& result)
	{
		RMLUI_ARC_PERF_SCOPE(SvgRasterize);
		SVGDocument& doc = *entry.document;
		if (result.read_source && !doc.source)
			doc.source = std::move(result.read_source);

		if (result.parsed)
		{
			RMLUI_ARC_PERF_COUNT(SvgDocumentsParsed, 1);
			if (!doc.intrinsic_known)
			{
				doc.intrinsic_known = true;
				doc.intrinsic_dimensions = result.intrinsic_dimensions;
			}
			if (!doc.colour_known)
			{
				doc.colour_known = true;
				doc.follows_current_colour = result.follows_current_colour;
			}
			if (entry.crop_to_content && !entry.crop_dimensions_known)
			{
				entry.crop_dimensions_known = true;
				entry.crop_dimensions = result.crop_dimensions;
			}
		}

		switch (result.status)
		{
		case RasterStatus::ReadFailed: Log::Message(Rml::Log::Type::LT_WARNING, "Could not load SVG file %s", source_id.c_str()); return;
		case RasterStatus::ParseFailed: Log::Message(Rml::Log::Type::LT_WARNING, "Could not load SVG data for item %s", source_id.c_str()); return;
		case RasterStatus::RenderFailed: Log::Message(Rml::Log::Type::LT_WARNING, "Could not render SVG to bitmap: %s", source_id.c_str()); return;
		case RasterStatus::Rendered: break;
		}

		RMLUI_ARC_PERF_COUNT(SvgRasterizations, 1);
		RMLUI_SVG_DEBUG_LOG("Installing texture: %s, (%d, %d), %s", source_id.c_str(), entry.render_dimensions.x, entry.render_dimensions.y,
			entry.crop_to_content ? "crop_to_content" : "crop_none");
		entry.bitmap = std::move(result.bitmap);
		entry.ready = true;
	}

	static void LaunchRasterJob(SVGTexture& entry, const String& source_id)
	{
		SVGTexture* entry_pointer = &entry;
		entry.job_owner.Launch([entry_pointer, source_id, source = entry.document->source, dimensions = entry.render_dimensions,
								   crop_to_content = entry.crop_to_content, current_colour = entry.current_colour,
								   stroke_width = entry.stroke_width]() -> AsyncInstall {
			SharedPtr<RasterResult> result = MakeShared<RasterResult>();
			Rasterize(source_id, source, dimensions, crop_to_content, current_colour, stroke_width, *result);
			return [entry_pointer, source_id, result]() { InstallRaster(*entry_pointer, source_id, *result); };
		});
	}

	static SharedPtr<SVGData> GetHandle(RenderManager& render_manager, String source_id, const String* source_data, const Vector2i dimensions,
		const bool crop_to_content, const ColourbPremultiplied colour, const Colourb current_colour, const String& stroke_width,
		const bool require_intrinsic)
	{
		UnorderedMap<String, UniquePtr<SVGDocument>>& documents = svg_cache_data->documents;
		StableUnorderedMap<SVGKey, WeakPtr<SVGData>>& handles = svg_cache_data->handles;

		// Find or create a document
		UnorderedMap<String, UniquePtr<SVGDocument>>::iterator it_svg_document = documents.find(source_id);
		const bool document_created = (it_svg_document == documents.end());
		if (document_created)
		{
			RMLUI_SVG_DEBUG_LOG("Creating SVG document %s", source_id.c_str());
			UniquePtr<SVGDocument> created_document = MakeUnique<SVGDocument>();
			if (source_data)
				created_document->source = MakeShared<const String>(*source_data);
			it_svg_document = documents.emplace(source_id, std::move(created_document)).first;
		}

		SVGDocument& doc = *it_svg_document->second;

		Vector2f crop_dimensions;
		bool crop_dimensions_known = crop_to_content && FindCropDimensions(doc, stroke_width, crop_dimensions);
		if (require_intrinsic && (!doc.intrinsic_known || (crop_to_content && !crop_dimensions_known)))
		{
			if (!ParseIntrinsic(doc, source_id, crop_to_content, current_colour, stroke_width, crop_dimensions))
			{
				if (document_created)
					documents.erase(it_svg_document);
				return {};
			}
			crop_dimensions_known = crop_to_content;
			for (const UniquePtr<SVGTexture>& entry : doc.textures)
			{
				if (crop_dimensions_known && entry->crop_to_content && !entry->crop_dimensions_known && entry->stroke_width == stroke_width)
				{
					entry->crop_dimensions_known = true;
					entry->crop_dimensions = crop_dimensions;
				}
			}
		}

		const Colourb key_current_colour = GetKeyColour(doc, dimensions, crop_to_content, current_colour, stroke_width);
		SVGKey key{std::move(source_id), dimensions, crop_to_content, colour, key_current_colour, stroke_width};

		if (!document_created)
		{
			const StableUnorderedMap<SVGKey, WeakPtr<SVGData>>::iterator it_handle = handles.find(key);
			if (it_handle != handles.end())
			{
				RMLUI_SVG_DEBUG_LOG("Found handle, reusing: %s, (%d, %d), %s, %#x", key.source_id.c_str(), dimensions.x, dimensions.y,
					crop_to_content ? "crop_to_content" : "crop_none", *reinterpret_cast<const uint32_t*>(&colour[0]));
				SharedPtr<SVGData> result = it_handle->second.lock();
				RMLUI_ASSERTMSG(result, "Failed to lock handle in SVG cache");
				return result;
			}
		}

		RMLUI_SVG_DEBUG_LOG("Making new handle: %s, (%d, %d), %s, %#x", key.source_id.c_str(), dimensions.x, dimensions.y,
			crop_to_content ? "crop_to_content" : "crop_none", *reinterpret_cast<const uint32_t*>(&colour[0]));

		// Find or create texture
		Vector<UniquePtr<SVGTexture>>::iterator it_size = FindSVGTexture(doc, dimensions, crop_to_content, key.current_colour, key.stroke_width);
		if (it_size == doc.textures.end())
		{
			RMLUI_SVG_DEBUG_LOG("Creating per-size data for (%d, %d), %s", dimensions.x, dimensions.y,
				crop_to_content ? "crop_to_content" : "crop_none");
			UniquePtr<SVGTexture> created_texture = MakeUnique<SVGTexture>();
			SVGTexture& svg_texture = *created_texture;
			svg_texture.document = &doc;
			svg_texture.render_dimensions = dimensions;
			svg_texture.crop_to_content = crop_to_content;
			svg_texture.current_colour = key.current_colour;
			svg_texture.stroke_width = key.stroke_width;
			svg_texture.crop_dimensions_known = crop_dimensions_known;
			svg_texture.crop_dimensions = crop_dimensions;

			SVGTexture* texture_pointer = &svg_texture;
			CallbackTextureFunction texture_callback = [texture_pointer](const CallbackTextureInterface& texture_interface) -> bool {
				RMLUI_ARC_PERF_SCOPE(SvgUpload);
				// A callback that returns false latches the texture as failed (Source/Core/TextureDatabase.cpp:55), so only ready textures are drawn.
				RMLUI_ASSERT(texture_pointer->ready);
				const lunasvg::Bitmap& bitmap = texture_pointer->bitmap;
				const size_t bitmap_byte_size = bitmap.width() * bitmap.height() * 4;
				if (!texture_interface.GenerateTexture({reinterpret_cast<const Rml::byte*>(bitmap.data()), bitmap_byte_size},
						Vector2i{bitmap.width(), bitmap.height()}))
				{
					Log::Message(Rml::Log::Type::LT_WARNING, "Could not generate texture for SVG (%d, %d)", bitmap.width(), bitmap.height());
					return false;
				}
				return true;
			};
			svg_texture.texture = render_manager.MakeCallbackTexture(std::move(texture_callback));

			doc.textures.push_back(std::move(created_texture));
			it_size = std::prev(doc.textures.end());

			if (dimensions.x > 0 && dimensions.y > 0)
				LaunchRasterJob(svg_texture, key.source_id);
		}

		// Construct and insert per-color geometry
		SVGTexture& size_data = **it_size;
		RMLUI_ASSERTMSG(FindSVGGeometry(size_data, colour) == size_data.geometries.end(),
			"We found an existing color entry in the SVG document cache, this should have been found as a cache key map entry instead.");
		SVGGeometry colour_data;
		colour_data.colour = colour;
		Mesh mesh;
		MeshUtilities::GenerateQuad(mesh, Vector2f(0), Vector2f(size_data.render_dimensions), colour, Vector2f(0), Vector2f(1));
		colour_data.geometry = MakeUnique<Geometry>(render_manager.MakeGeometry(std::move(mesh)));
		size_data.geometries.push_back(std::move(colour_data));

		// Create and insert the handle
		const std::pair<StableUnorderedMap<SVGKey, WeakPtr<SVGData>>::iterator, bool> iterator_inserted =
			handles.emplace(std::move(key), WeakPtr<SVGData>());
		RMLUI_ASSERTMSG(iterator_inserted.second, "Could not insert entry into the SVG cache handle map, duplicate key.");
		const SVGKey& inserted_key = iterator_inserted.first->first;
		WeakPtr<SVGData>& inserted_weak_data_pointer = iterator_inserted.first->second;

		SharedPtr<SVGData> svg_handle = MakeShared<SVGData>(*size_data.geometries.back().geometry.get(), size_data.texture, size_data, inserted_key);
		inserted_weak_data_pointer = svg_handle;

		return svg_handle;
	}

	static void ReleaseHandle(SVGData* handle)
	{
		// There are no longer any users of the cache entry uniquely identified by the handle address. Start from the
		// tip (i.e. per-color data) and remove that entry from its parent. Move up the cache ancestry and erase any
		// entries that no longer have any children.
		UnorderedMap<String, UniquePtr<SVGDocument>>& documents = svg_cache_data->documents;
		StableUnorderedMap<SVGKey, WeakPtr<SVGData>>& handles = svg_cache_data->handles;
		const SVGKey& key = handle->cache_key;

		const StableUnorderedMap<SVGKey, WeakPtr<SVGData>>::iterator it_handle = handles.find(key);
		RMLUI_ASSERT(it_handle != handles.cend());

		const UnorderedMap<String, UniquePtr<SVGDocument>>::iterator it_document = documents.find(key.source_id);
		RMLUI_ASSERT(it_document != documents.cend());
		SVGDocument& svg_document = *it_document->second;

		RMLUI_SVG_DEBUG_LOG("Releasing handle: %s, (%d, %d), %s, %#x", key.source_id.c_str(), key.dimensions.x, key.dimensions.y,
			key.crop_to_content ? "crop_to_content" : "crop_none", *reinterpret_cast<const uint32_t*>(&key.colour[0]));

		const Vector<UniquePtr<SVGTexture>>::iterator it_texture = std::find_if(svg_document.textures.begin(), svg_document.textures.end(),
			[handle](const UniquePtr<SVGTexture>& entry) { return entry.get() == &handle->texture_entry; });
		RMLUI_ASSERT(it_texture != svg_document.textures.end());
		SVGTexture& svg_texture = **it_texture;

		const Vector<SVGGeometry>::iterator it_geometry = FindSVGGeometry(svg_texture, key.colour);
		RMLUI_ASSERT(it_geometry != svg_texture.geometries.end());

		if (svg_texture.geometries.size() > 1)
		{
			RMLUI_SVG_DEBUG_LOG("Releasing handle from geometries, size: %zu", svg_texture.geometries.size());
			std::iter_swap(it_geometry, std::prev(svg_texture.geometries.end()));
			svg_texture.geometries.pop_back();
		}
		else if (svg_document.textures.size() > 1)
		{
			RMLUI_SVG_DEBUG_LOG("Releasing handle from textures, size: %zu", svg_document.textures.size());
			std::iter_swap(it_texture, std::prev(svg_document.textures.end()));
			svg_document.textures.pop_back();
		}
		else
		{
			RMLUI_SVG_DEBUG_LOG("Releasing document");
			documents.erase(it_document);
		}

		handles.erase(it_handle);

#ifdef RMLUI_DEBUG
		size_t count_unique_entries = 0;
		for (const std::pair<const String, UniquePtr<SVGDocument>>& document : documents)
		{
			RMLUI_ASSERT(!document.second->textures.empty());
			for (const UniquePtr<SVGTexture>& size_data : document.second->textures)
			{
				RMLUI_ASSERT(!size_data->geometries.empty());
				count_unique_entries += size_data->geometries.size();
			}
		}
		RMLUI_ASSERT(count_unique_entries == handles.size());
#endif
	}

	void SVGCache::Initialize()
	{
		svg_cache_data.Initialize();
	}

	void SVGCache::Shutdown()
	{
		svg_cache_data.Shutdown();
		file_reader = nullptr;
	}

	SharedPtr<SVGData> SVGCache::GetHandle(const String& source_id, const String& source, SourceType source_type, Element* element,
		const bool crop_to_content, Vector2f render_size, const String& stroke_width, const bool require_intrinsic)
	{
		RMLUI_ARC_PERF_ELEMENT_SCOPE(SvgHandle, element, Svg);
		RMLUI_ARC_PERF_COUNT(SvgHandleLookups, 1);
		RenderManager* render_manager = element->GetRenderManager();
		if (!render_manager)
			return {};

		const ComputedValues& computed = element->GetComputedValues();
		const ColourbPremultiplied colour = computed.image_color().ToPremultiplied(computed.opacity());
		const Colourb current_colour = computed.color();
		Vector2i dimensions(render_size.Round());
		if (dimensions.x <= 0 || dimensions.y <= 0)
			dimensions = {0, 0};

		if (source_type == File)
		{
			String path;
			if (ElementDocument* document = element->GetOwnerDocument())
			{
				const String document_source_url = StringUtilities::Replace(document->GetSourceURL(), '|', ':');
				GetSystemInterface()->JoinPath(path, document_source_url, source_id);
			}
			return Rml::SVG::GetHandle(*render_manager, std::move(path), nullptr, dimensions, crop_to_content, colour, current_colour, stroke_width,
				require_intrinsic);
		}

		return Rml::SVG::GetHandle(*render_manager, source_id, &source, dimensions, crop_to_content, colour, current_colour, stroke_width,
			require_intrinsic);
	}

} // namespace SVG
} // namespace Rml
