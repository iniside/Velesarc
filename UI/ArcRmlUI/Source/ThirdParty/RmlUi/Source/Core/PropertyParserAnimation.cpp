#include "PropertyParserAnimation.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/PropertyIdSet.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "../../Include/RmlUi/Core/StyleSheetSpecification.h"
#include "PropertyShorthandDefinition.h"
#include <stdlib.h>

namespace Rml {

enum class KeywordType {
	None,
	Tween,
	All,
	Alternate,
	AlternateReverse,
	Reverse,
	Normal,
	Infinite,
	Paused,
	Running,
	FillForwards,
	FillBackwards,
	FillBoth,
};

struct Keyword {
	KeywordType type;
	Tween tween;
	Keyword(Tween tween) : type(KeywordType::Tween), tween(tween) {}
	Keyword(KeywordType type) : type(type) {}

	bool ValidTransition() const
	{
		return type == KeywordType::None || type == KeywordType::Tween || type == KeywordType::All || type == KeywordType::Normal;
	}
	bool ValidAnimation() const { return type != KeywordType::All; }
};

struct PropertyParserAnimationData {
	const UnorderedMap<String, Keyword> keywords = {
		{"none", {KeywordType::None}},
		{"all", {KeywordType::All}},
		{"alternate", {KeywordType::Alternate}},
		{"alternate-reverse", {KeywordType::AlternateReverse}},
		{"reverse", {KeywordType::Reverse}},
		{"normal", {KeywordType::Normal}},
		{"infinite", {KeywordType::Infinite}},
		{"paused", {KeywordType::Paused}},
		{"running", {KeywordType::Running}},
		{"forwards", {KeywordType::FillForwards}},
		{"backwards", {KeywordType::FillBackwards}},
		{"both", {KeywordType::FillBoth}},

		{"ease", {Tween{0.25f, 0.1f, 0.25f, 1.f}}},
		{"ease-in", {Tween{0.42f, 0.f, 1.f, 1.f}}},
		{"ease-out", {Tween{0.f, 0.f, 0.58f, 1.f}}},
		{"ease-in-out", {Tween{0.42f, 0.f, 0.58f, 1.f}}},
		{"linear", {Tween{}}},
		{"step-start", {Tween{1, Tween::StepPosition::JumpStart}}},
		{"step-end", {Tween{1, Tween::StepPosition::JumpEnd}}},

		{"back-in", {Tween{Tween::Back, Tween::In}}},
		{"back-out", {Tween{Tween::Back, Tween::Out}}},
		{"back-in-out", {Tween{Tween::Back, Tween::InOut}}},

		{"bounce-in", {Tween{Tween::Bounce, Tween::In}}},
		{"bounce-out", {Tween{Tween::Bounce, Tween::Out}}},
		{"bounce-in-out", {Tween{Tween::Bounce, Tween::InOut}}},

		{"circular-in", {Tween{Tween::Circular, Tween::In}}},
		{"circular-out", {Tween{Tween::Circular, Tween::Out}}},
		{"circular-in-out", {Tween{Tween::Circular, Tween::InOut}}},

		{"cubic-in", {Tween{Tween::Cubic, Tween::In}}},
		{"cubic-out", {Tween{Tween::Cubic, Tween::Out}}},
		{"cubic-in-out", {Tween{Tween::Cubic, Tween::InOut}}},

		{"elastic-in", {Tween{Tween::Elastic, Tween::In}}},
		{"elastic-out", {Tween{Tween::Elastic, Tween::Out}}},
		{"elastic-in-out", {Tween{Tween::Elastic, Tween::InOut}}},

		{"exponential-in", {Tween{Tween::Exponential, Tween::In}}},
		{"exponential-out", {Tween{Tween::Exponential, Tween::Out}}},
		{"exponential-in-out", {Tween{Tween::Exponential, Tween::InOut}}},

		{"linear-in", {Tween{Tween::Linear, Tween::In}}},
		{"linear-out", {Tween{Tween::Linear, Tween::Out}}},
		{"linear-in-out", {Tween{Tween::Linear, Tween::InOut}}},

		{"quadratic-in", {Tween{Tween::Quadratic, Tween::In}}},
		{"quadratic-out", {Tween{Tween::Quadratic, Tween::Out}}},
		{"quadratic-in-out", {Tween{Tween::Quadratic, Tween::InOut}}},

		{"quartic-in", {Tween{Tween::Quartic, Tween::In}}},
		{"quartic-out", {Tween{Tween::Quartic, Tween::Out}}},
		{"quartic-in-out", {Tween{Tween::Quartic, Tween::InOut}}},

		{"quintic-in", {Tween{Tween::Quintic, Tween::In}}},
		{"quintic-out", {Tween{Tween::Quintic, Tween::Out}}},
		{"quintic-in-out", {Tween{Tween::Quintic, Tween::InOut}}},

		{"sine-in", {Tween{Tween::Sine, Tween::In}}},
		{"sine-out", {Tween{Tween::Sine, Tween::Out}}},
		{"sine-in-out", {Tween{Tween::Sine, Tween::InOut}}},
	};
};

ControlledLifetimeResource<PropertyParserAnimationData> PropertyParserAnimation::parser_data;

namespace {

	bool ParseFloat(const String& value, float& out_value)
	{
		const String stripped = StringUtilities::StripWhitespace(value);
		if (stripped.empty())
			return false;
		const char* begin = stripped.c_str();
		char* end = nullptr;
		out_value = strtof(begin, &end);
		return end == begin + stripped.size();
	}

	// Parses a '<number>' or a '<time>' in seconds or milliseconds, the time is returned in seconds.
	bool ParseNumberOrTime(const String& argument, float& out_value, bool& out_is_time)
	{
		const char* begin = argument.c_str();
		char* end = nullptr;
		const float number = strtof(begin, &end);
		if (end == begin)
			return false;

		const String suffix = StringUtilities::ToLower(String(end));
		if (suffix.empty())
		{
			out_value = number;
			out_is_time = false;
			return true;
		}
		if (suffix == "s")
		{
			out_value = number;
			out_is_time = true;
			return true;
		}
		if (suffix == "ms")
		{
			out_value = number * 0.001f;
			out_is_time = true;
			return true;
		}
		return false;
	}

	// Parses the 'cubic-bezier()' and 'steps()' easing functions.
	bool ParseEasingFunction(const String& argument, Tween& out_tween)
	{
		const String value = StringUtilities::ToLower(argument);
		const size_t open = value.find('(');
		if (open == String::npos || value.back() != ')')
			return false;

		const String name = StringUtilities::StripWhitespace(StringView(value, 0, open));
		StringList arguments;
		StringUtilities::ExpandString(arguments, value.substr(open + 1, value.size() - open - 2), ',');

		if (name == "cubic-bezier")
		{
			float points[4] = {};
			if (arguments.size() != 4)
				return false;
			for (int i = 0; i < 4; i++)
			{
				if (!ParseFloat(arguments[i], points[i]))
					return false;
			}
			if (points[0] < 0.f || points[0] > 1.f || points[2] < 0.f || points[2] > 1.f)
				return false;
			out_tween = Tween{points[0], points[1], points[2], points[3]};
			return true;
		}

		if (name == "steps")
		{
			float num_steps = 0.f;
			if (arguments.empty() || arguments.size() > 2 || !ParseFloat(arguments[0], num_steps) || num_steps < 1.f ||
				float(int(num_steps)) != num_steps)
				return false;

			Tween::StepPosition position = Tween::StepPosition::JumpEnd;
			if (arguments.size() == 2)
			{
				const String& position_name = arguments[1];
				if (position_name == "jump-start" || position_name == "start")
					position = Tween::StepPosition::JumpStart;
				else if (position_name == "jump-end" || position_name == "end")
					position = Tween::StepPosition::JumpEnd;
				else if (position_name == "jump-none")
					position = Tween::StepPosition::JumpNone;
				else if (position_name == "jump-both")
					position = Tween::StepPosition::JumpBoth;
				else
					return false;
			}
			if (position == Tween::StepPosition::JumpNone && num_steps < 2.f)
				return false;

			out_tween = Tween{int(num_steps), position};
			return true;
		}

		return false;
	}

	StringList SplitArguments(const String& value)
	{
		String normalized = value;
		for (char& c : normalized)
		{
			if (StringUtilities::IsWhitespace(c))
				c = ' ';
		}
		StringList arguments;
		StringUtilities::ExpandString(arguments, normalized, ' ', '(', ')', true);
		return arguments;
	}

	String StripQuotes(const String& value)
	{
		if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
			return value.substr(1, value.size() - 2);
		return value;
	}

} // namespace

void PropertyParserAnimation::Initialize()
{
	parser_data.Initialize();
}

void PropertyParserAnimation::Shutdown()
{
	parser_data.Shutdown();
}

PropertyParserAnimation::PropertyParserAnimation(Type type) : type(type) {}

bool PropertyParserAnimation::ParseValue(Property& property, const String& value, const ParameterMap& /*parameters*/) const
{
	StringList list_of_values;
	StringUtilities::ExpandString(list_of_values, value, ',', '(', ')');

	bool result = false;
	if (type == ANIMATION_PARSER)
	{
		result = ParseAnimation(property, list_of_values);
	}
	else if (type == TRANSITION_PARSER)
	{
		result = ParseTransition(property, list_of_values);
	}

	return result;
}

bool PropertyParserAnimation::ParseAnimation(Property& property, const StringList& animation_values)
{
	AnimationList animation_list;

	for (const String& single_animation_value : animation_values)
	{
		Animation animation;

		bool duration_found = false;
		bool delay_found = false;
		bool num_iterations_found = false;
		bool tween_found = false;
		bool direction_found = false;
		bool fill_mode_found = false;
		bool play_state_found = false;
		bool name_found = false;

		for (const String& argument : SplitArguments(single_animation_value))
		{
			if (argument.empty())
				continue;

			auto it = parser_data->keywords.find(StringUtilities::ToLower(argument));
			if (it != parser_data->keywords.end() && it->second.ValidAnimation())
			{
				bool* found_flag = nullptr;
				switch (it->second.type)
				{
				case KeywordType::None:
					// The first 'none' is the fill mode, the second one the animation name.
					found_flag = (fill_mode_found ? &name_found : &fill_mode_found);
					break;
				case KeywordType::Tween:
					found_flag = &tween_found;
					animation.tween = it->second.tween;
					break;
				case KeywordType::Normal: found_flag = &direction_found; break;
				case KeywordType::Alternate:
					found_flag = &direction_found;
					animation.alternate = true;
					break;
				case KeywordType::Reverse:
					found_flag = &direction_found;
					animation.reverse = true;
					break;
				case KeywordType::AlternateReverse:
					found_flag = &direction_found;
					animation.alternate = true;
					animation.reverse = true;
					break;
				case KeywordType::Infinite:
					found_flag = &num_iterations_found;
					animation.num_iterations = -1;
					break;
				case KeywordType::Paused:
					found_flag = &play_state_found;
					animation.paused = true;
					break;
				case KeywordType::Running: found_flag = &play_state_found; break;
				case KeywordType::FillForwards:
					found_flag = &fill_mode_found;
					animation.fill_mode = AnimationFillMode::Forwards;
					break;
				case KeywordType::FillBackwards:
					found_flag = &fill_mode_found;
					animation.fill_mode = AnimationFillMode::Backwards;
					break;
				case KeywordType::FillBoth:
					found_flag = &fill_mode_found;
					animation.fill_mode = AnimationFillMode::Both;
					break;
				case KeywordType::All: break;
				}

				if (!found_flag || *found_flag)
					return false;
				*found_flag = true;
				continue;
			}

			Tween easing_function;
			if (ParseEasingFunction(argument, easing_function))
			{
				if (tween_found)
					return false;
				tween_found = true;
				animation.tween = easing_function;
				continue;
			}

			float number = 0.f;
			bool is_time = false;
			if (ParseNumberOrTime(argument, number, is_time))
			{
				if (is_time && !duration_found)
				{
					duration_found = true;
					animation.duration = number;
				}
				else if (is_time && !delay_found)
				{
					delay_found = true;
					animation.delay = number;
				}
				else if (!is_time && !num_iterations_found)
				{
					num_iterations_found = true;
					animation.num_iterations = Math::RoundToInteger(number);
				}
				else
				{
					return false;
				}
				continue;
			}

			if (name_found)
				return false;
			name_found = true;
			animation.name = StripQuotes(argument);
		}

		// An animation without a keyframes name does not animate anything.
		if (animation.name.empty())
			continue;

		if (animation.duration < 0.0f || (animation.num_iterations < -1 || animation.num_iterations == 0))
			return false;

		animation_list.push_back(std::move(animation));
	}

	property.value = std::move(animation_list);
	property.unit = Unit::ANIMATION;

	return true;
}

bool PropertyParserAnimation::ParseTransition(Property& property, const StringList& transition_values)
{
	TransitionList transition_list{false, false, {}};

	for (const String& single_transition_value : transition_values)
	{
		Transition transition;
		PropertyIdSet target_property_ids;

		bool duration_found = false;
		bool delay_found = false;
		bool reverse_adjustment_factor_found = false;
		bool tween_found = false;
		bool property_found = false;
		bool unknown_property_found = false;

		for (const String& argument : SplitArguments(single_transition_value))
		{
			if (argument.empty())
				continue;

			const String lowercase_argument = StringUtilities::ToLower(argument);

			// See if we have a <keyword> or <tween> specifier as defined in keywords
			auto it = parser_data->keywords.find(lowercase_argument);
			if (it != parser_data->keywords.end() && it->second.ValidTransition())
			{
				if (it->second.type == KeywordType::None)
				{
					if (transition_list.transitions.size() > 0) // The none keyword can not be part of multiple definitions
						return false;
					property = Property{TransitionList{true, false, {}}, Unit::TRANSITION};
					return true;
				}
				else if (it->second.type == KeywordType::All)
				{
					if (transition_list.transitions.size() > 0) // The all keyword can not be part of multiple definitions
						return false;
					transition_list.all = true;
					property_found = true;
				}
				else if (it->second.type == KeywordType::Tween)
				{
					if (tween_found)
						return false;
					tween_found = true;
					transition.tween = it->second.tween;
				}
				continue;
			}

			Tween easing_function;
			if (ParseEasingFunction(argument, easing_function))
			{
				if (tween_found)
					return false;
				tween_found = true;
				transition.tween = easing_function;
				continue;
			}

			float number = 0.f;
			bool is_time = false;
			if (ParseNumberOrTime(argument, number, is_time))
			{
				if (is_time && !duration_found)
				{
					duration_found = true;
					transition.duration = number;
				}
				else if (is_time && !delay_found)
				{
					delay_found = true;
					transition.delay = number;
				}
				else if (!is_time && !reverse_adjustment_factor_found)
				{
					// A plain number is the reverse adjustment factor.
					reverse_adjustment_factor_found = true;
					transition.reverse_adjustment_factor = number;
				}
				else
				{
					return false;
				}
				continue;
			}

			if (lowercase_argument == "allow-discrete")
				continue;

			// Must be a property name or shorthand, expand now
			property_found = true;
			if (auto shorthand = StyleSheetSpecification::GetShorthand(lowercase_argument))
			{
				PropertyIdSet underlying_properties = StyleSheetSpecification::GetShorthandUnderlyingProperties(shorthand->id);
				target_property_ids |= underlying_properties;
			}
			else if (auto definition = StyleSheetSpecification::GetProperty(lowercase_argument))
			{
				// Single property
				target_property_ids.Insert(definition->GetId());
			}
			else
			{
				// Unknown properties are not transitioned, without invalidating the other transitions.
				unknown_property_found = true;
			}
		}

		// The transitioned property defaults to 'all'.
		if (!property_found)
		{
			if (transition_list.transitions.size() > 0)
				return false;
			transition_list.all = true;
		}

		if (unknown_property_found && target_property_ids.Empty() && !transition_list.all)
			continue;

		// Validate the parsed transition
		if ((transition_list.all && !target_property_ids.Empty())    //
			|| (!transition_list.all && target_property_ids.Empty()) //
			|| transition.duration < 0.0f                            //
			|| transition.reverse_adjustment_factor < 0.0f           //
			|| transition.reverse_adjustment_factor > 1.0f           //
		)
		{
			return false;
		}

		if (transition_list.all)
		{
			transition.id = PropertyId::Invalid;
			transition_list.transitions.push_back(transition);
		}
		else
		{
			for (const PropertyId id : target_property_ids)
			{
				transition.id = id;
				transition_list.transitions.push_back(transition);
			}
		}
	}

	property.value = std::move(transition_list);
	property.unit = Unit::TRANSITION;

	return true;
}

} // namespace Rml
