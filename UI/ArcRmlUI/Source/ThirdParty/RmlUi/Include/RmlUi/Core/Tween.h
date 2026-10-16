#pragma once

#include "Header.h"
#include "Types.h"

namespace Rml {

class RMLUICORE_API Tween {
public:
	enum Type { None, Back, Bounce, Circular, Cubic, Elastic, Exponential, Linear, Quadratic, Quartic, Quintic, Sine, Callback, CubicBezier, Steps, Count };
	enum Direction { In = 1, Out = 2, InOut = 3 };
	enum class StepPosition : uint8_t { JumpStart, JumpEnd, JumpNone, JumpBoth };
	using CallbackFnc = float (*)(float);

	Tween(Type type = Linear, Direction direction = Out);
	Tween(Type type_in, Type type_out);
	Tween(CallbackFnc callback, Direction direction = In);
	// CSS 'cubic-bezier(x1, y1, x2, y2)' easing function, the x coordinates must be in [0, 1].
	Tween(float x1, float y1, float x2, float y2);
	// CSS 'steps(num_steps, position)' easing function.
	Tween(int num_steps, StepPosition position);

	// Evaluate the Tweening function at point t in [0, 1].
	float operator()(float t) const;

	// Reverse direction of the tweening function.
	void reverse();

	bool operator==(const Tween& other) const;
	bool operator!=(const Tween& other) const;

	String to_string() const;

private:
	float tween(Type type, float t) const;
	float in(float t) const;
	float out(float t) const;
	float in_out(float t) const;

	float cubic_bezier(float t) const;
	float steps(float t) const;

	Type type_in = None;
	Type type_out = None;
	CallbackFnc callback = nullptr;
	Array<float, 4> bezier_points = {};
	int num_steps = 0;
	StepPosition step_position = StepPosition::JumpEnd;
};

} // namespace Rml
