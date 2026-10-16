#include "../../Include/RmlUi/Core/Tween.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "../../Include/RmlUi/Core/TypeConverter.h"
#include <cmath>
#include <utility>

namespace Rml {

namespace TweenFunctions {

	/**
	    Tweening functions below.
	    Partly based on http://libclaw.sourceforge.net/tweeners.html
	 */

	static inline float square(float t)
	{
		return t * t;
	}

	static float back(float t)
	{
		return t * t * (2.70158f * t - 1.70158f);
	}

	static float bounce(float t)
	{
		if (t > 1.f - 1.f / 2.75f)
			return 1.f - 7.5625f * square(1.f - t);
		else if (t > 1.f - 2.f / 2.75f)
			return 1.0f - (7.5625f * square(1.f - t - 1.5f / 2.75f) + 0.75f);
		else if (t > 1.f - 2.5f / 2.75f)
			return 1.0f - (7.5625f * square(1.f - t - 2.25f / 2.75f) + 0.9375f);
		return 1.0f - (7.5625f * square(1.f - t - 2.625f / 2.75f) + 0.984375f);
	}

	static float circular(float t)
	{
		return 1.f - Math::SquareRoot(1.f - t * t);
	}

	static float cubic(float t)
	{
		return t * t * t;
	}

	static float elastic(float t)
	{
		if (t == 0)
			return t;
		if (t == 1)
			return t;
		return -Math::Exp(7.24f * (t - 1.f)) * Math::Sin((t - 1.1f) * 2.f * Math::RMLUI_PI / 0.4f);
	}

	static float exponential(float t)
	{
		if (t == 0)
			return t;
		if (t == 1)
			return t;
		return Math::Exp(7.24f * (t - 1.f));
	}

	static float linear(float t)
	{
		return t;
	}

	static float quadratic(float t)
	{
		return t * t;
	}

	static float quartic(float t)
	{
		return t * t * t * t;
	}

	static float quintic(float t)
	{
		return t * t * t * t * t;
	}

	static float sine(float t)
	{
		return 1.f - Math::Cos(t * Math::RMLUI_PI * 0.5f);
	}

} // namespace TweenFunctions

Tween::Tween(Type type, Direction direction)
{
	if (direction & In)
		type_in = type;
	if (direction & Out)
		type_out = type;
}
Tween::Tween(Type type_in, Type type_out) : type_in(type_in), type_out(type_out) {}
Tween::Tween(CallbackFnc callback, Direction direction) : callback(callback)
{
	if (direction & In)
		type_in = Callback;
	if (direction & Out)
		type_out = Callback;
}
Tween::Tween(float x1, float y1, float x2, float y2) : type_in(CubicBezier), type_out(CubicBezier), bezier_points{x1, y1, x2, y2} {}
Tween::Tween(int num_steps, StepPosition position) : type_in(Steps), type_out(Steps), num_steps(num_steps), step_position(position) {}

float Tween::operator()(float t) const
{
	if (type_in == CubicBezier)
		return cubic_bezier(t);
	if (type_in == Steps)
		return steps(t);

	if (type_in != None && type_out == None)
	{
		return in(t);
	}
	if (type_in == None && type_out != None)
	{
		return out(t);
	}
	if (type_in != None && type_out != None)
	{
		return in_out(t);
	}
	return t;
}

void Tween::reverse()
{
	std::swap(type_in, type_out);

	if (type_in == CubicBezier)
		bezier_points = {1.f - bezier_points[2], 1.f - bezier_points[3], 1.f - bezier_points[0], 1.f - bezier_points[1]};

	if (type_in == Steps)
	{
		if (step_position == StepPosition::JumpStart)
			step_position = StepPosition::JumpEnd;
		else if (step_position == StepPosition::JumpEnd)
			step_position = StepPosition::JumpStart;
	}
}

bool Tween::operator==(const Tween& other) const
{
	return type_in == other.type_in && type_out == other.type_out && callback == other.callback && bezier_points == other.bezier_points &&
		num_steps == other.num_steps && step_position == other.step_position;
}

bool Tween::operator!=(const Tween& other) const
{
	return !(*this == other);
}

String Tween::to_string() const
{
	static const Array<String, size_t(Count)> type_str = {{"none", "back", "bounce", "circular", "cubic", "elastic", "exponential", "linear",
		"quadratic", "quartic", "quintic", "sine", "callback", "cubic-bezier", "steps"}};

	if (type_in == CubicBezier)
	{
		return "cubic-bezier(" + ToString(bezier_points[0]) + ", " + ToString(bezier_points[1]) + ", " + ToString(bezier_points[2]) + ", " +
			ToString(bezier_points[3]) + ")";
	}
	if (type_in == Steps)
	{
		static const Array<String, 4> position_str = {{"jump-start", "jump-end", "jump-none", "jump-both"}};
		return "steps(" + ToString(num_steps) + ", " + position_str[size_t(step_position)] + ")";
	}

	if (size_t(type_in) < type_str.size() && size_t(type_out) < type_str.size())
	{
		if (type_in == None && type_out == None)
		{
			return "none";
		}
		else if (type_in == type_out)
		{
			return type_str[size_t(type_in)] + String("-in-out");
		}
		else if (type_in == None)
		{
			return type_str[size_t(type_out)] + String("-out");
		}
		else if (type_out == None)
		{
			return type_str[size_t(type_in)] + String("-in");
		}
		else if (type_in != type_out)
		{
			return type_str[size_t(type_in)] + String("-in-") + type_str[size_t(type_out)] + String("-out");
		}
	}
	return "unknown";
}

float Tween::tween(Type type, float t) const
{
	using namespace TweenFunctions;

	switch (type)
	{
	case Back: return back(t);
	case Bounce: return bounce(t);
	case Circular: return circular(t);
	case Cubic: return cubic(t);
	case Elastic: return elastic(t);
	case Exponential: return exponential(t);
	case Linear: return linear(t);
	case Quadratic: return quadratic(t);
	case Quartic: return quartic(t);
	case Quintic: return quintic(t);
	case Sine: return sine(t);
	case Callback:
		if (callback)
			return (*callback)(t);
		break;
	default: break;
	}
	return t;
}

float Tween::cubic_bezier(float t) const
{
	const float x1 = bezier_points[0];
	const float y1 = bezier_points[1];
	const float x2 = bezier_points[2];
	const float y2 = bezier_points[3];

	const float cx = 3.f * x1;
	const float bx = 3.f * (x2 - x1) - cx;
	const float ax = 1.f - cx - bx;
	const float cy = 3.f * y1;
	const float by = 3.f * (y2 - y1) - cy;
	const float ay = 1.f - cy - by;

	auto SampleX = [&](float s) { return ((ax * s + bx) * s + cx) * s; };
	auto SampleY = [&](float s) { return ((ay * s + by) * s + cy) * s; };
	auto SampleDerivativeX = [&](float s) { return (3.f * ax * s + 2.f * bx) * s + cx; };

	if (t <= 0.f || t >= 1.f)
		return Math::Clamp(t, 0.f, 1.f);

	const float epsilon = 1e-6f;

	float s = t;
	for (int i = 0; i < 8; i++)
	{
		const float x = SampleX(s) - t;
		if (Math::Absolute(x) < epsilon)
			return SampleY(s);
		const float derivative = SampleDerivativeX(s);
		if (Math::Absolute(derivative) < epsilon)
			break;
		s -= x / derivative;
	}

	float lower = 0.f;
	float upper = 1.f;
	s = t;
	for (int i = 0; i < 32 && lower < upper; i++)
	{
		const float x = SampleX(s);
		if (Math::Absolute(x - t) < epsilon)
			break;
		if (t > x)
			lower = s;
		else
			upper = s;
		s = 0.5f * (lower + upper);
	}
	return SampleY(s);
}

float Tween::steps(float t) const
{
	if (num_steps < 1)
		return t;

	int jumps = num_steps;
	if (step_position == StepPosition::JumpNone)
		jumps = num_steps - 1;
	else if (step_position == StepPosition::JumpBoth)
		jumps = num_steps + 1;
	if (jumps < 1)
		return t;

	int current_step = int(std::floor(t * float(num_steps)));
	if (step_position == StepPosition::JumpStart || step_position == StepPosition::JumpBoth)
		current_step += 1;
	if (t >= 0.f && current_step < 0)
		current_step = 0;
	if (t <= 1.f && current_step > jumps)
		current_step = jumps;

	return float(current_step) / float(jumps);
}

float Tween::in(float t) const
{
	return tween(type_in, t);
}

float Tween::out(float t) const
{
	return 1.0f - tween(type_out, 1.0f - t);
}

float Tween::in_out(float t) const
{
	if (t < 0.5f)
		return tween(type_in, 2.0f * t) * 0.5f;
	else
		return 0.5f + out(2.0f * t - 1.0f) * 0.5f;
}

} // namespace Rml
