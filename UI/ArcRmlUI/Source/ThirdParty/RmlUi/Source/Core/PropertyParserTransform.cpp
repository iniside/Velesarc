#include "PropertyParserTransform.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "../../Include/RmlUi/Core/NumericValue.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "../../Include/RmlUi/Core/Transform.h"
#include "../../Include/RmlUi/Core/TransformPrimitive.h"
#include "CalcExpression.h"
#include "PropertyParserCalc.h"
#include <string.h>

namespace Rml {

namespace {

	bool IsMathFunction(const String& argument)
	{
		const String lowercase_argument = StringUtilities::ToLower(argument);
		return StringUtilities::StartsWith(lowercase_argument, "calc(") || StringUtilities::StartsWith(lowercase_argument, "min(") ||
			StringUtilities::StartsWith(lowercase_argument, "max(") || StringUtilities::StartsWith(lowercase_argument, "clamp(");
	}

	// Adds a translation, and one more translation per additional unit of arguments declared as a sum of units in calc().
	template <typename TranslatePrimitive>
	void AddTranslatePrimitives(Transform& transform, const NumericValue* args, const Vector<NumericValue>* additional_terms, int nargs)
	{
		transform.AddPrimitive({TranslatePrimitive(args)});

		size_t num_additional = 0;
		for (int i = 0; i < nargs; i++)
			num_additional = Math::Max(num_additional, additional_terms[i].size());

		for (size_t term = 0; term < num_additional; term++)
		{
			NumericValue term_args[3];
			for (int i = 0; i < nargs; i++)
				term_args[i] = (term < additional_terms[i].size() ? additional_terms[i][term] : NumericValue(0.f, Unit::PX));
			transform.AddPrimitive({TranslatePrimitive(term_args)});
		}
	}

} // namespace

PropertyParserTransform::PropertyParserTransform() :
	number(Unit::NUMBER), length(Unit::LENGTH, Unit::PX), length_pct(Unit::LENGTH_PERCENT, Unit::PX), angle(Unit::ANGLE, Unit::RAD)
{}

PropertyParserTransform::~PropertyParserTransform() {}

bool PropertyParserTransform::ParseValue(Property& property, const String& value, const ParameterMap& /*parameters*/) const
{
	if (value == "none")
	{
		property.value = Variant(TransformPtr());
		property.unit = Unit::TRANSFORM;
		return true;
	}

	TransformPtr transform = MakeShared<Transform>();

	char const* next = value.c_str();

	NumericValue args[16];
	Vector<NumericValue> additional_terms[3];

	const PropertyParser* number16[] = {&number, &number, &number, &number, &number, &number, &number, &number, &number, &number, &number, &number,
		&number, &number, &number, &number};
	const PropertyParser* lengthpct2_length1[] = {&length_pct, &length_pct, &length};
	const PropertyParser* number3angle1[] = {&number, &number, &number, &angle};
	const PropertyParser* angle2[] = {&angle, &angle};
	const PropertyParser* length1[] = {&length};

	// For semantic purposes, define subsets of the above parsers when scanning primitives below.
	auto lengthpct1 = lengthpct2_length1;
	auto lengthpct2 = lengthpct2_length1;
	auto angle1 = angle2;
	auto number1 = number16;
	auto number2 = number16;
	auto number3 = number16;
	auto number6 = number16;

	while (*next)
	{
		using namespace Transforms;
		int bytes_read = 0;

		if (Scan(bytes_read, next, "perspective", length1, args, 1))
		{
			transform->AddPrimitive({Perspective(args)});
		}
		else if (Scan(bytes_read, next, "matrix", number6, args, 6))
		{
			transform->AddPrimitive({Matrix2D(args)});
		}
		else if (Scan(bytes_read, next, "matrix3d", number16, args, 16))
		{
			transform->AddPrimitive({Matrix3D(args)});
		}
		else if (Scan(bytes_read, next, "translateX", lengthpct1, args, 1, additional_terms))
		{
			AddTranslatePrimitives<TranslateX>(*transform, args, additional_terms, 1);
		}
		else if (Scan(bytes_read, next, "translateY", lengthpct1, args, 1, additional_terms))
		{
			AddTranslatePrimitives<TranslateY>(*transform, args, additional_terms, 1);
		}
		else if (Scan(bytes_read, next, "translateZ", length1, args, 1, additional_terms))
		{
			AddTranslatePrimitives<TranslateZ>(*transform, args, additional_terms, 1);
		}
		else if (Scan(bytes_read, next, "translate", lengthpct2, args, 2, additional_terms))
		{
			AddTranslatePrimitives<Translate2D>(*transform, args, additional_terms, 2);
		}
		else if (Scan(bytes_read, next, "translate3d", lengthpct2_length1, args, 3, additional_terms))
		{
			AddTranslatePrimitives<Translate3D>(*transform, args, additional_terms, 3);
		}
		else if (Scan(bytes_read, next, "scaleX", number1, args, 1))
		{
			transform->AddPrimitive({ScaleX(args)});
		}
		else if (Scan(bytes_read, next, "scaleY", number1, args, 1))
		{
			transform->AddPrimitive({ScaleY(args)});
		}
		else if (Scan(bytes_read, next, "scaleZ", number1, args, 1))
		{
			transform->AddPrimitive({ScaleZ(args)});
		}
		else if (Scan(bytes_read, next, "scale", number2, args, 2))
		{
			transform->AddPrimitive({Scale2D(args)});
		}
		else if (Scan(bytes_read, next, "scale", number1, args, 1))
		{
			args[1] = args[0];
			transform->AddPrimitive({Scale2D(args)});
		}
		else if (Scan(bytes_read, next, "scale3d", number3, args, 3))
		{
			transform->AddPrimitive({Scale3D(args)});
		}
		else if (Scan(bytes_read, next, "rotateX", angle1, args, 1))
		{
			transform->AddPrimitive({RotateX(args)});
		}
		else if (Scan(bytes_read, next, "rotateY", angle1, args, 1))
		{
			transform->AddPrimitive({RotateY(args)});
		}
		else if (Scan(bytes_read, next, "rotateZ", angle1, args, 1))
		{
			transform->AddPrimitive({RotateZ(args)});
		}
		else if (Scan(bytes_read, next, "rotate", angle1, args, 1))
		{
			transform->AddPrimitive({Rotate2D(args)});
		}
		else if (Scan(bytes_read, next, "rotate3d", number3angle1, args, 4))
		{
			transform->AddPrimitive({Rotate3D(args)});
		}
		else if (Scan(bytes_read, next, "skewX", angle1, args, 1))
		{
			transform->AddPrimitive({SkewX(args)});
		}
		else if (Scan(bytes_read, next, "skewY", angle1, args, 1))
		{
			transform->AddPrimitive({SkewY(args)});
		}
		else if (Scan(bytes_read, next, "skew", angle2, args, 2))
		{
			transform->AddPrimitive({Skew2D(args)});
		}

		if (bytes_read > 0)
		{
			next += bytes_read;
		}
		else
		{
			return false;
		}
	}

	property.value = Variant(std::move(transform));
	property.unit = Unit::TRANSFORM;

	return true;
}

bool PropertyParserTransform::Scan(int& out_bytes_read, const char* str, const char* keyword, const PropertyParser** parsers, NumericValue* args,
	int nargs, Vector<NumericValue>* additional_terms) const
{
	if (additional_terms)
	{
		for (int i = 0; i < nargs; i++)
			additional_terms[i].clear();
	}

	out_bytes_read = 0;
	int total_bytes_read = 0, bytes_read = 0;

	/* skip leading white space */
	bytes_read = 0;
	sscanf(str, " %n", &bytes_read);
	str += bytes_read;
	total_bytes_read += bytes_read;

	/* find the keyword */
	if (!memcmp(str, keyword, strlen(keyword)))
	{
		bytes_read = (int)strlen(keyword);
		str += bytes_read;
		total_bytes_read += bytes_read;
	}
	else
	{
		return false;
	}

	/* skip any white space */
	bytes_read = 0;
	sscanf(str, " %n", &bytes_read);
	str += bytes_read;
	total_bytes_read += bytes_read;

	/* find the opening brace */
	bytes_read = 0;
	if (sscanf(str, " ( %n", &bytes_read), bytes_read)
	{
		str += bytes_read;
		total_bytes_read += bytes_read;
	}
	else
	{
		return false;
	}

	/* parse the arguments */
	for (int i = 0; i < nargs; ++i)
	{
		const char* arg_begin = str;
		int depth = 0;
		while (*str && (depth > 0 || (*str != ',' && *str != ')')))
		{
			if (*str == '(')
				depth++;
			else if (*str == ')')
				depth--;
			str++;
		}
		total_bytes_read += int(str - arg_begin);

		const String arg = StringUtilities::StripWhitespace(StringView(arg_begin, str));
		if (arg.empty() || !ParseArgument(arg, parsers[i], args[i], additional_terms ? &additional_terms[i] : nullptr))
			return false;

		/* find the comma */
		if (i < nargs - 1)
		{
			bytes_read = 0;
			if (sscanf(str, " , %n", &bytes_read), bytes_read)
			{
				str += bytes_read;
				total_bytes_read += bytes_read;
			}
			else
			{
				return false;
			}
		}
	}

	/* find the closing brace */
	bytes_read = 0;
	if (sscanf(str, " ) %n", &bytes_read), bytes_read)
	{
		str += bytes_read;
		total_bytes_read += bytes_read;
	}
	else
	{
		return false;
	}

	out_bytes_read = total_bytes_read;
	return total_bytes_read > 0;
}

bool PropertyParserTransform::ParseArgument(const String& arg, const PropertyParser* parser, NumericValue& out_value,
	Vector<NumericValue>* out_additional_terms) const
{
	Property prop;
	if (!IsMathFunction(arg))
	{
		if (!parser->ParseValue(prop, arg, ParameterMap()))
			return false;
		out_value = NumericValue(prop.value.Get<float>(), prop.unit);
		return true;
	}

	Units units = Unit::LENGTH_PERCENT;
	if (parser == &number)
		units = Unit::NUMBER;
	else if (parser == &angle)
		units = Unit::ANGLE;
	else if (parser == &length)
		units = Unit::LENGTH;

	if (!PropertyParserCalc::ParseMathFunction(prop, arg, units, false))
		return false;

	if (prop.unit != Unit::CALC)
	{
		out_value = NumericValue(prop.value.Get<float>(), prop.unit);
		return true;
	}

	const CalcExpressionPtr& expression = prop.value.GetReference<CalcExpressionPtr>();
	Vector<NumericValue> terms;
	if (!out_additional_terms || !expression || !LinearizeCalcExpression(*expression, terms) || terms.empty())
		return false;

	out_value = terms[0];
	out_additional_terms->assign(terms.begin() + 1, terms.end());
	return true;
}

} // namespace Rml
