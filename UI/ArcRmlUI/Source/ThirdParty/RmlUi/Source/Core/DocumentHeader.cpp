#include "DocumentHeader.h"
#include "../../Include/RmlUi/Core/Core.h"
#include "../../Include/RmlUi/Core/Log.h"
#include "../../Include/RmlUi/Core/StreamMemory.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "../../Include/RmlUi/Core/StyleSheetContainer.h"
#include "../../Include/RmlUi/Core/SystemInterface.h"
#include "../../Include/RmlUi/Core/URL.h"
#include "StyleSheetFactory.h"
#include "Template.h"
#include "TemplateCache.h"
#include "XMLParseTools.h"

namespace Rml {

void DocumentHeader::MergeHeader(const DocumentHeader& header)
{
	// Copy the title across if ours is empty
	if (title.empty())
		title = header.title;
	// Copy the url across if ours is empty
	if (source.empty())
		source = header.source;

	// Combine external data, keeping relative paths
	MergePaths(template_resources, header.template_resources, header.source);
	MergeResources(rcss, header.rcss);
	MergeResources(scripts, header.scripts);
}

void DocumentHeader::MergePaths(StringList& target, const StringList& source, const String& source_path)
{
	for (size_t i = 0; i < source.size(); i++)
	{
		String joined_path;
		::Rml::GetSystemInterface()->JoinPath(joined_path, StringUtilities::Replace(source_path, '|', ':'),
			StringUtilities::Replace(source[i], '|', ':'));

		target.push_back(StringUtilities::Replace(joined_path, ':', '|'));
	}
}

void DocumentHeader::MergeResources(ResourceList& target, const ResourceList& source)
{
	target.insert(target.end(), source.begin(), source.end());
}

DocumentHeader MergeDocumentHeaderTemplates(const DocumentHeader& document_header)
{
	// Construct a new header and copy the template details across
	DocumentHeader header;
	header.MergePaths(header.template_resources, document_header.template_resources, document_header.source);

	// Merge in any templates, note a merge may cause more templates to merge
	for (size_t i = 0; i < header.template_resources.size(); i++)
	{
		Template* merge_template = TemplateCache::LoadTemplate(URL(header.template_resources[i]).GetURL());

		if (merge_template)
			header.MergeHeader(*merge_template->GetHeader());
		else
			Log::Message(Log::LT_WARNING, "Template %s not found", header.template_resources[i].c_str());
	}

	// Merge the document's header last, as it is the most overriding.
	header.MergeHeader(document_header);
	return header;
}

SharedPtr<StyleSheetContainer> LoadDocumentHeaderStyleSheet(const DocumentHeader& header)
{
	SharedPtr<StyleSheetContainer> new_style_sheet;

	// Combine any inline sheets.
	for (const DocumentHeader::Resource& rcss : header.rcss)
	{
		if (rcss.is_inline)
		{
			SharedPtr<StyleSheetContainer> inline_sheet = MakeShared<StyleSheetContainer>();
			UniquePtr<StreamMemory> stream = MakeUnique<StreamMemory>((const byte*)rcss.content.c_str(), rcss.content.size());
			stream->SetSourceURL(rcss.path);

			if (inline_sheet->LoadStyleSheetContainer(stream.get(), rcss.line))
			{
				if (new_style_sheet)
					new_style_sheet->MergeStyleSheetContainer(*inline_sheet);
				else
					new_style_sheet = std::move(inline_sheet);
			}

			stream.reset();
		}
		else
		{
			const StyleSheetContainer* sub_sheet = StyleSheetFactory::GetStyleSheetContainer(rcss.path);
			if (sub_sheet)
			{
				if (new_style_sheet)
					new_style_sheet->MergeStyleSheetContainer(*sub_sheet);
				else
					new_style_sheet = sub_sheet->CombineStyleSheetContainer(StyleSheetContainer());
			}
			else
				Log::Message(Log::LT_ERROR, "Failed to load style sheet %s.", rcss.path.c_str());
		}
	}

	return new_style_sheet;
}

} // namespace Rml
