// CSS Grid value parsing. Value semantics follow Taffy (https://github.com/DioxusLabs/taffy) src/style/grid.rs at 66b3364.
// Taffy is MIT licensed: Copyright (c) 2018 Visly Inc., Copyright (c) 2026 Taffy Authors.
#include "PropertyParserGrid.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/PropertyDictionary.h"
#include "../../Include/RmlUi/Core/PropertySpecification.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "../../Include/RmlUi/Core/StyleTypes.h"
#include "CalcExpression.h"
#include "GridDefinition.h"
#include "PropertyParserCalc.h"
#include "PropertyShorthandDefinition.h"
#include <stdlib.h>

namespace Rml {

struct GridToken {
	enum class Type { Word, Function, String, Comma, Slash, OpenBracket, CloseBracket, CloseParen };
	Type type;
	String text;
};

struct GridTokenCursor {
	const Vector<GridToken>& tokens;
	size_t index;

	bool AtEnd() const { return index >= tokens.size(); }
	const GridToken* Peek() const { return AtEnd() ? nullptr : &tokens[index]; }
	bool Accept(GridToken::Type type)
	{
		if (AtEnd() || tokens[index].type != type)
			return false;
		index++;
		return true;
	}
};

static bool IsGridDelimiter(char c)
{
	return StringUtilities::IsWhitespace(c) || c == '[' || c == ']' || c == '(' || c == ')' || c == ',' || c == '/' || c == '"' || c == '\'';
}

static bool IsGridMathFunction(const String& name)
{
	return name == "calc" || name == "min" || name == "max" || name == "clamp";
}

static bool TokenizeGridValue(const String& value, Vector<GridToken>& tokens)
{
	const size_t length = value.size();
	size_t i = 0;
	while (i < length)
	{
		const char c = value[i];
		if (StringUtilities::IsWhitespace(c))
		{
			i++;
			continue;
		}

		if (c == '"' || c == '\'')
		{
			const size_t end = value.find(c, i + 1);
			if (end == String::npos)
				return false;
			tokens.push_back(GridToken{GridToken::Type::String, value.substr(i + 1, end - i - 1)});
			i = end + 1;
			continue;
		}

		GridToken::Type punctuation = GridToken::Type::Word;
		switch (c)
		{
		case '[': punctuation = GridToken::Type::OpenBracket; break;
		case ']': punctuation = GridToken::Type::CloseBracket; break;
		case ')': punctuation = GridToken::Type::CloseParen; break;
		case ',': punctuation = GridToken::Type::Comma; break;
		case '/': punctuation = GridToken::Type::Slash; break;
		case '(': return false;
		default: break;
		}
		if (punctuation != GridToken::Type::Word)
		{
			tokens.push_back(GridToken{punctuation, String()});
			i++;
			continue;
		}

		const size_t start = i;
		while (i < length && !IsGridDelimiter(value[i]))
			i++;

		String word = value.substr(start, i - start);
		if (i < length && value[i] == '(' && IsGridMathFunction(StringUtilities::ToLower(word)))
		{
			int depth = 0;
			for (; i < length; i++)
			{
				if (value[i] == '(')
					depth++;
				else if (value[i] == ')' && --depth == 0)
					break;
			}
			if (i >= length)
				return false;
			i++;
			tokens.push_back(GridToken{GridToken::Type::Word, value.substr(start, i - start)});
		}
		else if (i < length && value[i] == '(')
		{
			tokens.push_back(GridToken{GridToken::Type::Function, StringUtilities::ToLower(std::move(word))});
			i++;
		}
		else
		{
			tokens.push_back(GridToken{GridToken::Type::Word, std::move(word)});
		}
	}
	return true;
}

static bool ParseGridInteger(const String& word, int& out)
{
	size_t i = 0;
	bool negative = false;
	if (i < word.size() && (word[i] == '-' || word[i] == '+'))
	{
		negative = (word[i] == '-');
		i++;
	}
	if (i >= word.size())
		return false;

	long long result = 0;
	for (; i < word.size(); i++)
	{
		if (word[i] < '0' || word[i] > '9')
			return false;
		result = Math::Min(result * 10 + (word[i] - '0'), 1000000LL);
	}
	out = int(negative ? -result : result);
	return true;
}

static bool ParseGridNonNegativeNumber(const String& word, float& out)
{
	bool has_digit = false;
	for (const char c : word)
	{
		if (c >= '0' && c <= '9')
			has_digit = true;
		else if (c != '.')
			return false;
	}
	if (!has_digit)
		return false;
	out = strtof(word.c_str(), nullptr);
	return true;
}

static bool IsGridCustomIdent(const String& word)
{
	if (word.empty())
		return false;

	const String lower = StringUtilities::ToLower(word);
	if (lower == "span" || lower == "auto" || lower == "none" || lower == "inherit" || lower == "initial" || lower == "unset" || lower == "default")
		return false;

	const char first = word[0];
	if ((first >= '0' && first <= '9') || first == '+' || first == '.')
		return false;
	if (first == '-' && word.size() > 1 && ((word[1] >= '0' && word[1] <= '9') || word[1] == '.'))
		return false;
	return true;
}

static bool ParseGridBreadth(const String& word, bool allow_flex, const PropertyParser* parser_length_percent, GridTrackSizingValue& out)
{
	const String lower = StringUtilities::ToLower(word);
	if (lower == "auto")
	{
		out.type = GridTrackSizingType::Auto;
		return true;
	}
	if (lower == "min-content")
	{
		out.type = GridTrackSizingType::MinContent;
		return true;
	}
	if (lower == "max-content")
	{
		out.type = GridTrackSizingType::MaxContent;
		return true;
	}
	if (lower.size() > 2 && StringUtilities::EndsWith(lower, "fr"))
	{
		float number = 0.f;
		if (!allow_flex || !ParseGridNonNegativeNumber(lower.substr(0, lower.size() - 2), number))
			return false;
		out.type = GridTrackSizingType::Flex;
		out.value = NumericValue(number, Unit::NUMBER);
		return true;
	}

	Property property;
	if (!parser_length_percent->ParseValue(property, lower, ParameterMap()) && !PropertyParserCalc::ParseMathFunction(property, lower, Unit::LENGTH_PERCENT, true))
		return false;

	if (property.unit == Unit::CALC)
	{
		out.type = GridTrackSizingType::Calc;
		out.calc = property.Get<CalcExpressionPtr>();
		return true;
	}

	const NumericValue numeric = property.GetNumericValue();
	if (numeric.number < 0.f)
		return false;

	out.type = (numeric.unit == Unit::PERCENT ? GridTrackSizingType::Percentage : GridTrackSizingType::Length);
	out.value = numeric;
	return true;
}

static bool ParseGridTrackSize(GridTokenCursor& cursor, const PropertyParser* parser_length_percent, GridTrackSize& out)
{
	const GridToken* token = cursor.Peek();
	if (!token)
		return false;

	if (token->type == GridToken::Type::Function)
	{
		if (token->text == "minmax")
		{
			cursor.index++;
			const GridToken* min_token = cursor.Peek();
			if (!min_token || min_token->type != GridToken::Type::Word || !ParseGridBreadth(min_token->text, false, parser_length_percent, out.min))
				return false;
			cursor.index++;
			if (!cursor.Accept(GridToken::Type::Comma))
				return false;
			const GridToken* max_token = cursor.Peek();
			if (!max_token || max_token->type != GridToken::Type::Word || !ParseGridBreadth(max_token->text, true, parser_length_percent, out.max))
				return false;
			cursor.index++;
			return cursor.Accept(GridToken::Type::CloseParen);
		}

		if (token->text == "fit-content")
		{
			cursor.index++;
			const GridToken* limit_token = cursor.Peek();
			GridTrackSizingValue limit;
			if (!limit_token || limit_token->type != GridToken::Type::Word || !ParseGridBreadth(limit_token->text, false, parser_length_percent, limit))
				return false;
			if (limit.type != GridTrackSizingType::Length && limit.type != GridTrackSizingType::Percentage)
				return false;
			cursor.index++;
			out.min.type = GridTrackSizingType::Auto;
			out.max.type = (limit.type == GridTrackSizingType::Length ? GridTrackSizingType::FitContentLength : GridTrackSizingType::FitContentPercentage);
			out.max.value = limit.value;
			return cursor.Accept(GridToken::Type::CloseParen);
		}

		return false;
	}

	if (token->type != GridToken::Type::Word)
		return false;

	GridTrackSizingValue breadth;
	if (!ParseGridBreadth(token->text, true, parser_length_percent, breadth))
		return false;
	cursor.index++;

	if (breadth.type == GridTrackSizingType::Flex)
	{
		out.min.type = GridTrackSizingType::Auto;
		out.max = breadth;
	}
	else
	{
		out.min = breadth;
		out.max = breadth;
	}
	return true;
}

static bool ParseGridLineNames(GridTokenCursor& cursor, StringList& names)
{
	if (!cursor.Accept(GridToken::Type::OpenBracket))
		return false;

	while (const GridToken* token = cursor.Peek())
	{
		cursor.index++;
		if (token->type == GridToken::Type::CloseBracket)
			return true;
		if (token->type != GridToken::Type::Word || !IsGridCustomIdent(token->text))
			return false;
		names.push_back(token->text);
	}
	return false;
}

static bool AllGridLineNamesEmpty(const Vector<StringList>& line_names)
{
	for (const StringList& names : line_names)
	{
		if (!names.empty())
			return false;
	}
	return true;
}

static bool ParseGridRepeat(GridTokenCursor& cursor, const PropertyParser* parser_length_percent, GridTrackComponent& component)
{
	if (!cursor.Accept(GridToken::Type::Function))
		return false;

	const GridToken* count_token = cursor.Peek();
	if (!count_token || count_token->type != GridToken::Type::Word)
		return false;

	const String count_lower = StringUtilities::ToLower(count_token->text);
	if (count_lower == "auto-fill")
	{
		component.repeat = GridRepeatType::AutoFill;
	}
	else if (count_lower == "auto-fit")
	{
		component.repeat = GridRepeatType::AutoFit;
	}
	else
	{
		int count = 0;
		if (!ParseGridInteger(count_lower, count) || count < 1)
			return false;
		component.repeat = GridRepeatType::Count;
		component.repeat_count = count;
	}
	cursor.index++;

	if (!cursor.Accept(GridToken::Type::Comma))
		return false;

	component.line_names.emplace_back();
	while (true)
	{
		const GridToken* token = cursor.Peek();
		if (!token)
			return false;

		if (token->type == GridToken::Type::CloseParen)
		{
			cursor.index++;
			break;
		}
		if (token->type == GridToken::Type::OpenBracket)
		{
			if (!ParseGridLineNames(cursor, component.line_names.back()))
				return false;
			continue;
		}

		component.tracks.emplace_back();
		if (!ParseGridTrackSize(cursor, parser_length_percent, component.tracks.back()))
			return false;
		component.line_names.emplace_back();
	}

	if (component.tracks.empty())
		return false;
	if (AllGridLineNamesEmpty(component.line_names))
		component.line_names.clear();
	return true;
}

static bool ParseGridTrackList(const Vector<GridToken>& tokens, bool allow_repeat_and_names, const PropertyParser* parser_length_percent,
	GridTrackList& list)
{
	GridTokenCursor cursor{tokens, 0};
	list.line_names.emplace_back();

	while (!cursor.AtEnd())
	{
		const GridToken& token = tokens[cursor.index];
		if (token.type == GridToken::Type::OpenBracket)
		{
			if (!allow_repeat_and_names || !ParseGridLineNames(cursor, list.line_names.back()))
				return false;
			continue;
		}

		GridTrackComponent component;
		if (token.type == GridToken::Type::Function && token.text == "repeat")
		{
			if (!allow_repeat_and_names || !ParseGridRepeat(cursor, parser_length_percent, component))
				return false;
		}
		else
		{
			component.tracks.emplace_back();
			if (!ParseGridTrackSize(cursor, parser_length_percent, component.tracks.back()))
				return false;
		}
		list.components.push_back(std::move(component));
		list.line_names.emplace_back();
	}

	if (list.components.empty())
		return false;
	if (AllGridLineNamesEmpty(list.line_names))
		list.line_names.clear();
	return true;
}

static void SplitGridAreaCells(const String& row, StringList& cells)
{
	size_t i = 0;
	while (i < row.size())
	{
		const char c = row[i];
		if (StringUtilities::IsWhitespace(c))
		{
			i++;
		}
		else if (c == '.')
		{
			while (i < row.size() && row[i] == '.')
				i++;
			cells.push_back(".");
		}
		else
		{
			const size_t start = i;
			while (i < row.size() && !StringUtilities::IsWhitespace(row[i]) && row[i] != '.')
				i++;
			cells.push_back(row.substr(start, i - start));
		}
	}
}

static bool ParseGridTemplateAreas(const Vector<GridToken>& tokens, GridTemplateAreas& result)
{
	Vector<StringList> rows;
	for (const GridToken& token : tokens)
	{
		if (token.type != GridToken::Type::String)
			return false;
		rows.emplace_back();
		SplitGridAreaCells(token.text, rows.back());
		if (rows.back().empty())
			return false;
	}

	if (rows.empty())
		return false;

	const int column_count = int(rows[0].size());
	for (const StringList& row : rows)
	{
		if (int(row.size()) != column_count)
			return false;
	}

	Vector<int> cell_counts;
	for (int row_index = 0; row_index < int(rows.size()); row_index++)
	{
		for (int column_index = 0; column_index < column_count; column_index++)
		{
			const String& name = rows[row_index][column_index];
			if (name == ".")
				continue;

			size_t area_index = 0;
			while (area_index < result.areas.size() && result.areas[area_index].name != name)
				area_index++;

			if (area_index == result.areas.size())
			{
				GridTemplateArea area;
				area.name = name;
				area.row_start = row_index;
				area.row_end = row_index;
				area.column_start = column_index;
				area.column_end = column_index;
				result.areas.push_back(std::move(area));
				cell_counts.push_back(0);
			}

			GridTemplateArea& area = result.areas[area_index];
			area.row_start = Math::Min(area.row_start, row_index);
			area.row_end = Math::Max(area.row_end, row_index);
			area.column_start = Math::Min(area.column_start, column_index);
			area.column_end = Math::Max(area.column_end, column_index);
			cell_counts[area_index] += 1;
		}
	}

	for (size_t i = 0; i < result.areas.size(); i++)
	{
		GridTemplateArea& area = result.areas[i];
		if (cell_counts[i] != (area.row_end - area.row_start + 1) * (area.column_end - area.column_start + 1))
			return false;

		area.row_start += 1;
		area.row_end += 2;
		area.column_start += 1;
		area.column_end += 2;
	}

	result.row_count = int(rows.size());
	result.column_count = column_count;
	return true;
}

static bool ParseGridPlacement(const Vector<GridToken>& tokens, GridPlacement& placement)
{
	if (tokens.size() == 1 && tokens[0].type == GridToken::Type::Word && StringUtilities::ToLower(tokens[0].text) == "auto")
	{
		placement.type = GridPlacementType::Auto;
		return true;
	}

	bool has_span = false;
	bool has_integer = false;
	bool has_name = false;
	int integer = 0;

	for (const GridToken& token : tokens)
	{
		if (token.type != GridToken::Type::Word)
			return false;

		int parsed_integer = 0;
		if (StringUtilities::ToLower(token.text) == "span")
		{
			if (has_span)
				return false;
			has_span = true;
		}
		else if (ParseGridInteger(token.text, parsed_integer))
		{
			if (has_integer)
				return false;
			has_integer = true;
			integer = parsed_integer;
		}
		else if (IsGridCustomIdent(token.text))
		{
			if (has_name)
				return false;
			has_name = true;
			placement.name = token.text;
		}
		else
		{
			return false;
		}
	}

	if (!has_integer && !has_name)
		return false;

	if (has_span)
	{
		if (has_integer && integer <= 0)
			return false;
		placement.type = GridPlacementType::Span;
		placement.index = (has_integer ? integer : 1);
	}
	else
	{
		if (has_integer && integer == 0)
			return false;
		placement.type = GridPlacementType::Line;
		placement.index = (has_integer ? integer : 0);
	}
	return true;
}

static bool ParseGridAutoFlow(const Vector<GridToken>& tokens, int& keyword)
{
	bool row = false;
	bool column = false;
	bool dense = false;

	for (const GridToken& token : tokens)
	{
		if (token.type != GridToken::Type::Word)
			return false;

		const String lower = StringUtilities::ToLower(token.text);
		bool* flag = nullptr;
		if (lower == "row")
			flag = &row;
		else if (lower == "column")
			flag = &column;
		else if (lower == "dense")
			flag = &dense;

		if (!flag || *flag)
			return false;
		*flag = true;
	}

	if (row && column)
		return false;

	Style::GridAutoFlow flow = Style::GridAutoFlow::Row;
	if (column)
		flow = (dense ? Style::GridAutoFlow::ColumnDense : Style::GridAutoFlow::Column);
	else if (dense)
		flow = Style::GridAutoFlow::RowDense;

	keyword = int(flow);
	return true;
}

PropertyParserGrid::PropertyParserGrid(PropertyParser* parser_length_percent) : parser_length_percent(parser_length_percent)
{
	RMLUI_ASSERT(parser_length_percent);
}

bool PropertyParserGrid::ParseValue(Property& property, const String& value, const ParameterMap& parameters) const
{
	Vector<GridToken> tokens;
	if (!TokenizeGridValue(value, tokens) || tokens.empty())
		return false;

	if (parameters.find("auto-flow") != parameters.end())
	{
		int keyword = 0;
		if (!ParseGridAutoFlow(tokens, keyword))
			return false;
		property.value = Variant(keyword);
		property.unit = Unit::KEYWORD;
		return true;
	}

	const bool is_none = (tokens.size() == 1 && tokens[0].type == GridToken::Type::Word && StringUtilities::ToLower(tokens[0].text) == "none");

	GridDefinition definition;
	definition.value = StringUtilities::StripWhitespace(value);

	if (parameters.find("track-list") != parameters.end())
	{
		definition.type = GridDefinition::Type::TrackList;
		if (!is_none && !ParseGridTrackList(tokens, true, parser_length_percent, definition.track_list))
			return false;
	}
	else if (parameters.find("auto-tracks") != parameters.end())
	{
		definition.type = GridDefinition::Type::TrackList;
		if (!ParseGridTrackList(tokens, false, parser_length_percent, definition.track_list))
			return false;
	}
	else if (parameters.find("areas") != parameters.end())
	{
		definition.type = GridDefinition::Type::Areas;
		if (!is_none && !ParseGridTemplateAreas(tokens, definition.areas))
			return false;
	}
	else if (parameters.find("placement") != parameters.end())
	{
		definition.type = GridDefinition::Type::Placement;
		if (!ParseGridPlacement(tokens, definition.placement))
			return false;
	}
	else
	{
		return false;
	}

	property.value = Variant(GridDefinitionPtr(MakeShared<GridDefinition>(std::move(definition))));
	property.unit = Unit::GRID;
	return true;
}

static void SplitGridSlashParts(const String& value, StringList& parts)
{
	String current;
	int depth = 0;
	char quote = 0;
	for (const char c : value)
	{
		if (quote)
		{
			current += c;
			if (c == quote)
				quote = 0;
			continue;
		}

		if (c == '"' || c == '\'')
			quote = c;
		else if (c == '(')
			depth++;
		else if (c == ')')
			depth--;
		else if (c == '/' && depth == 0)
		{
			parts.push_back(StringUtilities::StripWhitespace(current));
			current.clear();
			continue;
		}
		current += c;
	}
	parts.push_back(StringUtilities::StripWhitespace(current));
}

static void SplitGridTemplateItems(const String& value, StringList& items)
{
	size_t i = 0;
	const size_t length = value.size();
	while (i < length)
	{
		const char c = value[i];
		if (StringUtilities::IsWhitespace(c))
		{
			i++;
			continue;
		}

		const size_t start = i;
		if (c == '"' || c == '\'')
		{
			const size_t end = value.find(c, i + 1);
			i = (end == String::npos ? length : end + 1);
		}
		else if (c == '[')
		{
			const size_t end = value.find(']', i + 1);
			i = (end == String::npos ? length : end + 1);
		}
		else
		{
			int depth = 0;
			while (i < length)
			{
				const char d = value[i];
				if (d == '(')
					depth++;
				else if (d == ')')
					depth--;
				else if (depth == 0 && (StringUtilities::IsWhitespace(d) || d == '"' || d == '\'' || d == '['))
					break;
				i++;
			}
		}
		items.push_back(value.substr(start, i - start));
	}
}

static bool IsGridSingleCustomIdent(const String& value)
{
	Vector<GridToken> tokens;
	return TokenizeGridValue(value, tokens) && tokens.size() == 1 && tokens[0].type == GridToken::Type::Word && IsGridCustomIdent(tokens[0].text);
}

static bool BuildGridAreasTemplate(const String& value, String& rows, String& areas)
{
	StringList items;
	SplitGridTemplateItems(value, items);

	String pending_names;
	bool has_row = false;
	bool row_has_size = false;

	for (const String& item : items)
	{
		const char first = item[0];
		if (first == '[')
		{
			pending_names += ' ' + item;
		}
		else if (first == '"' || first == '\'')
		{
			if (has_row && !row_has_size)
				rows += " auto";
			rows += pending_names;
			pending_names.clear();
			areas += ' ' + item;
			has_row = true;
			row_has_size = false;
		}
		else
		{
			if (!has_row || row_has_size || !pending_names.empty())
				return false;
			rows += ' ' + item;
			row_has_size = true;
		}
	}

	if (!has_row)
		return false;
	if (!row_has_size)
		rows += " auto";
	rows += pending_names;
	return true;
}

static bool ExpandGridTemplate(const StringList& parts, String& rows, String& columns, String& areas)
{
	if (parts.size() == 1 && StringUtilities::ToLower(parts[0]) == "none")
	{
		rows = columns = areas = "none";
		return true;
	}

	if (parts[0].find_first_of("\"'") != String::npos)
	{
		if (parts.size() > 2 || !BuildGridAreasTemplate(parts[0], rows, areas))
			return false;
		columns = (parts.size() == 2 ? parts[1] : String("none"));
		return true;
	}

	if (parts.size() != 2)
		return false;

	rows = parts[0];
	columns = parts[1];
	areas = "none";
	return true;
}

static bool ExtractGridAutoFlow(const String& value, bool& has_auto_flow, bool& dense, String& remainder)
{
	StringList items;
	SplitGridTemplateItems(value, items);

	has_auto_flow = false;
	dense = false;
	size_t i = 0;
	for (; i < items.size(); i++)
	{
		const String lower = StringUtilities::ToLower(items[i]);
		if (lower == "auto-flow" && !has_auto_flow)
			has_auto_flow = true;
		else if (lower == "dense" && !dense)
			dense = true;
		else
			break;
	}

	if (dense && !has_auto_flow)
		return false;

	remainder.clear();
	for (; i < items.size(); i++)
	{
		if (!remainder.empty())
			remainder += ' ';
		remainder += items[i];
	}
	return true;
}

bool PropertyParserGrid::ParseShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& definition, const String& value)
{
	StringList parts;
	SplitGridSlashParts(value, parts);
	for (const String& part : parts)
	{
		if (part.empty())
			return false;
	}

	StringList values(definition.items.size());

	switch (definition.type)
	{
	case ShorthandType::GridLine:
	{
		if (values.size() != 2 || parts.size() > 2)
			return false;
		values[0] = parts[0];
		values[1] = (parts.size() == 2 ? parts[1] : (IsGridSingleCustomIdent(parts[0]) ? parts[0] : String("auto")));
	}
	break;
	case ShorthandType::GridArea:
	{
		if (values.size() != 4 || parts.size() > 4)
			return false;
		values[0] = parts[0];
		values[1] = (parts.size() > 1 ? parts[1] : (IsGridSingleCustomIdent(values[0]) ? values[0] : String("auto")));
		values[2] = (parts.size() > 2 ? parts[2] : (IsGridSingleCustomIdent(values[0]) ? values[0] : String("auto")));
		values[3] = (parts.size() > 3 ? parts[3] : (IsGridSingleCustomIdent(values[1]) ? values[1] : String("auto")));
	}
	break;
	case ShorthandType::GridTemplate:
	{
		if (values.size() != 3 || !ExpandGridTemplate(parts, values[0], values[1], values[2]))
			return false;
	}
	break;
	case ShorthandType::Grid:
	{
		if (values.size() != 6)
			return false;

		bool left_auto_flow = false, left_dense = false, right_auto_flow = false, right_dense = false;
		String left_remainder, right_remainder;
		if (parts.size() == 2 &&
			(!ExtractGridAutoFlow(parts[0], left_auto_flow, left_dense, left_remainder) ||
				!ExtractGridAutoFlow(parts[1], right_auto_flow, right_dense, right_remainder)))
			return false;

		if (left_auto_flow && right_auto_flow)
			return false;

		if (left_auto_flow)
		{
			values[0] = "none";
			values[1] = parts[1];
			values[2] = "none";
			values[3] = (left_remainder.empty() ? String("auto") : left_remainder);
			values[4] = "auto";
			values[5] = (left_dense ? "row dense" : "row");
		}
		else if (right_auto_flow)
		{
			values[0] = parts[0];
			values[1] = "none";
			values[2] = "none";
			values[3] = "auto";
			values[4] = (right_remainder.empty() ? String("auto") : right_remainder);
			values[5] = (right_dense ? "column dense" : "column");
		}
		else
		{
			if (!ExpandGridTemplate(parts, values[0], values[1], values[2]))
				return false;
			values[3] = "auto";
			values[4] = "auto";
			values[5] = "row";
		}
	}
	break;
	default: return false;
	}

	Vector<Property> properties(values.size());
	for (size_t i = 0; i < values.size(); i++)
	{
		const ShorthandItem& item = definition.items[i];
		if (item.type != ShorthandItemType::Property || !item.property_definition->ParseValue(properties[i], values[i]))
			return false;
	}

	for (size_t i = 0; i < values.size(); i++)
		dictionary.SetProperty(definition.items[i].property_id, properties[i]);

	return true;
}

} // namespace Rml
