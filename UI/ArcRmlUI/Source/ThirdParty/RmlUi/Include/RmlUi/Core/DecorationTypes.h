#pragma once

#include "NumericValue.h"
#include "Types.h"

namespace Rml {

struct ColorStop {
	ColourbPremultiplied color;
	NumericValue position;
	bool current_color = false;
};
inline bool operator==(const ColorStop& a, const ColorStop& b)
{
	return a.color == b.color && a.position == b.position && a.current_color == b.current_color;
}
inline bool operator!=(const ColorStop& a, const ColorStop& b)
{
	return !(a == b);
}

struct BoxShadow {
	ColourbPremultiplied color;
	NumericValue offset_x, offset_y;
	NumericValue blur_radius;
	NumericValue spread_distance;
	bool inset = false;
	bool current_color = false;
};
inline bool operator==(const BoxShadow& a, const BoxShadow& b)
{
	return a.color == b.color && a.offset_x == b.offset_x && a.offset_y == b.offset_y && a.blur_radius == b.blur_radius &&
		a.spread_distance == b.spread_distance && a.inset == b.inset && a.current_color == b.current_color;
}
inline bool operator!=(const BoxShadow& a, const BoxShadow& b)
{
	return !(a == b);
}

/// Position, size, repetition and positioning area of a background or mask layer painted by a decorator.
struct DecoratorLayer {
	enum class SizeType : uint8_t { Auto, Cover, Contain, Explicit };

	NumericValue position_x = NumericValue(0.f, Unit::PERCENT);
	NumericValue position_y = NumericValue(0.f, Unit::PERCENT);
	SizeType size_type = SizeType::Auto;
	// Explicit sizes, where Unit::KEYWORD stands for 'auto'.
	NumericValue size_x = NumericValue(0.f, Unit::KEYWORD);
	NumericValue size_y = NumericValue(0.f, Unit::KEYWORD);
	bool repeat_x = true;
	bool repeat_y = true;
	// Positioning area, or Auto to use the paint area.
	BoxArea origin = BoxArea::Auto;
};
inline bool operator==(const DecoratorLayer& a, const DecoratorLayer& b)
{
	return a.position_x == b.position_x && a.position_y == b.position_y && a.size_type == b.size_type && a.size_x == b.size_x &&
		a.size_y == b.size_y && a.repeat_x == b.repeat_x && a.repeat_y == b.repeat_y && a.origin == b.origin;
}
inline bool operator!=(const DecoratorLayer& a, const DecoratorLayer& b)
{
	return !(a == b);
}

} // namespace Rml
