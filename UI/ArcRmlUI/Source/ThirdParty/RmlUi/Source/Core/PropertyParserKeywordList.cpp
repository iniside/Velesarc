#include "PropertyParserKeywordList.h"

namespace Rml {

bool PropertyParserKeywordList::ParseValue(Property& property, const String& value, const ParameterMap& parameters) const
{
	StringList items;
	StringUtilities::ExpandString(items, value, ',');
	if (items.empty())
		return false;

	String result;
	for (const String& item : items)
	{
		const String keyword = StringUtilities::ToLower(item);
		if (parameters.find(keyword) == parameters.end())
			return false;

		if (!result.empty())
			result += ", ";
		result += keyword;
	}

	property.value = Variant(result);
	property.unit = Unit::STRING;
	return true;
}

} // namespace Rml
