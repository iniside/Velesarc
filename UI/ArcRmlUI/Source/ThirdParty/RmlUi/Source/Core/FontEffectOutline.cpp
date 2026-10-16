#include "FontEffectOutline.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"

namespace Rml {

FontEffectOutline::FontEffectOutline()
{
	width = 0.f;
	radius = 0;
	SetLayer(Layer::Back);
}

FontEffectOutline::~FontEffectOutline() {}

bool FontEffectOutline::HasUniqueTexture() const
{
	return true;
}

bool FontEffectOutline::Initialise(float _width)
{
	if (_width <= 0.f)
		return false;

	width = _width;
	radius = (int)Math::RoundUp(width);

	filter.Initialise(radius, FilterOperation::Dilation);
	for (int x = -radius; x <= radius; ++x)
	{
		for (int y = -radius; y <= radius; ++y)
		{
			float weight = 1;

			float distance = Math::SquareRoot(float(x * x + y * y));
			if (distance > width)
			{
				weight = (width + 1) - distance;
				weight = Math::Max(weight, 0.0f);
			}

			filter[x + radius][y + radius] = weight;
		}
	}

	return true;
}

bool FontEffectOutline::GetGlyphMetrics(Vector2i& origin, Vector2i& dimensions, const FontGlyph& glyph) const
{
	if (glyph.bitmap_data && dimensions.x * dimensions.y > 0)
	{
		origin.x -= radius;
		origin.y -= radius;

		dimensions.x += 2 * radius;
		dimensions.y += 2 * radius;

		return true;
	}

	return false;
}

void FontEffectOutline::GenerateGlyphTexture(byte* destination_data, const Vector2i destination_dimensions, int destination_stride,
	const FontGlyph& glyph) const
{
	filter.Run(destination_data, destination_dimensions, destination_stride, ColorFormat::RGBA8, glyph.bitmap_data, glyph.bitmap_dimensions,
		Vector2i(radius), glyph.color_format);

	FillColorValuesFromAlpha(destination_data, destination_dimensions, destination_stride);
}

FontEffectOutlineInstancer::FontEffectOutlineInstancer() : id_width(PropertyId::Invalid), id_color(PropertyId::Invalid)
{
	id_width = RegisterProperty("width", "1px", true).AddParser("length").GetId();
	id_color = RegisterProperty("color", "white", false).AddParser("color").GetId();
	RegisterShorthand("font-effect", "width, color", ShorthandType::FallThrough);
}

FontEffectOutlineInstancer::~FontEffectOutlineInstancer() {}

SharedPtr<FontEffect> FontEffectOutlineInstancer::InstanceFontEffect(const String& /*name*/, const PropertyDictionary& properties)
{
	float width = properties.GetProperty(id_width)->Get<float>();
	Colourb color = properties.GetProperty(id_color)->Get<Colourb>();

	auto font_effect = MakeShared<FontEffectOutline>();
	if (font_effect->Initialise(width))
	{
		font_effect->SetColour(color);
		return font_effect;
	}

	return nullptr;
}

} // namespace Rml
