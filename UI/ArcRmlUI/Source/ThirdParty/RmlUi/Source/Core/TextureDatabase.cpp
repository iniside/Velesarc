#include "TextureDatabase.h"
#include "ArcPerfInternal.h"
#include "../../Include/RmlUi/Core/Log.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "../../Include/RmlUi/Core/RenderInterface.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "../../Include/RmlUi/Core/TextureFileDecoder.h"

namespace Rml {

CallbackTextureDatabase::CallbackTextureDatabase()
{
	constexpr size_t reserve_callback_textures = 30;
	texture_list.reserve(reserve_callback_textures);
}

CallbackTextureDatabase::~CallbackTextureDatabase()
{
	if (!texture_list.empty())
	{
		Log::Message(Log::LT_ERROR, "TextureDatabase destroyed with outstanding callback textures. Will likely result in memory corruption.");
		RMLUI_ERROR;
	}
}

StableVectorIndex CallbackTextureDatabase::CreateTexture(CallbackTextureFunction&& callback)
{
	RMLUI_ASSERT(callback);
	return texture_list.insert(CallbackTextureEntry{std::move(callback), TextureHandle(), Vector2i()});
}

void CallbackTextureDatabase::ReleaseTexture(RenderInterface* render_interface, StableVectorIndex callback_index)
{
	CallbackTextureEntry& data = texture_list[callback_index];
	if (data.texture_handle)
		render_interface->ReleaseTexture(data.texture_handle);
	texture_list.erase(callback_index);
}

Vector2i CallbackTextureDatabase::GetDimensions(RenderManager* render_manager, RenderInterface* render_interface, StableVectorIndex callback_index)
{
	return EnsureLoaded(render_manager, render_interface, callback_index).dimensions;
}

TextureHandle CallbackTextureDatabase::GetHandle(RenderManager* render_manager, RenderInterface* render_interface, StableVectorIndex callback_index)
{
	return EnsureLoaded(render_manager, render_interface, callback_index).texture_handle;
}

auto CallbackTextureDatabase::EnsureLoaded(RenderManager* render_manager, RenderInterface* render_interface, StableVectorIndex callback_index)
	-> CallbackTextureEntry&
{
	CallbackTextureEntry& data = texture_list[callback_index];
	if (!data.texture_handle && !data.load_failed)
	{
		if (!data.callback(CallbackTextureInterface(*render_manager, *render_interface, data.texture_handle, data.dimensions)))
		{
			data.load_failed = true;
			data.texture_handle = {};
			data.dimensions = {};
		}
	}
	return data;
}

size_t CallbackTextureDatabase::size() const
{
	return texture_list.size();
}

void CallbackTextureDatabase::ReleaseAllTextures(RenderInterface* render_interface)
{
	texture_list.for_each([render_interface](CallbackTextureEntry& texture) {
		if (texture.texture_handle)
		{
			render_interface->ReleaseTexture(texture.texture_handle);
			texture.texture_handle = {};
			texture.dimensions = {};
		}
	});
}

static TextureFileDecoder texture_file_decoder;

void SetTextureFileDecoder(TextureFileDecoder decoder)
{
	texture_file_decoder = std::move(decoder);
}

static const StringView asset_source_prefix = "asset:";
static AssetTextureRequester asset_texture_requester;

bool IsAssetTextureSource(const String& source)
{
	return StringUtilities::StartsWith(source, asset_source_prefix);
}

void SetAssetTextureRequester(AssetTextureRequester requester)
{
	asset_texture_requester = std::move(requester);
}

FileTextureDatabase::FileTextureDatabase() {}

FileTextureDatabase::~FileTextureDatabase()
{
#ifdef RMLUI_DEBUG
	for (const FileTextureEntry& texture : texture_list)
	{
		RMLUI_ASSERTMSG(!texture.texture_handle,
			"TextureDatabase destroyed without releasing all file textures first. Ensure that 'ReleaseAllTextures' is called before destruction.");
	}
#endif
}

TextureFileIndex FileTextureDatabase::InsertTexture(const String& source)
{
	auto it = texture_map.find(source);
	if (it != texture_map.end())
		return it->second;

	const TextureFileIndex index = TextureFileIndex(texture_list.size());
	texture_map[source] = index;
	FileTextureEntry& entry = texture_list.emplace_back();
	entry.source = source;
	entry.asset = IsAssetTextureSource(source);

	return index;
}

FileTextureDatabase::FileTextureEntry& FileTextureDatabase::EnsureRequested(TextureFileIndex index)
{
	RMLUI_ASSERT(size_t(index) < texture_list.size());
	FileTextureEntry& entry = texture_list[size_t(index)];
	if (entry.state != LoadState::Unrequested)
		return entry;

	entry.state = LoadState::Pending;
	if (entry.asset)
	{
		if (asset_texture_requester)
		{
			asset_texture_requester(entry.source.substr(asset_source_prefix.size()), AssetTextureRequest{index, entry.generation});
		}
		else
		{
			entry.state = LoadState::Failed;
			Log::Message(Log::LT_WARNING, "Could not load texture: %s", entry.source.c_str());
		}
		return entry;
	}

	job_owner.Launch([this, list_index = size_t(index), generation = entry.generation, source = entry.source,
						 decoder = texture_file_decoder]() -> AsyncInstall {
		SharedPtr<DecodeResult> result = MakeShared<DecodeResult>();
		result->decoded = decoder && decoder(source, result->dimensions, result->pixels);
		return [this, list_index, generation, result]() { InstallDecode(list_index, generation, *result); };
	});
	return entry;
}

void FileTextureDatabase::InstallDecode(size_t index, unsigned int generation, DecodeResult& result)
{
	RMLUI_ARC_PERF_SCOPE(ImageInstall);
	FileTextureEntry& entry = texture_list[index];
	if (entry.generation != generation || entry.state != LoadState::Pending)
		return;

	const size_t expected_size = size_t(Math::Max(result.dimensions.x, 0)) * size_t(Math::Max(result.dimensions.y, 0)) * 4;
	if (!result.decoded || expected_size == 0 || result.pixels.size() != expected_size)
	{
		entry.state = LoadState::Failed;
		Log::Message(Log::LT_WARNING, "Could not load texture: %s", entry.source.c_str());
		return;
	}

	entry.state = LoadState::Ready;
	entry.dimensions = result.dimensions;
	entry.pixels = std::move(result.pixels);
}

void FileTextureDatabase::InstallAsset(AssetTextureRequest request, bool loaded, Vector2i dimensions, AssetTextureToken token)
{
	RMLUI_ARC_PERF_SCOPE(ImageInstall);
	if (size_t(request.index) >= texture_list.size())
		return;

	FileTextureEntry& entry = texture_list[size_t(request.index)];
	if (!entry.asset || entry.generation != request.generation || entry.state != LoadState::Pending)
		return;

	if (!loaded || dimensions.x <= 0 || dimensions.y <= 0 || !token)
	{
		entry.state = LoadState::Failed;
		Log::Message(Log::LT_WARNING, "Could not load texture: %s", entry.source.c_str());
		return;
	}

	entry.state = LoadState::Ready;
	entry.dimensions = dimensions;
	entry.asset_token = token;
}

bool FileTextureDatabase::IsReady(TextureFileIndex index)
{
	const LoadState state = EnsureRequested(index).state;
	return state == LoadState::Ready || state == LoadState::Failed;
}

TextureHandle FileTextureDatabase::GetHandle(RenderInterface* render_interface, TextureFileIndex index)
{
	RMLUI_ASSERT(size_t(index) < texture_list.size());
	FileTextureEntry& entry = texture_list[size_t(index)];
	// RenderManager::Render draws only ready textures, so the pixels are installed before the first upload.
	RMLUI_ASSERT(entry.state == LoadState::Ready || entry.state == LoadState::Failed);
	if (entry.state == LoadState::Ready && !entry.texture_handle)
	{
		RMLUI_ARC_PERF_SCOPE(ImageUpload);
		RMLUI_ARC_PERF_COUNT(TexturesImage, 1);
		entry.texture_handle = entry.asset ? render_interface->GenerateAssetTexture(entry.asset_token, entry.dimensions)
										   : render_interface->GenerateTexture(entry.pixels, entry.dimensions);
		entry.pixels = Vector<byte>();
		if (!entry.texture_handle)
		{
			entry.state = LoadState::Failed;
			Log::Message(Log::LT_WARNING, "Could not load texture: %s", entry.source.c_str());
		}
	}
	return entry.texture_handle;
}

Vector2i FileTextureDatabase::GetDimensions(TextureFileIndex index)
{
	return EnsureRequested(index).dimensions;
}

void FileTextureDatabase::GetSourceList(StringList& source_list) const
{
	source_list.reserve(source_list.size() + texture_list.size());
	for (const auto& texture : texture_map)
		source_list.push_back(texture.first);
}

void FileTextureDatabase::ReleaseEntry(RenderInterface* render_interface, FileTextureEntry& entry)
{
	render_interface->ReleaseTexture(entry.texture_handle);
	entry.texture_handle = {};
	entry.state = LoadState::Unrequested;
	entry.dimensions = {};
	entry.asset_token = 0;
	entry.generation += 1;
}

bool FileTextureDatabase::ReleaseTexture(RenderInterface* render_interface, const String& source)
{
	auto it = texture_map.find(source);
	if (it == texture_map.end())
		return false;

	FileTextureEntry& texture = texture_list[size_t(it->second)];
	if (texture.texture_handle)
	{
		ReleaseEntry(render_interface, texture);
		return true;
	}

	return false;
}

void FileTextureDatabase::ReleaseAllTextures(RenderInterface* render_interface)
{
	for (FileTextureEntry& texture : texture_list)
	{
		if (texture.texture_handle)
			ReleaseEntry(render_interface, texture);
	}
}

} // namespace Rml
