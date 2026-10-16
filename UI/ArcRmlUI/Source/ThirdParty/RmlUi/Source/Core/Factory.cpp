#include "../../Include/RmlUi/Core/Factory.h"
#include "ArcPerfInternal.h"
#include "../../Include/RmlUi/Core/CompiledDocument.h"
#include "../../Include/RmlUi/Core/Context.h"
#include "../../Include/RmlUi/Core/ContextInstancer.h"
#include "../../Include/RmlUi/Core/Core.h"
#include "../../Include/RmlUi/Core/ElementDocument.h"
#include "../../Include/RmlUi/Core/ElementInstancer.h"
#include "../../Include/RmlUi/Core/ElementText.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/Elements/ElementForm.h"
#include "../../Include/RmlUi/Core/Elements/ElementFormControlInput.h"
#include "../../Include/RmlUi/Core/Elements/ElementFormControlSelect.h"
#include "../../Include/RmlUi/Core/Elements/ElementFormControlTextArea.h"
#include "../../Include/RmlUi/Core/Elements/ElementProgress.h"
#include "../../Include/RmlUi/Core/Elements/ElementTabSet.h"
#include "../../Include/RmlUi/Core/EventListenerInstancer.h"
#include "../../Include/RmlUi/Core/StreamMemory.h"
#include "../../Include/RmlUi/Core/StyleSheet.h"
#include "../../Include/RmlUi/Core/SystemInterface.h"
#include "../../Include/RmlUi/Core/URL.h"
#include "ContextInstancerDefault.h"
#include "ControlledLifetimeResource.h"
#include "DataControllerDefault.h"
#include "DataViewDefault.h"
#include "DecoratorGradient.h"
#include "DecoratorNinePatch.h"
#include "DecoratorShader.h"
#include "DecoratorText.h"
#include "DecoratorTiledBox.h"
#include "DecoratorTiledHorizontal.h"
#include "DecoratorTiledImage.h"
#include "DecoratorTiledVertical.h"
#include "DocumentHeader.h"
#include "ElementHandle.h"
#include "Elements/ElementImage.h"
#include "Elements/ElementLabel.h"
#include "Elements/ElementTextSelection.h"
#include "Elements/XMLNodeHandlerSelect.h"
#include "Elements/XMLNodeHandlerTabSet.h"
#include "Elements/XMLNodeHandlerTextArea.h"
#include "EventInstancerDefault.h"
#include "FilterBasic.h"
#include "FilterBlur.h"
#include "FilterDropShadow.h"
#include "FontEffectBlur.h"
#include "FontEffectGlow.h"
#include "FontEffectOutline.h"
#include "FontEffectShadow.h"
#include "PluginRegistry.h"
#include "StyleSheetFactory.h"
#include "TemplateCache.h"
#include "XMLNodeHandlerBody.h"
#include "XMLNodeHandlerDefault.h"
#include "XMLNodeHandlerHead.h"
#include "XMLNodeHandlerTemplate.h"
#include "XMLEventStream.h"
#include "XMLParseTools.h"
#include <algorithm>

namespace Rml {

// Default instancers are constructed and destroyed on Initialise and Shutdown, respectively.
struct DefaultInstancers {
	UniquePtr<ContextInstancer> context_default;
	UniquePtr<EventInstancer> event_default;

	// Basic elements
	ElementInstancerElement element_default;
	ElementInstancerText element_text;
	ElementInstancerGeneric<ElementImage> element_img;
	ElementInstancerGeneric<ElementHandle> element_handle;
	ElementInstancerGeneric<ElementDocument> element_body;

	// Control elements
	ElementInstancerGeneric<ElementForm> form;
	ElementInstancerGeneric<ElementFormControlInput> input;
	ElementInstancerGeneric<ElementFormControlSelect> select;
	ElementInstancerGeneric<ElementLabel> element_label;

	ElementInstancerGeneric<ElementFormControlTextArea> textarea;
	ElementInstancerGeneric<ElementTextSelection> selection;
	ElementInstancerGeneric<ElementTabSet> tabset;

	ElementInstancerGeneric<ElementProgress> progress;

	// Decorators
	DecoratorTextInstancer decorator_text;
	DecoratorTiledHorizontalInstancer decorator_tiled_horizontal;
	DecoratorTiledVerticalInstancer decorator_tiled_vertical;
	DecoratorTiledBoxInstancer decorator_tiled_box;
	DecoratorTiledImageInstancer decorator_image;
	DecoratorNinePatchInstancer decorator_ninepatch;
	DecoratorShaderInstancer decorator_shader;
	DecoratorStraightGradientInstancer decorator_straight_gradient;
	DecoratorLinearGradientInstancer decorator_linear_gradient;
	DecoratorRadialGradientInstancer decorator_radial_gradient;
	DecoratorConicGradientInstancer decorator_conic_gradient;

	// Filters
	FilterBasicInstancer filter_hue_rotate = {FilterBasicInstancer::ValueType::Angle, "0rad"};
	FilterBasicInstancer filter_basic_d0 = {FilterBasicInstancer::ValueType::NumberPercent, "0"};
	FilterBasicInstancer filter_basic_d1 = {FilterBasicInstancer::ValueType::NumberPercent, "1"};
	FilterBlurInstancer filter_blur;
	FilterDropShadowInstancer filter_drop_shadow;

	// Font effects
	FontEffectBlurInstancer font_effect_blur;
	FontEffectGlowInstancer font_effect_glow;
	FontEffectOutlineInstancer font_effect_outline;
	FontEffectShadowInstancer font_effect_shadow;

	// Data binding views
	DataViewInstancerDefault<DataViewAttribute> data_view_attribute;
	DataViewInstancerDefault<DataViewAttributeIf> data_view_attribute_if;
	DataViewInstancerDefault<DataViewClass> data_view_class;
	DataViewInstancerDefault<DataViewVisible> data_view_visible;
	DataViewInstancerDefault<DataViewRml> data_view_rml;
	DataViewInstancerDefault<DataViewStyle> data_view_style;
	DataViewInstancerDefault<DataViewText> data_view_text;
	DataViewInstancerDefault<DataViewValue> data_view_value;
	DataViewInstancerDefault<DataViewChecked> data_view_checked;
	DataViewInstancerDefault<DataViewWorld> data_view_world;
	DataViewInstancerDefault<DataViewAlias> data_view_alias;

	DataViewInstancerDefault<DataViewFor> structural_data_view_for;
	DataViewInstancerDefault<DataViewIf> structural_data_view_if;

	// Data binding controllers
	DataControllerInstancerDefault<DataControllerEvent> data_controller_event;
	DataControllerInstancerDefault<DataControllerValue> data_controller_value;
};

struct FactoryData {
	DefaultInstancers default_instancers;
	UnorderedMap<String, ElementInstancer*> element_instancers;
	UnorderedMap<String, DecoratorInstancer*> decorator_instancers;
	UnorderedMap<String, FilterInstancer*> filter_instancers;
	UnorderedMap<String, FontEffectInstancer*> font_effect_instancers;
	UnorderedMap<String, DataViewInstancer*> data_view_instancers;
	UnorderedMap<String, DataControllerInstancer*> data_controller_instancers;
	SmallUnorderedSet<String> structural_data_view_attribute_names;
};

static ControlledLifetimeResource<FactoryData> factory_data;

static ContextInstancer* context_instancer = nullptr;
static EventInstancer* event_instancer = nullptr;
static EventListenerInstancer* event_listener_instancer = nullptr;

Factory::Factory() {}

Factory::~Factory() {}

void Factory::Initialise()
{
	factory_data.Initialize();

	DefaultInstancers& default_instancers = factory_data->default_instancers;

	// Default context instancer
	if (!context_instancer)
	{
		default_instancers.context_default = MakeUnique<ContextInstancerDefault>();
		context_instancer = default_instancers.context_default.get();
	}

	// Default event instancer
	if (!event_instancer)
	{
		default_instancers.event_default = MakeUnique<EventInstancerDefault>();
		event_instancer = default_instancers.event_default.get();
	}

	// No default event listener instancer
	if (!event_listener_instancer)
		event_listener_instancer = nullptr;

	// Basic element instancers
	RegisterElementInstancer("*", &default_instancers.element_default);
	RegisterElementInstancer("img", &default_instancers.element_img);
	RegisterElementInstancer("#text", &default_instancers.element_text);
	RegisterElementInstancer("handle", &default_instancers.element_handle);
	RegisterElementInstancer("body", &default_instancers.element_body);

	// Control element instancers
	RegisterElementInstancer("form", &default_instancers.form);
	RegisterElementInstancer("input", &default_instancers.input);
	RegisterElementInstancer("select", &default_instancers.select);
	RegisterElementInstancer("label", &default_instancers.element_label);

	RegisterElementInstancer("textarea", &default_instancers.textarea);
	RegisterElementInstancer("#selection", &default_instancers.selection);
	RegisterElementInstancer("tabset", &default_instancers.tabset);

	RegisterElementInstancer("progress", &default_instancers.progress);
	RegisterElementInstancer("progressbar", &default_instancers.progress);

	// Decorator instancers
	RegisterDecoratorInstancer("text", &default_instancers.decorator_text);
	RegisterDecoratorInstancer("tiled-horizontal", &default_instancers.decorator_tiled_horizontal);
	RegisterDecoratorInstancer("tiled-vertical", &default_instancers.decorator_tiled_vertical);
	RegisterDecoratorInstancer("tiled-box", &default_instancers.decorator_tiled_box);
	RegisterDecoratorInstancer("image", &default_instancers.decorator_image);
	RegisterDecoratorInstancer("ninepatch", &default_instancers.decorator_ninepatch);
	RegisterDecoratorInstancer("shader", &default_instancers.decorator_shader);

	RegisterDecoratorInstancer("gradient", &default_instancers.decorator_straight_gradient);
	RegisterDecoratorInstancer("horizontal-gradient", &default_instancers.decorator_straight_gradient);
	RegisterDecoratorInstancer("vertical-gradient", &default_instancers.decorator_straight_gradient);

	RegisterDecoratorInstancer("linear-gradient", &default_instancers.decorator_linear_gradient);
	RegisterDecoratorInstancer("repeating-linear-gradient", &default_instancers.decorator_linear_gradient);
	RegisterDecoratorInstancer("radial-gradient", &default_instancers.decorator_radial_gradient);
	RegisterDecoratorInstancer("repeating-radial-gradient", &default_instancers.decorator_radial_gradient);
	RegisterDecoratorInstancer("conic-gradient", &default_instancers.decorator_conic_gradient);
	RegisterDecoratorInstancer("repeating-conic-gradient", &default_instancers.decorator_conic_gradient);

	// Filter instancers
	RegisterFilterInstancer("hue-rotate", &default_instancers.filter_hue_rotate);
	RegisterFilterInstancer("brightness", &default_instancers.filter_basic_d1);
	RegisterFilterInstancer("contrast", &default_instancers.filter_basic_d1);
	RegisterFilterInstancer("grayscale", &default_instancers.filter_basic_d0);
	RegisterFilterInstancer("invert", &default_instancers.filter_basic_d0);
	RegisterFilterInstancer("opacity", &default_instancers.filter_basic_d1);
	RegisterFilterInstancer("saturate", &default_instancers.filter_basic_d1);
	RegisterFilterInstancer("sepia", &default_instancers.filter_basic_d0);

	RegisterFilterInstancer("blur", &default_instancers.filter_blur);
	RegisterFilterInstancer("drop-shadow", &default_instancers.filter_drop_shadow);

	// Font effect instancers
	RegisterFontEffectInstancer("blur", &default_instancers.font_effect_blur);
	RegisterFontEffectInstancer("glow", &default_instancers.font_effect_glow);
	RegisterFontEffectInstancer("outline", &default_instancers.font_effect_outline);
	RegisterFontEffectInstancer("shadow", &default_instancers.font_effect_shadow);

	// Data binding views
	// clang-format off
	RegisterDataViewInstancer(&default_instancers.data_view_attribute,      "attr",    false);
	RegisterDataViewInstancer(&default_instancers.data_view_attribute_if,   "attrif",  false);
	RegisterDataViewInstancer(&default_instancers.data_view_class,          "class",   false);
	RegisterDataViewInstancer(&default_instancers.structural_data_view_if,  "if",      true );
	RegisterDataViewInstancer(&default_instancers.data_view_visible,        "visible", false);
	RegisterDataViewInstancer(&default_instancers.data_view_rml,            "rml",     false);
	RegisterDataViewInstancer(&default_instancers.data_view_style,          "style",   false);
	RegisterDataViewInstancer(&default_instancers.data_view_text,           "text",    false);
	RegisterDataViewInstancer(&default_instancers.data_view_value,          "value",   false);
	RegisterDataViewInstancer(&default_instancers.data_view_checked,        "checked", false);
	RegisterDataViewInstancer(&default_instancers.data_view_world,          "world",   false);
	RegisterDataViewInstancer(&default_instancers.data_view_alias,          "alias",   false);
	RegisterDataViewInstancer(&default_instancers.structural_data_view_for, "for",     true );
	// clang-format on

	// Data binding controllers
	RegisterDataControllerInstancer(&default_instancers.data_controller_value, "checked");
	RegisterDataControllerInstancer(&default_instancers.data_controller_event, "event");
	RegisterDataControllerInstancer(&default_instancers.data_controller_value, "value");

	// XML nodes that only contain CDATA
	XMLParser::RegisterPersistentCDATATag("script");
	XMLParser::RegisterPersistentCDATATag("style");

	// XML node handlers
	XMLParser::RegisterNodeHandler("", MakeShared<XMLNodeHandlerDefault>());
	XMLParser::RegisterNodeHandler("body", MakeShared<XMLNodeHandlerBody>());
	XMLParser::RegisterNodeHandler("head", MakeShared<XMLNodeHandlerHead>());
	XMLParser::RegisterNodeHandler("template", MakeShared<XMLNodeHandlerTemplate>());

	// XML node handlers for control elements
	XMLParser::RegisterNodeHandler("tabset", MakeShared<XMLNodeHandlerTabSet>());
	XMLParser::RegisterNodeHandler("textarea", MakeShared<XMLNodeHandlerTextArea>());
	XMLParser::RegisterNodeHandler("select", MakeShared<XMLNodeHandlerSelect>());
}

void Factory::Shutdown()
{
	context_instancer = nullptr;
	event_listener_instancer = nullptr;
	event_instancer = nullptr;

	XMLParser::ReleaseHandlers();

	factory_data.Shutdown();
}

void Factory::RegisterContextInstancer(ContextInstancer* instancer)
{
	context_instancer = instancer;
}

ContextPtr Factory::InstanceContext(const String& name, RenderManager* render_manager, TextInputHandler* text_input_handler)
{
	ContextPtr new_context = context_instancer->InstanceContext(name, render_manager, text_input_handler);
	if (new_context)
		new_context->SetInstancer(context_instancer);
	return new_context;
}

void Factory::RegisterElementInstancer(const String& name, ElementInstancer* instancer)
{
	factory_data->element_instancers[StringUtilities::ToLower(name)] = instancer;
}

ElementInstancer* Factory::GetElementInstancer(const String& tag)
{
	auto instancer_iterator = factory_data->element_instancers.find(tag);
	if (instancer_iterator == factory_data->element_instancers.end())
	{
		instancer_iterator = factory_data->element_instancers.find("*");
		if (instancer_iterator == factory_data->element_instancers.end())
			return nullptr;
	}

	return instancer_iterator->second;
}

ElementPtr Factory::InstanceElement(Element* parent, const String& instancer_name, const String& tag, const XMLAttributes& attributes)
{
	RMLUI_ARC_PERF_SCOPE(ElementInstance);
	RMLUI_ARC_PERF_COUNT(ElementsInstanced, 1);
	RMLUI_ARC_PERF_CODE(const ArcPerf::Stopwatch arc_perf_stopwatch(ArcPerf::Enabled());
		struct ArcPerfRecordTag {
			const ArcPerf::Stopwatch& stopwatch;
			const String& tag;
			~ArcPerfRecordTag()
			{
				if (ArcPerf::Enabled())
					ArcPerf::RecordNamed(ArcPerf::NamedList::InstancedTags, tag, stopwatch.ElapsedMs(), 1);
			}
		} arc_perf_record_tag{arc_perf_stopwatch, tag};)
	if (ElementInstancer* instancer = GetElementInstancer(instancer_name))
	{
		if (ElementPtr element = instancer->InstanceElement(parent, tag, attributes))
		{
			element->SetInstancer(instancer);
			element->SetAttributes(attributes);

			PluginRegistry::NotifyElementCreate(element.get());
			return element;
		}
	}

	return nullptr;
}

bool Factory::InstanceElementText(Element* parent, const String& in_text)
{
	RMLUI_ASSERT(parent);
	RMLUI_ARC_PERF_SCOPE(TextInstance);
	RMLUI_ARC_PERF_COUNT(TextNodesInstanced, 1);

	String text;
	if (SystemInterface* system_interface = GetSystemInterface())
		system_interface->TranslateString(text, in_text);

	ElementTextContent content = ElementTextContent::WhiteSpace;
	bool has_data_expression = false;
	if (const char* error_str = ClassifyElementText(text, content, has_data_expression))
	{
		Log::Message(Log::LT_WARNING, "Failed to instance text element '%s'. %s", text.c_str(), error_str);
		return false;
	}

	// If this text node only contains white-space we don't want to construct it.
	if (content == ElementTextContent::WhiteSpace)
		return true;

	if (content == ElementTextContent::Text)
		return InstanceTextElement(parent, std::move(text), has_data_expression);

	// If the text contains RML elements then run it through the XML parser again.
	RMLUI_ZoneScopedNC("InstanceStream", 0xDC143C);
	UniquePtr<StreamMemory> stream = MakeUnique<StreamMemory>(text.size() + 32);
	Context* context = parent->GetContext();
	String tag = context ? context->GetDocumentsBaseTag() : "body";
	String open_tag = "<" + tag + ">";
	String close_tag = "</" + tag + ">";
	stream->Write(open_tag.c_str(), open_tag.size());
	stream->Write(text);
	stream->Write(close_tag.c_str(), close_tag.size());
	stream->Seek(0, SEEK_SET);

	InstanceElementStream(parent, stream.get());

	return true;
}

bool Factory::InstanceElementStream(Element* parent, Stream* stream)
{
	XMLParser parser(parent);
	parser.Parse(stream);
	return true;
}

ElementPtr Factory::InstanceDocumentCompiled(Context* context, const CompiledDocument& compiled_document, const URL& source_url,
	const String& document_base_tag)
{
	RMLUI_ZoneScoped;

	ElementPtr element = Factory::InstanceElement(nullptr, document_base_tag, document_base_tag, XMLAttributes());
	if (!element)
	{
		Log::Message(Log::LT_ERROR, "Failed to instance document, instancer returned nullptr.");
		return nullptr;
	}

	ElementDocument* document = rmlui_dynamic_cast<ElementDocument*>(element.get());
	if (!document)
	{
		Log::Message(Log::LT_ERROR, "Failed to instance document element. Found type '%s', was expecting derivative of ElementDocument.",
			rmlui_type_name(*element));
		return nullptr;
	}

	document->context = context;

	XMLParser parser(element.get());
	XMLEventReplay::ReplayDocument(parser, compiled_document.GetData(), source_url);

	return element;
}

Element* Factory::InstanceDocumentCompiledBody(Element* parent, const CompiledDocument& compiled_document, const String& source_url,
	SharedPtr<StyleSheetContainer>& out_style_sheet)
{
	RMLUI_ZoneScoped;
	RMLUI_ASSERT(parent && compiled_document.GetData());

	out_style_sheet.reset();

	if (!parent->GetContext())
	{
		Log::Message(Log::LT_ERROR, "Failed to instance compiled document %s into %s: the element is not in a context.", source_url.c_str(),
			parent->GetAddress().c_str());
		return nullptr;
	}

	const URL url(source_url);
	DocumentHeader document_header;
	Element* body = XMLEventReplay::ReplayDocumentBody(parent, compiled_document.GetData(), url, document_header);
	if (!body)
		return nullptr;

	out_style_sheet = LoadDocumentHeaderStyleSheet(MergeDocumentHeaderTemplates(document_header));
	return body;
}

void Factory::RegisterDecoratorInstancer(const String& name, DecoratorInstancer* instancer)
{
	RMLUI_ASSERT(instancer);
	factory_data->decorator_instancers[StringUtilities::ToLower(name)] = instancer;
}

DecoratorInstancer* Factory::GetDecoratorInstancer(const String& name)
{
	auto iterator = factory_data->decorator_instancers.find(name);
	if (iterator == factory_data->decorator_instancers.end())
		return nullptr;

	return iterator->second;
}

void Factory::RegisterFilterInstancer(const String& name, FilterInstancer* instancer)
{
	RMLUI_ASSERT(instancer);
	factory_data->filter_instancers[StringUtilities::ToLower(name)] = instancer;
}

FilterInstancer* Factory::GetFilterInstancer(const String& name)
{
	auto iterator = factory_data->filter_instancers.find(name);
	if (iterator == factory_data->filter_instancers.end())
		return nullptr;

	return iterator->second;
}

void Factory::RegisterFontEffectInstancer(const String& name, FontEffectInstancer* instancer)
{
	RMLUI_ASSERT(instancer);
	factory_data->font_effect_instancers[StringUtilities::ToLower(name)] = instancer;
}

FontEffectInstancer* Factory::GetFontEffectInstancer(const String& name)
{
	auto iterator = factory_data->font_effect_instancers.find(name);
	if (iterator == factory_data->font_effect_instancers.end())
		return nullptr;

	return iterator->second;
}

void Factory::ClearStyleSheetCache()
{
	StyleSheetFactory::ClearStyleSheetCache();
}

void Factory::ClearTemplateCache()
{
	TemplateCache::Clear();
}

void Factory::RegisterEventInstancer(EventInstancer* instancer)
{
	event_instancer = instancer;
}

EventPtr Factory::InstanceEvent(Element* target, EventId id, const String& type, const Dictionary& parameters, bool interruptible)
{
	EventPtr event = event_instancer->InstanceEvent(target, id, type, parameters, interruptible);
	if (event)
		event->instancer = event_instancer;
	return event;
}

void Factory::RegisterEventListenerInstancer(EventListenerInstancer* instancer)
{
	event_listener_instancer = instancer;
}

EventListener* Factory::InstanceEventListener(const String& value, Element* element)
{
	// If we have an event listener instancer, use it
	if (event_listener_instancer)
		return event_listener_instancer->InstanceEventListener(value, element);

	return nullptr;
}

void Factory::RegisterDataViewInstancer(DataViewInstancer* instancer, const String& name, bool is_structural_view)
{
	const bool inserted = factory_data->data_view_instancers.emplace(name, instancer).second;
	if (!inserted)
	{
		Log::Message(Log::LT_WARNING, "Could not register data view instancer '%s'. The given name is already registered.", name.c_str());
		return;
	}
	if (is_structural_view)
		factory_data->structural_data_view_attribute_names.emplace("data-" + name);
}

void Factory::RegisterDataControllerInstancer(DataControllerInstancer* instancer, const String& name)
{
	bool inserted = factory_data->data_controller_instancers.emplace(name, instancer).second;
	if (!inserted)
		Log::Message(Log::LT_WARNING, "Could not register data controller instancer '%s'. The given name is already registered.", name.c_str());
}

DataViewPtr Factory::InstanceDataView(const String& type_name, Element* element)
{
	RMLUI_ASSERT(element);
	const auto it = factory_data->data_view_instancers.find(type_name);
	if (it != factory_data->data_view_instancers.end())
		return it->second->InstanceView(element);
	return nullptr;
}

DataControllerPtr Factory::InstanceDataController(const String& type_name, Element* element)
{
	const auto it = factory_data->data_controller_instancers.find(type_name);
	if (it != factory_data->data_controller_instancers.end())
		return it->second->InstanceController(element);
	return nullptr;
}

bool Factory::IsStructuralDataView(const String& type_name)
{
	const String attribute = "data-" + type_name;
	return factory_data->structural_data_view_attribute_names.find(attribute) != factory_data->structural_data_view_attribute_names.end();
}

const SmallUnorderedSet<String>& Factory::GetStructuralDataViewAttributeNames()
{
	return factory_data->structural_data_view_attribute_names;
}

} // namespace Rml
