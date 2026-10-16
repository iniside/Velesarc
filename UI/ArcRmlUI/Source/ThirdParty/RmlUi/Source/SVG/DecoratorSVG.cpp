#include "DecoratorSVG.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/ElementDocument.h"
#include "../../Include/RmlUi/Core/Geometry.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "SVGCache.h"

namespace Rml {
namespace SVG {

	DecoratorSVG::DecoratorSVG(const String& source, const bool crop_to_content) : source_path(source), crop_to_content(crop_to_content)
	{
		RMLUI_ASSERT(!source_path.empty());
	}

	DecoratorSVG::~DecoratorSVG() {}

	DecoratorDataHandle DecoratorSVG::GenerateElementData(Element* element, BoxArea paint_area) const
	{
		SharedPtr<SVGData> handle = SVGCache::GetHandle(source_path, source_path, SVGCache::File, element, crop_to_content,
			element->GetBox().GetSize(paint_area), String(), false);
		if (!handle)
			return {};

		SharedPtr<SVGData> ready_handle;
		const UnorderedMap<Element*, Data*>::iterator it_previous = generated_data.find(element);
		if (it_previous != generated_data.end() && !handle->HasZeroSize())
		{
			const Data& previous = *it_previous->second;
			ready_handle = previous.handle->IsReady() ? previous.handle : previous.ready_handle;
		}

		Data* data = new Data{
			std::move(handle),
			std::move(ready_handle),
			paint_area,
			element,
		};
		generated_data[element] = data;

		return reinterpret_cast<DecoratorDataHandle>(data);
	}

	void DecoratorSVG::ReleaseElementData(DecoratorDataHandle element_data) const
	{
		Data* data = reinterpret_cast<Data*>(element_data);
		const UnorderedMap<Element*, Data*>::iterator it_data = generated_data.find(data->element);
		if (it_data != generated_data.end() && it_data->second == data)
			generated_data.erase(it_data);
		delete data;
	}

	void DecoratorSVG::RenderElement(Element* element, DecoratorDataHandle element_data) const
	{
		Data* data = reinterpret_cast<Data*>(element_data);
		RMLUI_ASSERT(data && data->handle);
		if (data->handle->IsReady())
			data->ready_handle.reset();
		const SharedPtr<SVGData>& drawn_handle = data->handle->IsReady() ? data->handle : data->ready_handle;
		if (drawn_handle)
			drawn_handle->geometry.Render(element->GetAbsoluteOffset(data->paint_area), drawn_handle->texture);
	}

	DecoratorSVGInstancer::DecoratorSVGInstancer()
	{
		source_id = RegisterProperty("source", "").AddParser("string").GetId();
		crop_id = RegisterProperty("crop", "crop-none").AddParser("keyword", "crop-none, crop-to-content").GetId();
		RegisterShorthand("decorator", "source, crop", ShorthandType::FallThrough);
	}

	DecoratorSVGInstancer::~DecoratorSVGInstancer() {}

	SharedPtr<Decorator> DecoratorSVGInstancer::InstanceDecorator(const String&, const PropertyDictionary& properties,
		const DecoratorInstancerInterface&)
	{
		String source = properties.GetProperty(source_id)->Get<String>();
		if (source.empty())
			return nullptr;

		const bool crop_to_content = properties.GetProperty(crop_id)->Get<int>() != 0;

		return MakeShared<DecoratorSVG>(source, crop_to_content);
	}

} // namespace SVG
} // namespace Rml
