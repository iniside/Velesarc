#include "StyleSheetFactory.h"
#include "ArcPerfInternal.h"
#include "../../Include/RmlUi/Core/Log.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "../../Include/RmlUi/Core/StyleSheetContainer.h"
#include "../../Include/RmlUi/Core/URL.h"
#include "StyleSheetBinary.h"
#include "StyleSheetNode.h"
#include "StyleSheetParser.h"
#include "StyleSheetSelector.h"

namespace Rml {

static UniquePtr<StyleSheetFactory> instance;

static CompiledStyleSheetReader compiled_style_sheet_reader;

void SetCompiledStyleSheetReader(CompiledStyleSheetReader reader)
{
	compiled_style_sheet_reader = std::move(reader);
}

StyleSheetFactory::StyleSheetFactory() :
	selectors{
		{"nth-child", StructuralSelectorType::Nth_Child},
		{"nth-last-child", StructuralSelectorType::Nth_Last_Child},
		{"nth-of-type", StructuralSelectorType::Nth_Of_Type},
		{"nth-last-of-type", StructuralSelectorType::Nth_Last_Of_Type},
		{"first-child", StructuralSelectorType::First_Child},
		{"last-child", StructuralSelectorType::Last_Child},
		{"first-of-type", StructuralSelectorType::First_Of_Type},
		{"last-of-type", StructuralSelectorType::Last_Of_Type},
		{"only-child", StructuralSelectorType::Only_Child},
		{"only-of-type", StructuralSelectorType::Only_Of_Type},
		{"empty", StructuralSelectorType::Empty},
		{"not", StructuralSelectorType::Not},
		{"scope", StructuralSelectorType::Scope},
	}
{}

StyleSheetFactory::~StyleSheetFactory() {}

bool StyleSheetFactory::Initialise()
{
	RMLUI_ASSERT(instance == nullptr);
	instance = UniquePtr<StyleSheetFactory>(new StyleSheetFactory);
	return true;
}

void StyleSheetFactory::Shutdown()
{
	instance.reset();
	compiled_style_sheet_reader = nullptr;
	StyleSheetBinaryInternal::Shutdown();
}

const StyleSheetContainer* StyleSheetFactory::GetStyleSheetContainer(const String& sheet_name)
{
	RMLUI_ARC_PERF_SCOPE(RcssFactory);
	// Look up the sheet definition in the cache
	auto it = instance->stylesheets.find(sheet_name);
	if (it != instance->stylesheets.end())
	{
		RMLUI_ARC_PERF_COUNT(StyleSheetCacheHits, 1);
		RMLUI_ARC_PERF_CODE(if (ArcPerf::Enabled()) ArcPerf::RecordNamed(ArcPerf::NamedList::StyleSheetCache, "hit " + sheet_name, 0.0, 1);)
		return it->second.get();
	}
	RMLUI_ARC_PERF_CODE(if (ArcPerf::Enabled()) ArcPerf::RecordNamed(ArcPerf::NamedList::StyleSheetCache, "miss " + sheet_name, 0.0, 1);)

	// Don't currently have the sheet, attempt to load it
	UniquePtr<const StyleSheetContainer> sheet = instance->LoadStyleSheetContainer(sheet_name);
	if (!sheet)
		return nullptr;

	const StyleSheetContainer* result = sheet.get();

	// Add it to the cache.
	instance->stylesheets[sheet_name] = std::move(sheet);

	return result;
}

void StyleSheetFactory::ClearStyleSheetCache()
{
	instance->stylesheets.clear();
}

StructuralSelector StyleSheetFactory::GetSelector(const String& name)
{
	SelectorMap::const_iterator it;
	const size_t parameter_start = name.find('(');

	if (parameter_start == String::npos)
		it = instance->selectors.find(name);
	else
		it = instance->selectors.find(name.substr(0, parameter_start));

	if (it == instance->selectors.end())
		return StructuralSelector(StructuralSelectorType::Invalid, 0, 0);

	const StructuralSelectorType selector_type = it->second;

	bool requires_parameter = false;
	switch (selector_type)
	{
	case StructuralSelectorType::Nth_Child:
	case StructuralSelectorType::Nth_Last_Child:
	case StructuralSelectorType::Nth_Of_Type:
	case StructuralSelectorType::Nth_Last_Of_Type:
	case StructuralSelectorType::Not: requires_parameter = true; break;
	default: break;
	}

	const size_t parameter_end = name.rfind(')');
	const bool has_parameter = (parameter_start != String::npos && parameter_end != String::npos && parameter_start < parameter_end);

	if (requires_parameter != has_parameter)
	{
		Log::Message(Log::LT_WARNING, "Invalid selector ':%s' encountered, expected %s parameters", name.c_str(),
			requires_parameter ? "parenthesized" : "no");
		return StructuralSelector(StructuralSelectorType::Invalid, 0, 0);
	}

	// Parse the 'a' and 'b' values.
	int a = 1;
	int b = 0;

	if (has_parameter)
	{
		const String parameters = StringUtilities::StripWhitespace(StringView(name, parameter_start + 1, parameter_end - (parameter_start + 1)));

		if (selector_type == StructuralSelectorType::Not)
		{
			auto list = MakeShared<SelectorTree>();
			list->root = MakeUnique<StyleSheetNode>();
			list->leafs = StyleSheetParser::ConstructNodes(*list->root, parameters);

			int specificity = 0;
			for (const StyleSheetNode* node : list->leafs)
				specificity = Math::Max(specificity, node->GetSpecificity());

			return StructuralSelector(selector_type, std::move(list), specificity);
		}

		// Check for 'even' or 'odd' first.
		if (parameters == "even")
		{
			a = 2;
			b = 0;
		}
		else if (parameters == "odd")
		{
			a = 2;
			b = 1;
		}
		else
		{
			// Alrighty; we've got an equation in the form of [[+/-]an][(+/-)b]. So, foist up, we split on 'n'.
			const size_t n_index = parameters.find('n');
			if (n_index == String::npos)
			{
				// The equation is 0n + b. So a = 0, and we only have to parse b.
				a = 0;
				b = atoi(parameters.c_str());
			}
			else
			{
				if (n_index == 0)
					a = 1;
				else
				{
					const String a_parameter = parameters.substr(0, n_index);
					if (StringUtilities::StripWhitespace(a_parameter) == "-")
						a = -1;
					else
						a = atoi(a_parameter.c_str());
				}

				size_t pm_index = parameters.find('+', n_index + 1);
				if (pm_index != String::npos)
					b = 1;
				else
				{
					pm_index = parameters.find('-', n_index + 1);
					if (pm_index != String::npos)
						b = -1;
				}

				if (n_index == parameters.size() - 1 || pm_index == String::npos)
					b = 0;
				else
					b = b * atoi(parameters.data() + pm_index + 1);
			}
		}
	}

	return StructuralSelector(selector_type, a, b);
}

UniquePtr<const StyleSheetContainer> StyleSheetFactory::LoadStyleSheetContainer(const String& sheet)
{
	RMLUI_ARC_PERF_SCOPE(CompiledRead);
	if (!compiled_style_sheet_reader)
	{
		Log::Message(Log::LT_ERROR, "Cannot load style sheet %s: no compiled style sheet reader is installed.", sheet.c_str());
		return nullptr;
	}

	if (!StringUtilities::EndsWith(StringUtilities::ToLower(sheet), ".rcss"))
	{
		Log::Message(Log::LT_ERROR, "Cannot load style sheet %s: only .rcss style sheets can be linked.", sheet.c_str());
		return nullptr;
	}

	const String compiled_path = sheet + "c";
	Vector<byte> body;
	if (!compiled_style_sheet_reader(compiled_path, body))
	{
		Log::Message(Log::LT_ERROR, "Cannot load style sheet %s: the compiled style sheet %s could not be read.", sheet.c_str(),
			compiled_path.c_str());
		return nullptr;
	}

	const String source_path = StringUtilities::Replace(URL(StringUtilities::Replace(sheet, ':', '|')).GetURL(), '|', ':');
	String error;
	UniquePtr<StyleSheetContainer> new_style_sheet = ReadStyleSheetBinary(Span<const byte>(body), source_path, error);
	if (!new_style_sheet)
		Log::Message(Log::LT_ERROR, "Cannot load style sheet %s: the compiled style sheet %s is unusable: %s", sheet.c_str(), compiled_path.c_str(),
			error.c_str());

	return new_style_sheet;
}

} // namespace Rml
