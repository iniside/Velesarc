#pragma once

#include "../../Include/RmlUi/Core/DecorationTypes.h"
#include "../../Include/RmlUi/Core/Filter.h"
#include "../../Include/RmlUi/Core/ID.h"
#include "../../Include/RmlUi/Core/NumericValue.h"

namespace Rml {

class FilterDropShadow : public Filter {
public:
	bool Initialise(const BoxShadow& shadow);

	CompiledFilter CompileFilter(Element* element) const override;

	void ExtendInkOverflow(Element* element, Rectanglef& scissor_region) const override;

private:
	BoxShadow shadow;
};

class FilterDropShadowInstancer : public FilterInstancer {
public:
	FilterDropShadowInstancer();

	SharedPtr<Filter> InstanceFilter(const String& name, const PropertyDictionary& properties) override;

private:
	struct PropertyIds {
		PropertyId shadow;
	};
	PropertyIds ids;
};

} // namespace Rml
