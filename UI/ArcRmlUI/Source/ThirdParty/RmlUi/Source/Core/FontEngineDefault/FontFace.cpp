#include "FontFace.h"
#include "../ArcPerfInternal.h"
#include "../../../Include/RmlUi/Core/Log.h"
#include "FontFaceHandleDefault.h"
#include "FreeTypeInterface.h"

namespace Rml {

FontFace::FontFace(FontFaceHandleFreetype _face, Span<const byte> _face_data, int _face_index, Style::FontStyle _style, Style::FontWeight _weight)
{
	style = _style;
	weight = _weight;
	face = _face;
	face_data = _face_data;
	face_index = _face_index;
}

FontFace::~FontFace()
{
	if (face)
		FreeType::ReleaseFace(face);
}

Style::FontStyle FontFace::GetStyle() const
{
	return style;
}

Style::FontWeight FontFace::GetWeight() const
{
	return weight;
}

FontFaceHandleDefault* FontFace::GetHandle(int size)
{
	auto it = handles.find(size);
	if (it != handles.end())
		return it->second.get();

	// See if this face has been released.
	if (!face)
	{
		Log::Message(Log::LT_WARNING, "Font face has been released, unable to generate new handle.");
		return nullptr;
	}

	RMLUI_ARC_PERF_SCOPE(StyleFontFaceCreate);
	RMLUI_ARC_PERF_COUNT(FontHandleCreations, 1);
	// Construct and initialise the new handle.
	auto handle = MakeUnique<FontFaceHandleDefault>();
	if (!handle->Initialize(face, face_data, face_index, size))
	{
		handles[size] = nullptr;
		return nullptr;
	}

	FontFaceHandleDefault* result = handle.get();

	// Save the new handle to the font face
	handles[size] = std::move(handle);

	return result;
}

void FontFace::ReleaseFontResources()
{
	HandleMap().swap(handles);
}

} // namespace Rml
