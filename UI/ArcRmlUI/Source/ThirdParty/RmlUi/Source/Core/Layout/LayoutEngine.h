#pragma once

#include "../../../Include/RmlUi/Core/Types.h"

namespace Rml {

/**
    See the CSS glossary for terms used in the layout engine:
    https://www.w3.org/TR/css-display-3/#glossary
 */
class LayoutEngine {
public:
	/// Formats the contents for a root-level element, usually a document, or a replaced element with custom formatting.
	/// @param[in] element The element to lay out.
	/// @param[in] containing_block The size of the containing block.
	static void FormatElement(Element* element, Vector2f containing_block);

	/// Formats each out-of-flow layout boundary against its stored containing block and places it from its stored static
	/// position, under one measurement memo. The boundaries must not be nested.
	static void FormatOutOfFlowBoundaries(const Vector<Element*>& boundaries);

#ifdef RMLUI_ARC_PERF
	static void FormatElementReference(Element* element, Vector2f containing_block);
	static bool IsReferencePass();

private:
	static bool reference_pass;
#endif
};

} // namespace Rml
