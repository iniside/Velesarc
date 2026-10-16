/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Widgets/SArcModifierRowHandle.h"
#include "Widgets/SArcModifierTreeView.h"


#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SArcModifierRowHandle"

namespace ArcModifierRowHandleImpl
{

} // namespace ArcModifierRowHandleImpl

// ---------------------------------------------------------------------------
// FArcModifierRowDragDropOp
// ---------------------------------------------------------------------------

TSharedRef<FArcModifierRowDragDropOp> FArcModifierRowDragDropOp::New(
	TWeakPtr<SArcModifierTreeView> InTreeView,
	int32 InSourceIndex)
{
	TSharedRef<FArcModifierRowDragDropOp> Op = MakeShareable(new FArcModifierRowDragDropOp());
	Op->DefaultHoverText = LOCTEXT("DragRow", "Modifier Row");
	Op->CurrentHoverText = Op->DefaultHoverText;
	Op->TreeViewWeak = InTreeView;
	Op->SourceIndex = InSourceIndex;
	Op->Construct();
	return Op;
}

// ---------------------------------------------------------------------------
// SArcModifierRowHandle
// ---------------------------------------------------------------------------

void SArcModifierRowHandle::Construct(const FArguments& InArgs)
{
	TreeViewWeak = InArgs._TreeView;
	RowIndex = InArgs._RowIndex;

	ChildSlot
	[
		SNew(SBox)
		.Padding(0.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.WidthOverride(16.0f)
		[
			SNew(SImage)
			.Image(FCoreStyle::Get().GetBrush("VerticalBoxDragIndicatorShort"))
		]
	];
}

FReply SArcModifierRowHandle::OnMouseButtonDown(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& !MouseEvent.IsControlDown()
		&& !MouseEvent.IsShiftDown())
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}
	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SArcModifierRowHandle::OnDragDetected(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	TSharedRef<FArcModifierRowDragDropOp> DragOp = FArcModifierRowDragDropOp::New(TreeViewWeak, RowIndex);
	return FReply::Handled().BeginDragDrop(DragOp);
}

#undef LOCTEXT_NAMESPACE
