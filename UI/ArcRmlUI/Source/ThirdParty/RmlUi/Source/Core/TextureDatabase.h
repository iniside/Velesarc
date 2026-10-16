#pragma once

#include "../../Include/RmlUi/Core/AssetTexture.h"
#include "../../Include/RmlUi/Core/AsyncJobInterface.h"
#include "../../Include/RmlUi/Core/CallbackTexture.h"
#include "../../Include/RmlUi/Core/StableVector.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

class RenderInterface;

class CallbackTextureDatabase : NonCopyMoveable {
public:
	CallbackTextureDatabase();
	~CallbackTextureDatabase();

	StableVectorIndex CreateTexture(CallbackTextureFunction&& callback);
	void ReleaseTexture(RenderInterface* render_interface, StableVectorIndex callback_index);

	Vector2i GetDimensions(RenderManager* render_manager, RenderInterface* render_interface, StableVectorIndex callback_index);
	TextureHandle GetHandle(RenderManager* render_manager, RenderInterface* render_interface, StableVectorIndex callback_index);

	size_t size() const;

	void ReleaseAllTextures(RenderInterface* render_interface);

private:
	struct CallbackTextureEntry {
		CallbackTextureFunction callback;
		TextureHandle texture_handle = {};
		Vector2i dimensions;
		bool load_failed = false;
	};

	CallbackTextureEntry& EnsureLoaded(RenderManager* render_manager, RenderInterface* render_interface, StableVectorIndex callback_index);

	StableVector<CallbackTextureEntry> texture_list;
};

class FileTextureDatabase : NonCopyMoveable {
public:
	FileTextureDatabase();
	~FileTextureDatabase();

	TextureFileIndex InsertTexture(const String& source);

	bool IsReady(TextureFileIndex index);
	TextureHandle GetHandle(RenderInterface* render_interface, TextureFileIndex index);
	Vector2i GetDimensions(TextureFileIndex index);

	void GetSourceList(StringList& source_list) const;

	bool ReleaseTexture(RenderInterface* render_interface, const String& source);
	void ReleaseAllTextures(RenderInterface* render_interface);

	void InstallAsset(AssetTextureRequest request, bool loaded, Vector2i dimensions, AssetTextureToken token);

private:
	enum class LoadState : uint8_t { Unrequested, Pending, Ready, Failed };

	struct FileTextureEntry {
		String source;
		bool asset = false;
		LoadState state = LoadState::Unrequested;
		unsigned int generation = 0;
		Vector2i dimensions;
		Vector<byte> pixels;
		AssetTextureToken asset_token = 0;
		TextureHandle texture_handle = {};
	};

	struct DecodeResult {
		bool decoded = false;
		Vector2i dimensions;
		Vector<byte> pixels;
	};

	FileTextureEntry& EnsureRequested(TextureFileIndex index);
	void InstallDecode(size_t index, unsigned int generation, DecodeResult& result);
	void ReleaseEntry(RenderInterface* render_interface, FileTextureEntry& entry);

	Vector<FileTextureEntry> texture_list;
	UnorderedMap<String, TextureFileIndex> texture_map; // key: source, value: index into 'texture_list'
	AsyncJobOwner job_owner;
};

class TextureDatabase {
public:
	FileTextureDatabase file_database;
	CallbackTextureDatabase callback_database;
};

} // namespace Rml
