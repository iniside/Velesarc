#pragma once

#include "Dictionary.h"
#include "Header.h"
#include "Types.h"

namespace Rml {

class Stream;
class URL;
using XMLAttributes = Dictionary;

enum class XMLDataType { Text, CDATA, InnerXML };

class RMLUICORE_API BaseXMLParser {
public:
	/// Tags in cdata_tags are lower case; every name in inner_xml_attributes starts with "data-".
	BaseXMLParser(const SmallUnorderedSet<String>& cdata_tags, const SmallUnorderedSet<String>& inner_xml_attributes);
	virtual ~BaseXMLParser();

	/// Parses the given stream as an XML file, and calls the handlers when
	/// interesting phenomena are encountered.
	void Parse(Stream* stream);

	/// Get the line number in the stream.
	/// @return The line currently being processed in the XML stream.
	int GetLineNumber() const;
	/// Get the line number of the last open tag in the stream.
	int GetLineNumberOpenTag() const;

	void SetSourceURL(const URL* url);
	void SetLineNumbers(int line_number, int line_number_open_tag);

	/// Called when the parser finds the beginning of an element tag.
	virtual void HandleElementStart(const String& name, const XMLAttributes& attributes);
	/// Called when the parser finds the end of an element tag.
	virtual void HandleElementEnd(const String& name);
	/// Called when the parser encounters data.
	virtual void HandleData(const String& data, XMLDataType type);

protected:
	const URL* GetSourceURLPtr() const;

private:
	const URL* source_url = nullptr;
	String xml_source;
	size_t xml_index = 0;

	bool AtEnd() const;

	void HandleElementStartInternal(const String& name, const XMLAttributes& attributes);

	void ReadHeader();
	void ReadBody();
	bool ReadOpenTag(bool& closed_inner_xml);
	bool ReadCloseTag();
	bool ReadCloseTagName();
	bool ReadAttributes(XMLAttributes* attributes, bool& parse_raw_xml_content);
	bool ReadCDATA(const char* tag_terminator, size_t& content_end);

	bool SkipInnerXML(size_t& capture_end);
	bool SkipOpenTag(bool& section_opened);

	const String* FindCDATATag(size_t name_begin, size_t name_end) const;
	bool IsInnerXMLAttribute(const String& name) const;

	bool FindWord(size_t& word_begin, size_t& word_end, const char* terminators = nullptr);
	bool FindString(const char* string, String* data, bool escape_brackets = false);
	bool PeekString(const char* string, bool consume = true);

	int line_number = 0;
	int line_number_open_tag = 0;
	int open_tag_depth = 0;

	// The loose data being read.
	String data;

	const SmallUnorderedSet<String>& cdata_tags;
	const SmallUnorderedSet<String>& inner_xml_attributes;
};

} // namespace Rml
