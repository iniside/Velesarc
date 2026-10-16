#include "XMLEventStream.h"
#include "ArcPerfInternal.h"
#include "../../Include/RmlUi/Core/CompiledDocument.h"
#include "../../Include/RmlUi/Core/Context.h"
#include "../../Include/RmlUi/Core/Core.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/ElementText.h"
#include "../../Include/RmlUi/Core/Factory.h"
#include "../../Include/RmlUi/Core/Log.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/StreamMemory.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "../../Include/RmlUi/Core/SystemInterface.h"
#include "../../Include/RmlUi/Core/URL.h"
#include "../../Include/RmlUi/Core/XMLParser.h"
#include "DocumentHeader.h"
#include "ElementMeta.h"
#include "XMLParseTools.h"
#include <algorithm>
#include <string.h>

namespace Rml {

namespace XMLEventStreamDetail {

	class BinaryWriter {
	public:
		explicit BinaryWriter(Vector<byte>& out) : out(out) {}

		void Raw(const void* data, size_t size)
		{
			const byte* bytes = static_cast<const byte*>(data);
			out.insert(out.end(), bytes, bytes + size);
		}
		void U8(uint8_t value) { out.push_back(value); }
		void U32(uint32_t value) { Raw(&value, sizeof(value)); }
		void I32(int32_t value) { Raw(&value, sizeof(value)); }
		void Str(const String& value)
		{
			U32(static_cast<uint32_t>(value.size()));
			Raw(value.data(), value.size());
		}

	private:
		Vector<byte>& out;
	};

	class BinaryReader {
	public:
		explicit BinaryReader(Span<const byte> data) : data(data) {}

		bool Raw(void* destination, size_t size)
		{
			if (failed || size > data.size() - position)
			{
				failed = true;
				return false;
			}
			if (size > 0)
				memcpy(destination, data.data() + position, size);
			position += size;
			return true;
		}
		uint8_t U8()
		{
			uint8_t value = 0;
			Raw(&value, sizeof(value));
			return value;
		}
		uint32_t U32()
		{
			uint32_t value = 0;
			Raw(&value, sizeof(value));
			return value;
		}
		int32_t I32()
		{
			int32_t value = 0;
			Raw(&value, sizeof(value));
			return value;
		}
		String Str()
		{
			const uint32_t size = U32();
			if (failed || size > data.size() - position)
			{
				failed = true;
				return String();
			}
			String value(reinterpret_cast<const char*>(data.data() + position), size);
			position += size;
			return value;
		}
		Span<const byte> Bytes(uint32_t size)
		{
			if (failed || size > data.size() - position)
			{
				failed = true;
				return Span<const byte>();
			}
			const Span<const byte> result(data.data() + position, size);
			position += size;
			return result;
		}
		size_t Remaining() const { return data.size() - position; }

		bool Failed() const { return failed; }
		bool AtEnd() const { return position == data.size(); }

	private:
		Span<const byte> data;
		size_t position = 0;
		bool failed = false;
	};

	String TranslateText(const String& in_text)
	{
		String text;
		if (SystemInterface* system_interface = GetSystemInterface())
			system_interface->TranslateString(text, in_text);
		return text;
	}

	bool IsCDATATag(const String& name)
	{
		const SmallUnorderedSet<String>& cdata_tags = XMLParser::GetPersistentCDATATags();
		return cdata_tags.find(StringUtilities::ToLower(name)) != cdata_tags.end();
	}

	String GetAttributeString(const XMLAttributes& attributes, const String& name)
	{
		const XMLAttributes::const_iterator it = attributes.find(name);
		return it != attributes.end() ? it->second.Get<String>() : String();
	}

	const char* FindUnsupportedConstruct(const String& in_name, const XMLAttributes& attributes)
	{
		const String name = StringUtilities::ToLower(in_name);
		if (name == "template")
			return "<template>";
		if (name == "style")
			return "inline <style>";
		if (name == "script")
			return "<script>";
		if (name == "link" && StringUtilities::ToLower(GetAttributeString(attributes, "type")) == "text/template")
			return "<link type=\"text/template\">";
		if (name == "body" && attributes.find("template") != attributes.end())
			return "<body template>";
		return nullptr;
	}

	uint32_t RecordInnerRml(CompiledDocumentData& document, const String& span, const String& source_url, int line_number, String& error);

	class XMLEventRecorder : public BaseXMLParser {
	public:
		XMLEventRecorder(CompiledDocumentData& document, bool frame_mode, const String& source_url, String& error) :
			BaseXMLParser(XMLParser::GetPersistentCDATATags(), Factory::GetStructuralDataViewAttributeNames()), document(document),
			frame_mode(frame_mode), source_url(source_url), error(error)
		{}

		Vector<XMLEvent> TakeEvents() { return std::move(events); }

		void HandleElementStart(const String& name, const XMLAttributes& attributes) override
		{
			if (const char* construct = FindUnsupportedConstruct(name, attributes))
				Fail(CreateString("unsupported %s", construct));

			XMLEvent& event = AddEvent(XMLEventType::Start);
			event.is_frame = frame_mode && open_tags.empty();
			event.text = name;
			event.attributes = attributes;
			open_tags.push_back(name);
		}

		void HandleElementEnd(const String& name) override
		{
			if (!open_tags.empty())
				open_tags.pop_back();
			XMLEvent& event = AddEvent(XMLEventType::End);
			event.is_frame = frame_mode && open_tags.empty();
			event.text = name;
		}

		void HandleData(const String& data, XMLDataType type) override
		{
			if (type != XMLDataType::InnerXML)
			{
				XMLEvent& event = AddEvent(XMLEventType::Data);
				event.data_type = type;
				event.text = data;
				return;
			}

			if (!open_tags.empty() && IsCDATATag(open_tags.back()))
			{
				Fail(CreateString("structural data view on raw-content element <%s>", open_tags.back().c_str()));
				return;
			}

			const uint32_t inner_stream = RecordInnerRml(document, data, source_url, GetLineNumber(), error);
			XMLEvent& event = AddEvent(XMLEventType::Data);
			event.data_type = type;
			event.inner_stream = inner_stream;
		}

	private:
		XMLEvent& AddEvent(XMLEventType type)
		{
			events.emplace_back();
			XMLEvent& event = events.back();
			event.type = type;
			event.line_number = GetLineNumber();
			event.line_number_open_tag = GetLineNumberOpenTag();
			return event;
		}

		void Fail(const String& message)
		{
			if (error.empty())
				error = CreateString("%s:%d: %s", source_url.c_str(), GetLineNumber(), message.c_str());
		}

		CompiledDocumentData& document;
		bool frame_mode;
		const String& source_url;
		String& error;
		Vector<XMLEvent> events;
		StringList open_tags;
	};

	uint32_t RecordInnerRml(CompiledDocumentData& document, const String& span, const String& source_url, int line_number, String& error)
	{
		XMLEventStream stream;
		stream.text = TranslateText(span);

		ElementTextContent content = ElementTextContent::WhiteSpace;
		if (const char* error_str = ClassifyElementText(stream.text, content, stream.has_data_expression))
		{
			if (error.empty())
				error = CreateString("%s:%d: inner RML of a structural data view: %s", source_url.c_str(), line_number, error_str);
			return 0;
		}

		if (content == ElementTextContent::WhiteSpace)
		{
			stream.kind = XMLEventStreamKind::Empty;
			stream.text.clear();
		}
		else if (content == ElementTextContent::Text)
		{
			stream.kind = XMLEventStreamKind::Text;
		}
		else
		{
			stream.kind = XMLEventStreamKind::Children;
			const String wrapped = "<body>" + stream.text + "</body>";
			stream.text.clear();

			StreamMemory inner_source(reinterpret_cast<const byte*>(wrapped.data()), wrapped.size());
			XMLEventRecorder recorder(document, true, source_url, error);
			recorder.Parse(&inner_source);
			stream.events = recorder.TakeEvents();
		}

		document.streams.push_back(std::move(stream));
		return static_cast<uint32_t>(document.streams.size() - 1);
	}

	void CollectTextExpressions(const String& text, const Function<void(const String&, bool)>& collect)
	{
		if (text.find('&') != String::npos)
			DataExpressionTable::ForEachTextExpression(StringUtilities::DecodeRml(text), collect);
		else
			DataExpressionTable::ForEachTextExpression(text, collect);
	}

	void CollectStreamExpressions(const XMLEventStream& stream, const Function<void(const String&, bool)>& collect)
	{
		if (stream.kind == XMLEventStreamKind::Text && stream.has_data_expression)
			CollectTextExpressions(stream.text, collect);

		for (const XMLEvent& event : stream.events)
		{
			if (event.type == XMLEventType::Start)
			{
				for (const XMLAttributes::value_type& attribute : event.attributes)
					DataExpressionTable::ForEachAttributeExpression(attribute.first, attribute.second.Get<String>(), collect);
			}
			else if (event.type == XMLEventType::Data && event.data_type == XMLDataType::Text)
			{
				const String text = TranslateText(event.text);
				ElementTextContent content = ElementTextContent::WhiteSpace;
				bool has_data_expression = false;
				if (!ClassifyElementText(text, content, has_data_expression) && content == ElementTextContent::Text && has_data_expression)
					CollectTextExpressions(text, collect);
			}
		}
	}

	bool CompileExpressions(CompiledDocumentData& document, String& out_error)
	{
		UnorderedSet<String> seen[2];
		bool encodable = true;
		const Function<void(const String&, bool)> collect = [&](const String& expression, bool is_assignment_expression) {
			if (!seen[is_assignment_expression ? 1 : 0].insert(expression).second)
				return;

			CompiledExpression compiled;
			compiled.expression = expression;
			compiled.is_assignment_expression = is_assignment_expression;
			if (!ParseDataExpressionModelFree(expression, is_assignment_expression, compiled.compiled))
				return;

			Vector<byte> program_bytes;
			if (!EncodeDataProgram(*compiled.compiled.program, program_bytes))
			{
				if (encodable)
					out_error = CreateString("data expression '%s' has a literal that cannot be stored", expression.c_str());
				encodable = false;
				return;
			}
			document.expressions.push_back(std::move(compiled));
		};

		for (const XMLEventStream& stream : document.streams)
			CollectStreamExpressions(stream, collect);

		return encodable;
	}

	bool WriteDocument(const CompiledDocumentData& document, Vector<byte>& out_body, String& out_error)
	{
		BinaryWriter out(out_body);
		out.U32(static_cast<uint32_t>(document.streams.size()));
		for (const XMLEventStream& stream : document.streams)
		{
			out.U8(static_cast<uint8_t>(stream.kind));
			out.Str(stream.text);
			out.U8(stream.has_data_expression ? 1 : 0);
			out.U32(static_cast<uint32_t>(stream.events.size()));
			for (const XMLEvent& event : stream.events)
			{
				out.U8(static_cast<uint8_t>(event.type));
				out.U8(static_cast<uint8_t>(event.data_type));
				out.U8(event.is_frame ? 1 : 0);
				out.I32(event.line_number);
				out.I32(event.line_number_open_tag);
				out.Str(event.text);
				out.U32(static_cast<uint32_t>(event.attributes.size()));
				for (const XMLAttributes::value_type& attribute : event.attributes)
				{
					if (attribute.second.GetType() != Variant::STRING)
					{
						out_error = CreateString("attribute '%s' is not a string", attribute.first.c_str());
						return false;
					}
					out.Str(attribute.first);
					out.Str(attribute.second.GetReference<String>());
				}
				out.U32(event.inner_stream);
			}
		}

		out.U32(static_cast<uint32_t>(document.expressions.size()));
		for (const CompiledExpression& expression : document.expressions)
		{
			out.Str(expression.expression);
			out.U8(expression.is_assignment_expression ? 1 : 0);
			out.U32(static_cast<uint32_t>(expression.compiled.address_names.size()));
			for (const String& name : expression.compiled.address_names)
				out.Str(name);

			Vector<byte> program_bytes;
			EncodeDataProgram(*expression.compiled.program, program_bytes);
			out.U32(static_cast<uint32_t>(program_bytes.size()));
			out.Raw(program_bytes.data(), program_bytes.size());
		}
		return true;
	}

	bool ReadEvents(BinaryReader& in, XMLEventStream& stream, uint32_t stream_count)
	{
		const uint32_t event_count = in.U32();
		if (in.Failed() || event_count > in.Remaining())
			return false;

		stream.events.resize(event_count);
		for (XMLEvent& event : stream.events)
		{
			const uint8_t type = in.U8();
			const uint8_t data_type = in.U8();
			if (type > uint8_t(XMLEventType::End) || data_type > uint8_t(XMLDataType::InnerXML))
				return false;
			event.type = XMLEventType(type);
			event.data_type = XMLDataType(data_type);
			event.is_frame = in.U8() != 0;
			event.line_number = in.I32();
			event.line_number_open_tag = in.I32();
			event.text = in.Str();

			const uint32_t attribute_count = in.U32();
			if (in.Failed() || attribute_count > in.Remaining())
				return false;
			for (uint32_t i = 0; i < attribute_count && !in.Failed(); i++)
			{
				String name = in.Str();
				event.attributes[std::move(name)] = in.Str();
			}

			event.inner_stream = in.U32();
			const bool is_inner_xml = (event.type == XMLEventType::Data && event.data_type == XMLDataType::InnerXML);
			if (in.Failed() || (is_inner_xml && (event.inner_stream == 0 || event.inner_stream >= stream_count)))
				return false;
		}
		return true;
	}

	SharedPtr<const CompiledDocumentData> ReadDocument(Span<const byte> body)
	{
		BinaryReader in(body);
		SharedPtr<CompiledDocumentData> document = MakeShared<CompiledDocumentData>();

		const uint32_t stream_count = in.U32();
		if (in.Failed() || stream_count == 0 || stream_count > in.Remaining())
			return nullptr;

		document->streams.resize(stream_count);
		for (uint32_t i = 0; i < stream_count; i++)
		{
			XMLEventStream& stream = document->streams[i];
			const uint8_t kind = in.U8();
			if (kind > uint8_t(XMLEventStreamKind::Children) || ((kind == uint8_t(XMLEventStreamKind::Document)) != (i == 0)))
				return nullptr;
			stream.document = document.get();
			stream.kind = XMLEventStreamKind(kind);
			stream.text = in.Str();
			stream.has_data_expression = in.U8() != 0;
			if (!ReadEvents(in, stream, stream_count))
				return nullptr;
		}

		const uint32_t expression_count = in.U32();
		if (in.Failed() || expression_count > in.Remaining())
			return nullptr;

		document->expressions.resize(expression_count);
		for (CompiledExpression& expression : document->expressions)
		{
			expression.expression = in.Str();
			expression.is_assignment_expression = in.U8() != 0;
			const uint32_t name_count = in.U32();
			if (in.Failed() || name_count > in.Remaining())
				return nullptr;
			expression.compiled.address_names.reserve(name_count);
			for (uint32_t i = 0; i < name_count; i++)
				expression.compiled.address_names.push_back(in.Str());

			const uint32_t program_size = in.U32();
			const Span<const byte> program_bytes = in.Bytes(program_size);
			if (in.Failed())
				return nullptr;
			expression.compiled.program = DecodeDataProgram(program_bytes.data(), program_bytes.size());
			if (!expression.compiled.program)
				return nullptr;
		}

		if (!in.AtEnd())
			return nullptr;

		return document;
	}

	bool HasStructuralAttribute(const XMLAttributes& attributes)
	{
		for (const String& name : Factory::GetStructuralDataViewAttributeNames())
		{
			if (attributes.find(name) != attributes.end())
				return true;
		}
		return false;
	}

	class StructuralDataViewFinder : public BaseXMLParser {
	public:
		StructuralDataViewFinder() : BaseXMLParser(XMLParser::GetPersistentCDATATags(), Factory::GetStructuralDataViewAttributeNames()) {}

		void HandleElementStart(const String& /*name*/, const XMLAttributes& attributes) override
		{
			found |= HasStructuralAttribute(attributes);
		}

		bool found = false;
	};

	bool FindElementEvents(const Vector<XMLEvent>& events, const String& name, size_t& out_begin, size_t& out_end)
	{
		for (size_t begin = 0; begin < events.size(); ++begin)
		{
			if (events[begin].type != XMLEventType::Start || StringUtilities::ToLower(events[begin].text) != name)
				continue;

			int depth = 0;
			for (size_t end = begin; end < events.size(); ++end)
			{
				if (events[end].type == XMLEventType::Start)
					++depth;
				else if (events[end].type == XMLEventType::End && --depth == 0)
				{
					out_begin = begin;
					out_end = end + 1;
					return true;
				}
			}
			return false;
		}
		return false;
	}

} // namespace XMLEventStreamDetail

const char* ClassifyElementText(const String& text, ElementTextContent& out_content, bool& out_has_data_expression)
{
	out_has_data_expression = false;

	if (std::all_of(text.begin(), text.end(), &StringUtilities::IsWhitespace))
	{
		out_content = ElementTextContent::WhiteSpace;
		return nullptr;
	}

	bool parse_as_rml = false;
	bool inside_brackets = false;
	bool inside_string = false;
	char previous = 0;
	for (const char c : text)
	{
		if (const char* error_str = XMLParseTools::ParseDataBrackets(inside_brackets, inside_string, c, previous))
			return error_str;

		if (inside_brackets)
			out_has_data_expression = true;
		else if (c == '<')
			parse_as_rml = true;

		previous = c;
	}

	out_content = parse_as_rml ? ElementTextContent::Rml : ElementTextContent::Text;
	return nullptr;
}

bool InstanceTextElement(Element* parent, String text, bool has_data_expression)
{
	RMLUI_ZoneScopedNC("InstanceText", 0x8FBC8F);

	XMLAttributes attributes;

	// If we have curly brackets in the text, we tag the element so that the appropriate data view (DataViewText) is constructed.
	if (has_data_expression)
		attributes.emplace("data-text", Variant());

	ElementPtr element = Factory::InstanceElement(parent, "#text", "#text", attributes);
	if (!element)
	{
		Log::Message(Log::LT_ERROR, "Failed to instance text element '%s', instancer returned nullptr.", text.c_str());
		return false;
	}

	ElementText* text_element = rmlui_dynamic_cast<ElementText*>(element.get());
	if (!text_element)
	{
		Log::Message(Log::LT_ERROR, "Failed to instance text element '%s'. Found type '%s', was expecting a derivative of ElementText.", text.c_str(),
			rmlui_type_name(*element));
		return false;
	}

	if (text.find('&') != String::npos)
		text = StringUtilities::DecodeRml(text);

	text_element->SetText(text);

	parent->AppendChild(std::move(element));

	return true;
}

void XMLEventReplay::Replay(XMLParser& parser, const SharedPtr<const XMLEventStream>& stream, const String& frame_tag)
{
	ReplayEvents(parser, stream, frame_tag, 0, stream->events.size());
}

void XMLEventReplay::ReplayEvents(XMLParser& parser, const SharedPtr<const XMLEventStream>& stream, const String& frame_tag, size_t begin,
	size_t end)
{
	BaseXMLParser& base = parser;
	for (size_t index = begin; index < end; ++index)
	{
		const XMLEvent& event = stream->events[index];
		parser.SetLineNumbers(event.line_number, event.line_number_open_tag);
		switch (event.type)
		{
		case XMLEventType::Start: base.HandleElementStart(event.is_frame ? frame_tag : event.text, event.attributes); break;
		case XMLEventType::End: base.HandleElementEnd(event.is_frame ? frame_tag : event.text); break;
		case XMLEventType::Data:
			if (event.data_type == XMLDataType::InnerXML)
			{
				parser.compiled_inner_xml = SharedPtr<const XMLEventStream>(stream, &stream->document->streams[event.inner_stream]);
				base.HandleData(String(), XMLDataType::InnerXML);
				parser.compiled_inner_xml.reset();
			}
			else
			{
				base.HandleData(event.text, event.data_type);
			}
			break;
		}
	}
}

void XMLEventReplay::ReplayDocument(XMLParser& parser, const SharedPtr<const CompiledDocumentData>& document, const URL& source_url)
{
	parser.SetSourceURL(&source_url);
	Replay(parser, SharedPtr<const XMLEventStream>(document, &document->streams[0]), String());
	parser.SetSourceURL(nullptr);
}

Element* XMLEventReplay::ReplayDocumentBody(Element* parent, const SharedPtr<const CompiledDocumentData>& document, const URL& source_url,
	DocumentHeader& out_header)
{
	using namespace XMLEventStreamDetail;

	const SharedPtr<const XMLEventStream> stream(document, &document->streams[0]);

	size_t body_begin = 0;
	size_t body_end = 0;
	if (!FindElementEvents(stream->events, "body", body_begin, body_end))
	{
		Log::Message(Log::LT_ERROR, "Failed to instance compiled document %s into %s: the document has no body.", source_url.GetURL().c_str(),
			parent->GetAddress().c_str());
		return nullptr;
	}

	size_t head_begin = 0;
	size_t head_end = 0;
	if (FindElementEvents(stream->events, "head", head_begin, head_end))
	{
		XMLParser head_parser(nullptr);
		head_parser.SetSourceURL(&source_url);
		ReplayEvents(head_parser, stream, String(), head_begin, head_end);
		head_parser.SetSourceURL(nullptr);
		out_header = *head_parser.GetDocumentHeader();
	}

	ElementPtr wrapper = Factory::InstanceElement(parent, "div", "div", stream->events[body_begin].attributes);
	if (!wrapper)
	{
		Log::Message(Log::LT_ERROR, "Failed to instance compiled document %s into %s, instancer returned nullptr.", source_url.GetURL().c_str(),
			parent->GetAddress().c_str());
		return nullptr;
	}

	Element* body = parent->AppendChild(std::move(wrapper));

	XMLParser body_parser(body);
	body_parser.SetSourceURL(&source_url);
	ReplayEvents(body_parser, stream, String(), body_begin, body_end);
	body_parser.SetSourceURL(nullptr);

	return body;
}

void XMLEventReplay::InstanceInnerRml(Element* parent, const SharedPtr<const XMLEventStream>& stream)
{
	RMLUI_ASSERT(parent && stream);
	RMLUI_ARC_PERF_SCOPE(TextInstance);
	RMLUI_ARC_PERF_COUNT(TextNodesInstanced, 1);
	switch (stream->kind)
	{
	case XMLEventStreamKind::Empty: break;
	case XMLEventStreamKind::Text: InstanceTextElement(parent, stream->text, stream->has_data_expression); break;
	case XMLEventStreamKind::Document:
	case XMLEventStreamKind::Children:
	{
		Context* context = parent->GetContext();
		const String frame_tag = context ? context->GetDocumentsBaseTag() : "body";
		const URL source_url;
		XMLParser parser(parent);
		parser.SetSourceURL(&source_url);
		// The frame's body handler sets the default handler for the children, as the body wrapper of InstanceElementText does.
		Replay(parser, stream, frame_tag);
		parser.SetSourceURL(nullptr);
	}
	break;
	}
}

const SharedPtr<const XMLEventStream>& CompiledInnerRmlAccess::Get(const Element& element)
{
	return element.meta->compiled_inner_rml;
}

void CompiledInnerRmlAccess::Set(Element& element, const SharedPtr<const XMLEventStream>& stream)
{
	element.meta->compiled_inner_rml = stream;
}

bool HasStructuralDataView(const XMLEventStream& stream)
{
	for (const XMLEvent& event : stream.events)
	{
		if (event.type == XMLEventType::Start && XMLEventStreamDetail::HasStructuralAttribute(event.attributes))
			return true;
	}
	return false;
}

bool HasStructuralDataView(const String& inner_rml)
{
	const String wrapped = "<body>" + inner_rml + "</body>";
	StreamMemory source(reinterpret_cast<const byte*>(wrapped.data()), wrapped.size());
	XMLEventStreamDetail::StructuralDataViewFinder finder;
	finder.Parse(&source);
	return finder.found;
}

void SeedDataParseMemo(DataParseMemo& memo, const CompiledDocumentData& document)
{
	for (const CompiledExpression& expression : document.expressions)
		memo.SeedProgram(expression.expression, expression.is_assignment_expression, expression.compiled);
}

bool CompileDocument(const String& document_rml, const String& source_url, Vector<byte>& out_body, String& out_error)
{
	using namespace XMLEventStreamDetail;

	CompiledDocumentData document;
	document.streams.emplace_back();

	String error;
	{
		StreamMemory source(reinterpret_cast<const byte*>(document_rml.data()), document_rml.size());
		source.SetSourceURL(source_url);
		XMLEventRecorder recorder(document, false, source_url, error);
		recorder.Parse(&source);
		document.streams[0].events = recorder.TakeEvents();
	}

	if (error.empty() && !CompileExpressions(document, error))
		error = source_url + ": " + error;

	if (error.empty() && !WriteDocument(document, out_body, error))
		error = source_url + ": " + error;

	if (!error.empty())
	{
		out_error = std::move(error);
		out_body.clear();
		return false;
	}
	return true;
}

bool ReadCompiledDocument(Span<const byte> body, CompiledDocument& out_document, String& out_error)
{
	SharedPtr<const CompiledDocumentData> document = XMLEventStreamDetail::ReadDocument(body);
	if (!document)
	{
		out_error = "malformed compiled document body";
		return false;
	}
	out_document = CompiledDocument(std::move(document));
	return true;
}

void GetCompiledDocumentStyleSheetLinks(const CompiledDocument& document, StringList& out_hrefs)
{
	using namespace XMLEventStreamDetail;

	out_hrefs.clear();
	const SharedPtr<const CompiledDocumentData>& data = document.GetData();
	if (!data || data->streams.empty())
		return;

	bool in_head = false;
	for (const XMLEvent& event : data->streams[0].events)
	{
		if (event.type == XMLEventType::Data)
			continue;
		const String name = StringUtilities::ToLower(event.text);
		if (name == "head")
			in_head = (event.type == XMLEventType::Start);
		if (!in_head || event.type == XMLEventType::End || name != "link")
			continue;

		const String type = StringUtilities::ToLower(GetAttributeString(event.attributes, "type"));
		const String href = GetAttributeString(event.attributes, "href");
		if ((type == "text/rcss" || type == "text/css") && !href.empty())
			out_hrefs.push_back(href);
	}
}

} // namespace Rml
