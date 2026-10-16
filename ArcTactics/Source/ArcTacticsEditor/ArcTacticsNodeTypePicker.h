// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTacticsEditorClipboard.h"

class FArcTacticsEditorViewModel;
class SWidget;
struct FSlateBrush;

enum class EArcTacticsNodeTypePickerSurface : uint8
{
	View,
	Details
};

namespace ArcTacticsNodeTypePicker
{
	TSharedRef<SWidget> MakeNodeTypePicker(
		TWeakPtr<FArcTacticsEditorViewModel> ViewModel,
		EArcTacticsEditorItemKind Kind,
		const FArcTacticsEditorSelection& Target,
		bool bReplace,
		EArcTacticsNodeTypePickerSurface Surface,
		bool bKeepParentSelected = false);

	const FSlateBrush* GetAddConsiderationIcon();
}
