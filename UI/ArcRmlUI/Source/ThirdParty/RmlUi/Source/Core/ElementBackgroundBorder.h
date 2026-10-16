#pragma once

#include "../../Include/RmlUi/Core/CallbackTexture.h"
#include "../../Include/RmlUi/Core/Geometry.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

struct BoxShadowRenderable;

class ElementBackgroundBorder {
public:
	ElementBackgroundBorder();
	void RenderBackground(Element* element);
	void RenderBorder(Element* element);

	void DirtyBackground();
	void DirtyBorder();

	Geometry* GetClipGeometry(Element* element, BoxArea clip_area);

private:
	enum class BackgroundType { Background, Border, BoxShadow, ClipBorder, ClipPadding, ClipContent, Count };
	struct Background {
		Geometry geometry;
		SharedPtr<BoxShadowRenderable> box_shadow;
	};

	Background* GetBackground(BackgroundType type);
	Background& GetOrCreateBackground(BackgroundType type);
	void EraseBackground(BackgroundType type);

	void GenerateGeometry(Element* element);

	bool background_dirty = false;
	bool border_dirty = false;

	StableMap<BackgroundType, Background> backgrounds;
};

} // namespace Rml
