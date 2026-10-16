#include "../../Include/RmlUi/Core/Decorator.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/RenderManager.h"
#include "../../Include/RmlUi/Core/StyleSheet.h"
#include "../../Include/RmlUi/Core/Texture.h"
#include <algorithm>

namespace Rml {

Decorator::Decorator() {}

Decorator::~Decorator() {}

DecoratorDataHandle Decorator::GenerateLayerElementData(Element* element, BoxArea paint_area, const DecoratorLayer& layer) const
{
	const bool inner_origin = (layer.origin != BoxArea::Auto && layer.origin > paint_area);
	return GenerateElementData(element, inner_origin ? layer.origin : paint_area);
}

bool Decorator::AreTexturesReady() const
{
	if (!first_texture.IsReady())
		return false;
	for (const Texture& texture : additional_textures)
	{
		if (!texture.IsReady())
			return false;
	}
	return true;
}

int Decorator::AddTexture(Texture texture)
{
	if (!texture)
		return -1;

	if (!first_texture)
		first_texture = texture;

	if (first_texture == texture)
		return 0;

	auto it = std::find(additional_textures.begin(), additional_textures.end(), texture);
	if (it != additional_textures.end())
		return (int)(it - additional_textures.begin()) + 1;

	additional_textures.push_back(texture);
	return (int)additional_textures.size();
}

int Decorator::GetNumTextures() const
{
	int result = (first_texture ? 1 : 0);
	result += (int)additional_textures.size();
	return result;
}

Texture Decorator::GetTexture(int index) const
{
	if (index == 0)
		return first_texture;

	index -= 1;
	if (index < 0 || index >= (int)additional_textures.size())
		return {};

	return additional_textures[index];
}

DecoratorInstancer::DecoratorInstancer() {}

DecoratorInstancer::~DecoratorInstancer() {}

const Sprite* DecoratorInstancerInterface::GetSprite(const String& name) const
{
	return style_sheet.GetSprite(name);
}

Texture DecoratorInstancerInterface::GetTexture(const String& filename) const
{
	if (!property_source)
	{
		Log::Message(Log::LT_WARNING, "Texture name '%s' in decorator could not be loaded, no property source available.", filename.c_str());
		return {};
	}

	return render_manager.LoadTexture(filename, property_source->path);
}

RenderManager& DecoratorInstancerInterface::GetRenderManager() const
{
	return render_manager;
}

} // namespace Rml
