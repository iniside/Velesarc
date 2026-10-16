#pragma once

#include "../../Include/RmlUi/Core/BaseXMLParser.h"
#include "../../Include/RmlUi/Core/Types.h"
#include "DataExpression.h"

namespace Rml {

class DocumentHeader;
class Element;
class URL;
class XMLParser;
struct CompiledDocumentData;

enum class XMLEventType : uint8_t { Start, Data, End };

struct XMLEvent {
	XMLEventType type = XMLEventType::Start;
	XMLDataType data_type = XMLDataType::Text;
	bool is_frame = false;
	int line_number = 0;
	int line_number_open_tag = 0;
	String text;
	XMLAttributes attributes;
	uint32_t inner_stream = 0;
};

enum class XMLEventStreamKind : uint8_t { Document, Empty, Text, Children };

struct XMLEventStream {
	const CompiledDocumentData* document = nullptr;
	XMLEventStreamKind kind = XMLEventStreamKind::Document;
	String text;
	bool has_data_expression = false;
	Vector<XMLEvent> events;
};

struct CompiledExpression {
	String expression;
	bool is_assignment_expression = false;
	DataParseMemo::CompiledProgram compiled;
};

struct CompiledDocumentData {
	Vector<XMLEventStream> streams;
	Vector<CompiledExpression> expressions;
};

enum class ElementTextContent : uint8_t { WhiteSpace, Text, Rml };

const char* ClassifyElementText(const String& text, ElementTextContent& out_content, bool& out_has_data_expression);
bool InstanceTextElement(Element* parent, String text, bool has_data_expression);

struct XMLEventReplay {
	static void ReplayDocument(XMLParser& parser, const SharedPtr<const CompiledDocumentData>& document, const URL& source_url);
	static void InstanceInnerRml(Element* parent, const SharedPtr<const XMLEventStream>& stream);
	static Element* ReplayDocumentBody(Element* parent, const SharedPtr<const CompiledDocumentData>& document, const URL& source_url,
		DocumentHeader& out_header);

private:
	static void Replay(XMLParser& parser, const SharedPtr<const XMLEventStream>& stream, const String& frame_tag);
	static void ReplayEvents(XMLParser& parser, const SharedPtr<const XMLEventStream>& stream, const String& frame_tag, size_t begin, size_t end);
};

struct CompiledInnerRmlAccess {
	static const SharedPtr<const XMLEventStream>& Get(const Element& element);
	static void Set(Element& element, const SharedPtr<const XMLEventStream>& stream);
};

bool HasStructuralDataView(const XMLEventStream& stream);
bool HasStructuralDataView(const String& inner_rml);

void SeedDataParseMemo(DataParseMemo& memo, const CompiledDocumentData& document);

} // namespace Rml
