#pragma once

#include "../../../Include/RmlUi/Core/Types.h"

namespace Rml {

class Box;
class ContainerBox;
class LayoutBox;

enum class FormattingContextType {
	Block,
	Table,
	Flex,
	Grid,
	None,
};

/*
    An environment in which related boxes are layed out.
*/
class FormattingContext {
public:
	/// Format the element in an independent formatting context, generating a new layout box.
	/// @param[in] parent_container The container box which should act as the new box's parent.
	/// @param[in] element The element to be formatted.
	/// @param[in] override_initial_box Optionally set the initial box dimensions, otherwise one will be generated based on the element's properties.
	/// @param[in] backup_context If a formatting context can not be determined from the element's properties, use this context.
	/// @return A new, fully formatted layout box, or nullptr if its formatting context could not be determined, or if formatting was unsuccessful.
	static UniquePtr<LayoutBox> FormatIndependent(ContainerBox* parent_container, Element* element, const Box* override_initial_box,
		FormattingContextType backup_context);

	static UniquePtr<LayoutBox> FormatShrinkToFitProbe(ContainerBox* root, Element* wrapper, const Box* override_initial_box);

protected:
	FormattingContext() = default;
	~FormattingContext() = default;

	// Probe boxes are exact only while the wrapper's block formatting context has no floats, see FormatShrinkToFitProbe.
	static bool UseShrinkToFitProbeBox(ContainerBox* parent_container, Element* element, const Box* override_initial_box);

	static bool UseSizeOnlyFormat(Element* element);

private:
	class ShrinkToFitProbeScope {
	public:
		explicit ShrinkToFitProbeScope(Element* wrapper);
		~ShrinkToFitProbeScope();
		ShrinkToFitProbeScope(const ShrinkToFitProbeScope&) = delete;
		ShrinkToFitProbeScope& operator=(const ShrinkToFitProbeScope&) = delete;

		Element* wrapper;
		bool used_probe_box = false;

	private:
		ShrinkToFitProbeScope* previous;
	};

	static ShrinkToFitProbeScope* active_probe_scope;
};

} // namespace Rml
