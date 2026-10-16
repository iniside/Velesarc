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

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STreeView.h"

struct FArcScalableCurveFloatPickerNode
{
	FString DisplayName;
	UScriptStruct* FragmentType = nullptr;
	FStructProperty* StructProperty = nullptr;
	bool bIsLeaf = false;
	TArray<TSharedPtr<FArcScalableCurveFloatPickerNode>> Children;
};

DECLARE_DELEGATE_TwoParams(FOnArcScalableFloatSelected, UScriptStruct* /*OwnerType*/, FStructProperty* /*Prop*/);

class SArcScalableCurveFloatPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcScalableCurveFloatPicker) {}
		SLATE_EVENT(FOnArcScalableFloatSelected, OnSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void CollectPickerNodes();

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArcScalableCurveFloatPickerNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FArcScalableCurveFloatPickerNode> Node, TArray<TSharedPtr<FArcScalableCurveFloatPickerNode>>& OutChildren);
	void OnSelectionChanged(TSharedPtr<FArcScalableCurveFloatPickerNode> SelectedNode, ESelectInfo::Type SelectInfo);
	void OnFilterTextChanged(const FText& InFilterText);

	TArray<TSharedPtr<FArcScalableCurveFloatPickerNode>> RootNodes;
	TArray<TSharedPtr<FArcScalableCurveFloatPickerNode>> FilteredRootNodes;
	TSharedPtr<STreeView<TSharedPtr<FArcScalableCurveFloatPickerNode>>> TreeView;
	FOnArcScalableFloatSelected OnSelected;
	FString FilterString;
};
