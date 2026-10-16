#include "PropertyParserContent.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include <algorithm>

namespace Rml {

namespace PropertyParserContentItems {
	struct ContentItem {
		bool is_attribute = false;
		String text;
	};

	int HexDigitValue(char c)
	{
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		return -1;
	}

	bool IsIdentifierCharacter(char c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || (unsigned char)c >= 0x80;
	}

	bool ReadString(const String& value, size_t& index, String& out_text)
	{
		const char quote = value[index];
		index++;

		while (index < value.size())
		{
			const char c = value[index];
			if (c == quote)
			{
				index++;
				return true;
			}

			if (c != '\\')
			{
				out_text += c;
				index++;
				continue;
			}

			index++;
			if (index >= value.size())
				return false;

			if (value[index] == '\n')
			{
				index++;
				continue;
			}

			if (HexDigitValue(value[index]) < 0)
			{
				out_text += value[index];
				index++;
				continue;
			}

			uint32_t code_point = 0;
			int num_digits = 0;
			for (; index < value.size() && num_digits < 6 && HexDigitValue(value[index]) >= 0; index++, num_digits++)
				code_point = code_point * 16 + uint32_t(HexDigitValue(value[index]));

			if (index < value.size() && StringUtilities::IsWhitespace(value[index]))
				index++;

			if (code_point == 0 || code_point > 0x10FFFF || (code_point >= 0xD800 && code_point <= 0xDFFF))
				code_point = uint32_t(Character::Replacement);

			out_text += StringUtilities::ToUTF8(Character(code_point));
		}

		return false;
	}

	bool ParseContentItems(const String& value, Vector<ContentItem>& out_items)
	{
		size_t index = 0;
		while (index < value.size())
		{
			const char c = value[index];
			if (StringUtilities::IsWhitespace(c))
			{
				index++;
				continue;
			}

			if (c == '"' || c == '\'')
			{
				ContentItem item;
				if (!ReadString(value, index, item.text))
					return false;
				out_items.push_back(std::move(item));
				continue;
			}

			const size_t name_begin = index;
			while (index < value.size() && IsIdentifierCharacter(value[index]))
				index++;

			const String function_name = StringUtilities::ToLower(value.substr(name_begin, index - name_begin));
			if (function_name != "attr" || index >= value.size() || value[index] != '(')
				return false;

			const size_t argument_end = value.find(')', index);
			if (argument_end == String::npos)
				return false;

			ContentItem item;
			item.is_attribute = true;
			item.text = StringUtilities::StripWhitespace(value.substr(index + 1, argument_end - index - 1));
			if (item.text.empty() || !std::all_of(item.text.begin(), item.text.end(), IsIdentifierCharacter))
				return false;

			out_items.push_back(std::move(item));
			index = argument_end + 1;
		}

		return !out_items.empty();
	}
} // namespace PropertyParserContentItems

bool PropertyParserContent::ParseValue(Property& property, const String& value, const ParameterMap& /*parameters*/) const
{
	const String trimmed_value = StringUtilities::StripWhitespace(value);
	const String keyword = StringUtilities::ToLower(trimmed_value);
	if (keyword == "none" || keyword == "normal")
	{
		property = Property(keyword, Unit::STRING);
		return true;
	}

	Vector<PropertyParserContentItems::ContentItem> items;
	if (!PropertyParserContentItems::ParseContentItems(trimmed_value, items))
		return false;

	property = Property(trimmed_value, Unit::STRING);
	return true;
}

bool PropertyParserContent::GeneratesBox(const String& value)
{
	return value != "none" && value != "normal";
}

String PropertyParserContent::ResolveText(const String& value, const Element* element)
{
	String result;
	if (!GeneratesBox(value))
		return result;

	Vector<PropertyParserContentItems::ContentItem> items;
	if (!PropertyParserContentItems::ParseContentItems(value, items))
		return result;

	for (const PropertyParserContentItems::ContentItem& item : items)
	{
		if (!item.is_attribute)
			result += item.text;
		else if (const Variant* attribute = element->GetAttribute(item.text))
			result += attribute->Get<String>();
	}

	return result;
}

bool PropertyParserContent::ReadsAnyAttribute(const String& value, const ElementAttributes& attributes)
{
	if (!GeneratesBox(value))
		return false;

	Vector<PropertyParserContentItems::ContentItem> items;
	if (!PropertyParserContentItems::ParseContentItems(value, items))
		return false;

	return std::any_of(items.begin(), items.end(), [&attributes](const PropertyParserContentItems::ContentItem& item) {
		return item.is_attribute && attributes.find(item.text) != attributes.end();
	});
}

} // namespace Rml
