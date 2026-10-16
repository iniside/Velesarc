#include "../../Include/RmlUi/Core/BaseXMLParser.h"
#include "ArcPerfInternal.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/Stream.h"
#include "XMLParseTools.h"
#include <string.h>

namespace Rml {

namespace BaseXMLParserScan {

	int CountNewlines(const char* begin, const char* end)
	{
		int count = 0;
		while (begin < end)
		{
			const char* found = static_cast<const char*>(memchr(begin, '\n', size_t(end - begin)));
			if (!found)
				break;
			++count;
			begin = found + 1;
		}
		return count;
	}

	char ToLowerAscii(char c)
	{
		return (c >= 'A' && c <= 'Z') ? char(c + ('a' - 'A')) : c;
	}

	bool EqualsLowerCase(const char* begin, const char* end, const char* lower_case)
	{
		for (; begin < end; ++begin, ++lower_case)
		{
			if (*lower_case == '\0' || ToLowerAscii(*begin) != *lower_case)
				return false;
		}
		return *lower_case == '\0';
	}

	bool MatchesAt(const char* source, size_t source_size, size_t index, const char* string, size_t string_length)
	{
		return source_size - index >= string_length && memcmp(source + index, string, string_length) == 0;
	}

} // namespace BaseXMLParserScan

BaseXMLParser::BaseXMLParser(const SmallUnorderedSet<String>& in_cdata_tags, const SmallUnorderedSet<String>& in_inner_xml_attributes) :
	cdata_tags(in_cdata_tags), inner_xml_attributes(in_inner_xml_attributes)
{}

BaseXMLParser::~BaseXMLParser() {}

void BaseXMLParser::Parse(Stream* stream)
{
	RMLUI_ARC_PERF_SCOPE(XmlParse);
	source_url = &stream->GetSourceURL();

	xml_source.clear();

	// We read in the whole XML file here.
	// TODO: It doesn't look like the Stream interface is used for anything useful. We
	//   might as well just use a span or StringView, and get completely rid of it.
	// @performance Otherwise, use the temporary allocator.
	const size_t source_size = stream->Length();
	stream->Read(xml_source, source_size);

	xml_index = 0;
	line_number = 1;
	line_number_open_tag = 1;

	// Read (er ... skip) the header, if one exists.
	ReadHeader();
	// Read the XML body.
	ReadBody();

	xml_source.clear();
	source_url = nullptr;
}

int BaseXMLParser::GetLineNumber() const
{
	return line_number;
}

int BaseXMLParser::GetLineNumberOpenTag() const
{
	return line_number_open_tag;
}

void BaseXMLParser::SetSourceURL(const URL* url)
{
	source_url = url;
}

void BaseXMLParser::SetLineNumbers(int in_line_number, int in_line_number_open_tag)
{
	line_number = in_line_number;
	line_number_open_tag = in_line_number_open_tag;
}

void BaseXMLParser::HandleElementStart(const String& /*name*/, const XMLAttributes& /*attributes*/) {}

void BaseXMLParser::HandleElementEnd(const String& /*name*/) {}

void BaseXMLParser::HandleData(const String& /*data*/, XMLDataType /*type*/) {}

const URL* BaseXMLParser::GetSourceURLPtr() const
{
	return source_url;
}

bool BaseXMLParser::AtEnd() const
{
	return xml_index >= xml_source.size();
}

void BaseXMLParser::HandleElementStartInternal(const String& name, const XMLAttributes& attributes)
{
	line_number_open_tag = line_number;
	HandleElementStart(name, attributes);
}

void BaseXMLParser::ReadHeader()
{
	if (PeekString("<?"))
		FindString(">", nullptr);
}

void BaseXMLParser::ReadBody()
{
	RMLUI_ZoneScoped;

	open_tag_depth = 0;
	line_number_open_tag = 0;

	for (;;)
	{
		// Find the next open tag.
		if (!FindString("<", &data, true))
			break;

		// Check what kind of tag this is.
		if (PeekString("!--"))
		{
			// Comment.
			if (!FindString("-->", nullptr))
				break;
		}
		else if (PeekString("![CDATA["))
		{
			// CDATA tag; read everything (including markup) until the ending
			// CDATA tag.
			FindString("]]>", &data);
		}
		else if (PeekString("/"))
		{
			if (!ReadCloseTag())
				break;

			// Bail if we've hit the end of the XML data.
			if (open_tag_depth == 0)
				break;
		}
		else
		{
			bool closed_inner_xml = false;
			if (!ReadOpenTag(closed_inner_xml))
				break;

			if (closed_inner_xml && open_tag_depth == 0)
				break;
		}
	}

	if (open_tag_depth > 0)
	{
		Log::Message(Log::LT_WARNING, "XML parse error on line %d of %s.", GetLineNumber(), source_url->GetURL().c_str());
	}
}

bool BaseXMLParser::ReadOpenTag(bool& closed_inner_xml)
{
	// Increase the open depth
	open_tag_depth++;

	// Opening tag; send data immediately and open the tag.
	if (!data.empty())
	{
		HandleData(data, XMLDataType::Text);
		data.clear();
	}

	size_t name_begin = 0;
	size_t name_end = 0;
	if (!FindWord(name_begin, name_end, "/>"))
		return false;

	const String tag_name(xml_source, name_begin, name_end - name_begin);
	XMLAttributes attributes;
	bool parse_inner_xml_as_data = false;
	bool section_opened = false;

	if (PeekString(">"))
	{
		// Simple open tag.
		HandleElementStartInternal(tag_name, attributes);
		section_opened = true;
	}
	else if (PeekString("/") && PeekString(">"))
	{
		// Empty open tag.
		HandleElementStartInternal(tag_name, attributes);
		HandleElementEnd(tag_name);

		// Tag immediately closed, reduce count
		open_tag_depth--;
	}
	else
	{
		// It appears we have some attributes. Let's parse them.
		if (!ReadAttributes(&attributes, parse_inner_xml_as_data))
			return false;

		if (PeekString(">"))
		{
			HandleElementStartInternal(tag_name, attributes);
			section_opened = true;
		}
		else if (PeekString("/") && PeekString(">"))
		{
			HandleElementStartInternal(tag_name, attributes);
			HandleElementEnd(tag_name);

			// Tag immediately closed, reduce count
			open_tag_depth--;
		}
		else
		{
			return false;
		}
	}

	if (!section_opened)
		return true;

	const String* cdata_tag = FindCDATATag(name_begin, name_end);

	if (parse_inner_xml_as_data)
	{
		const size_t capture_begin = xml_index;
		size_t capture_end = 0;
		const bool closed = cdata_tag ? ReadCDATA(cdata_tag->c_str(), capture_end) : SkipInnerXML(capture_end);
		if (!closed)
		{
			if (!AtEnd())
				return false;
			capture_end = xml_source.size();
		}

		HandleData(xml_source.substr(capture_begin, capture_end - capture_begin), XMLDataType::InnerXML);

		if (!closed)
			return false;

		if (cdata_tag)
		{
			open_tag_depth--;
			HandleElementEnd(tag_name);
			return true;
		}

		closed_inner_xml = true;
		return ReadCloseTagName();
	}

	// Check if this tag needs to be processed as CDATA.
	if (cdata_tag)
	{
		const size_t content_begin = xml_index;
		size_t content_end = 0;
		if (!ReadCDATA(cdata_tag->c_str(), content_end))
			return false;

		open_tag_depth--;
		data.append(xml_source, content_begin, content_end - content_begin);
		if (!data.empty())
		{
			HandleData(data, XMLDataType::CDATA);
			data.clear();
		}
		HandleElementEnd(tag_name);
	}

	return true;
}

bool BaseXMLParser::ReadCloseTag()
{
	if (open_tag_depth <= 0)
	{
		Log::Message(Log::LT_WARNING, "Found unexpected XML close tag on line %d of %s", GetLineNumber(), source_url->GetURL().c_str());
		return false;
	}

	// Closing tag; send data immediately and close the tag.
	if (!data.empty())
	{
		HandleData(data, XMLDataType::Text);
		data.clear();
	}

	return ReadCloseTagName();
}

bool BaseXMLParser::ReadCloseTagName()
{
	const size_t name_begin = xml_index;
	if (!FindString(">", nullptr))
		return false;

	const char* source = xml_source.data();
	HandleElementEnd(StringUtilities::StripWhitespace(StringView(source + name_begin, source + xml_index - 1)));

	// Tag closed, reduce count
	open_tag_depth--;

	return true;
}

bool BaseXMLParser::ReadAttributes(XMLAttributes* attributes, bool& parse_raw_xml_content)
{
	for (;;)
	{
		// Get the attribute name
		size_t name_begin = 0;
		size_t name_end = 0;
		if (!FindWord(name_begin, name_end, "=/>"))
			return false;

		// Check if theres an assigned value
		size_t value_begin = name_end;
		size_t value_end = name_end;
		if (PeekString("="))
		{
			if (PeekString("\""))
			{
				value_begin = xml_index;
				if (!FindString("\"", nullptr))
					return false;
				value_end = xml_index - 1;
			}
			else if (PeekString("'"))
			{
				value_begin = xml_index;
				if (!FindString("'", nullptr))
					return false;
				value_end = xml_index - 1;
			}
			else if (!FindWord(value_begin, value_end, "/>"))
			{
				return false;
			}
		}

		if (attributes)
		{
			const String name(xml_source, name_begin, name_end - name_begin);
			if (IsInnerXMLAttribute(name))
				parse_raw_xml_content = true;

			const char* value = xml_source.data() + value_begin;
			const size_t value_length = value_end - value_begin;
			if (memchr(value, '&', value_length))
				(*attributes)[name] = StringUtilities::DecodeRml(String(value, value_length));
			else
				(*attributes)[name] = String(value, value_length);
		}

		// Check for the end of the tag.
		if (PeekString("/", false) || PeekString(">", false))
			return true;
	}
}

bool BaseXMLParser::ReadCDATA(const char* tag_terminator, size_t& content_end)
{
	for (;;)
	{
		// Search for the next tag opening.
		if (!FindString("<", nullptr))
			return false;

		const size_t tag_open = xml_index - 1;
		if (PeekString("/", false))
		{
			const size_t tag_begin = xml_index;
			if (FindString(">", nullptr))
			{
				const char* source = xml_source.data();
				const char* name_begin = source + tag_begin;
				const char* name_end = source + xml_index - 1;
				if (const char* slash = static_cast<const char*>(memchr(name_begin, '/', size_t(name_end - name_begin))))
					name_begin = slash + 1;
				while (name_begin < name_end && StringUtilities::IsWhitespace(*name_begin))
					name_begin++;
				while (name_end > name_begin && StringUtilities::IsWhitespace(*(name_end - 1)))
					name_end--;

				if (BaseXMLParserScan::EqualsLowerCase(name_begin, name_end, tag_terminator))
				{
					content_end = tag_open;
					return true;
				}
			}
		}
	}
}

bool BaseXMLParser::SkipInnerXML(size_t& capture_end)
{
	// The branch ends at the first close tag at its own depth, whatever its name.
	int depth = 1;
	for (;;)
	{
		if (!FindString("<", nullptr, true))
			return false;

		const size_t tag_open = xml_index - 1;
		if (PeekString("!--"))
		{
			if (!FindString("-->", nullptr))
				return false;
		}
		else if (PeekString("![CDATA["))
		{
			FindString("]]>", nullptr);
		}
		else if (PeekString("/"))
		{
			if (depth == 1)
			{
				capture_end = tag_open;
				return true;
			}

			if (!FindString(">", nullptr))
				return false;
			depth--;
		}
		else
		{
			bool section_opened = false;
			if (!SkipOpenTag(section_opened))
				return false;
			if (section_opened)
				depth++;
		}
	}
}

bool BaseXMLParser::SkipOpenTag(bool& section_opened)
{
	size_t name_begin = 0;
	size_t name_end = 0;
	if (!FindWord(name_begin, name_end, "/>"))
		return false;

	if (PeekString(">"))
	{
		section_opened = true;
	}
	else if (!(PeekString("/") && PeekString(">")))
	{
		bool parse_raw_xml_content = false;
		if (!ReadAttributes(nullptr, parse_raw_xml_content))
			return false;

		if (PeekString(">"))
			section_opened = true;
		else if (!(PeekString("/") && PeekString(">")))
			return false;
	}

	line_number_open_tag = line_number;

	if (section_opened)
	{
		if (const String* cdata_tag = FindCDATATag(name_begin, name_end))
		{
			size_t content_end = 0;
			if (!ReadCDATA(cdata_tag->c_str(), content_end))
				return false;
			section_opened = false;
		}
	}

	return true;
}

const String* BaseXMLParser::FindCDATATag(size_t name_begin, size_t name_end) const
{
	const char* source = xml_source.data();
	for (const String& tag : cdata_tags)
	{
		if (BaseXMLParserScan::EqualsLowerCase(source + name_begin, source + name_end, tag.c_str()))
			return &tag;
	}
	return nullptr;
}

bool BaseXMLParser::IsInnerXMLAttribute(const String& name) const
{
	return name.size() >= 5 && name.compare(0, 5, "data-") == 0 && inner_xml_attributes.find(name) != inner_xml_attributes.end();
}

bool BaseXMLParser::FindWord(size_t& word_begin, size_t& word_end, const char* terminators)
{
	const char* source = xml_source.data();
	const size_t source_size = xml_source.size();
	size_t index = xml_index;

	while (index < source_size && StringUtilities::IsWhitespace(source[index]))
	{
		if (source[index] == '\n')
			line_number++;
		index++;
	}

	word_begin = index;
	for (; index < source_size; index++)
	{
		const char c = source[index];

		if (StringUtilities::IsWhitespace(c))
		{
			if (c == '\n')
				line_number++;
			xml_index = index;
			word_end = index;
			return true;
		}

		// Check for termination condition
		if (terminators && strchr(terminators, c))
		{
			xml_index = index;
			word_end = index;
			return index > word_begin;
		}
	}

	xml_index = source_size;
	word_end = source_size;
	return false;
}

bool BaseXMLParser::FindString(const char* string, String* out, bool escape_brackets)
{
	const char first_char = string[0];
	const size_t string_length = strlen(string);
	const char* source = xml_source.data();
	const size_t source_size = xml_source.size();
	const size_t start = xml_index;
	size_t index = start;
	bool found = false;

	if (escape_brackets)
	{
		bool in_brackets = false;
		bool in_string = false;
		for (; index < source_size; index++)
		{
			const char c = source[index];

			// Count line numbers
			if (c == '\n')
				line_number++;

			if (in_brackets || c == '{' || c == '}')
			{
				const char previous = index > start ? source[index - 1] : 0;
				if (const char* error_str = XMLParseTools::ParseDataBrackets(in_brackets, in_string, c, previous))
				{
					Log::Message(Log::LT_WARNING, "XML parse error. %s", error_str);
					if (out)
						out->append(source + start, index - start);
					xml_index = index;
					return false;
				}
			}

			if (c == first_char && !in_brackets && BaseXMLParserScan::MatchesAt(source, source_size, index, string, string_length))
			{
				found = true;
				break;
			}
		}
	}
	else
	{
		while (index < source_size)
		{
			const char* hit = static_cast<const char*>(memchr(source + index, first_char, source_size - index));
			const size_t hit_index = hit ? size_t(hit - source) : source_size;
			line_number += BaseXMLParserScan::CountNewlines(source + index, source + hit_index);
			index = hit_index;
			if (!hit)
				break;

			if (BaseXMLParserScan::MatchesAt(source, source_size, index, string, string_length))
			{
				found = true;
				break;
			}
			index++;
		}
	}

	if (out)
		out->append(source + start, index - start);
	xml_index = found ? index + string_length : index;
	return found;
}

bool BaseXMLParser::PeekString(const char* string, bool consume)
{
	const char* source = xml_source.data();
	const size_t source_size = xml_source.size();
	size_t index = xml_index;
	int new_lines = 0;

	// Seek past all the whitespace if we haven't hit the initial character yet.
	while (index < source_size && StringUtilities::IsWhitespace(source[index]))
	{
		if (source[index] == '\n')
			new_lines++;
		index++;
	}

	for (const char* c = string; *c; c++, index++)
	{
		if (index >= source_size || source[index] != *c)
			return false;
	}

	if (consume)
	{
		xml_index = index;
		line_number += new_lines;
	}

	return true;
}

} // namespace Rml
