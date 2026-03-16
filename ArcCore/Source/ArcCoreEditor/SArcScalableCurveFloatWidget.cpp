// Copyright Lukasz Baran. All Rights Reserved.

#include "SArcScalableCurveFloatWidget.h"

#include "Items/ArcItemScalableFloat.h"
#include "ArcScalableFloat.h"
#include "Curves/CurveFloat.h"

#include "SlateOptMacros.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Views/STreeView.h"
#include "Misc/TextFilter.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "ArcScalableCurveFloat"

DECLARE_DELEGATE_ThreeParams(FOnScalableCurveFloatPicked, FProperty*, UScriptStruct*, EArcScalableType);

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

struct FArcScalableCurveFloatNode
{
	// Group node (owner struct)
	FString DisplayName;
	UScriptStruct* OwnerStruct = nullptr;

	// Leaf node (property)
	FProperty* Property = nullptr;
	EArcScalableType Type = EArcScalableType::None;

	// Tree structure
	TArray<TSharedPtr<FArcScalableCurveFloatNode>> Children;
	TWeakPtr<FArcScalableCurveFloatNode> Parent;

	bool IsGroup() const { return Property == nullptr && OwnerStruct != nullptr; }
	bool IsLeaf() const { return Property != nullptr; }
};

class SArcScalableCurveFloatTreeItem : public STableRow<TSharedPtr<FArcScalableCurveFloatNode>>
{
public:
	SLATE_BEGIN_ARGS(SArcScalableCurveFloatTreeItem)
		: _HighlightText()
	{}
		SLATE_ARGUMENT(FText, HighlightText)
		SLATE_ARGUMENT(TSharedPtr<FArcScalableCurveFloatNode>, Node)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		Node = InArgs._Node;

		const bool bIsGroup = Node->IsGroup();
		const FSlateFontInfo Font = bIsGroup
			? FCoreStyle::GetDefaultFontStyle("Bold", 9)
			: FCoreStyle::GetDefaultFontStyle("Regular", 9);

		STableRow<TSharedPtr<FArcScalableCurveFloatNode>>::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(!bIsGroup),
			InOwnerTableView
		);

		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
				.IndentAmount(16)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(bIsGroup ? FMargin(0.0f, 3.0f, 6.0f, 3.0f) : FMargin(0.0f, 2.0f, 6.0f, 2.0f))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Node->DisplayName))
				.HighlightText(InArgs._HighlightText)
				.Font(Font)
			]
		];
	}

private:
	TSharedPtr<FArcScalableCurveFloatNode> Node;
};

class SArcScalableCurveFloatTreeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcScalableCurveFloatTreeWidget) {}
		SLATE_ARGUMENT(FOnScalableCurveFloatPicked, OnPropertyPickedDelegate)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnPropertyPicked = InArgs._OnPropertyPickedDelegate;

		RebuildNodes(FText::GetEmpty());

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(SearchBoxPtr, SSearchBox)
				.HintText(LOCTEXT("SearchHint", "Search Properties"))
				.OnTextChanged(this, &SArcScalableCurveFloatTreeWidget::OnFilterTextChanged)
				.DelayChangeNotificationsWhileTyping(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Visibility(EVisibility::Collapsed)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(TreeView, STreeView<TSharedPtr<FArcScalableCurveFloatNode>>)
				.SelectionMode(ESelectionMode::Single)
				.TreeItemsSource(&RootNodes)
				.OnGenerateRow(this, &SArcScalableCurveFloatTreeWidget::OnGenerateRow)
				.OnGetChildren(this, &SArcScalableCurveFloatTreeWidget::OnGetChildren)
				.OnSelectionChanged(this, &SArcScalableCurveFloatTreeWidget::OnSelectionChanged)
			]
		];

		// Expand all groups by default
		for (const auto& Root : RootNodes)
		{
			TreeView->SetItemExpansion(Root, true);
		}
	}

private:
	void RebuildNodes(const FText& FilterText)
	{
		RootNodes.Empty();

		const FString FilterString = FilterText.ToString();

		// Add "None" entry at root level
		TSharedPtr<FArcScalableCurveFloatNode> NoneNode = MakeShared<FArcScalableCurveFloatNode>();
		NoneNode->DisplayName = TEXT("None");
		NoneNode->Property = nullptr;
		NoneNode->OwnerStruct = nullptr;
		NoneNode->Type = EArcScalableType::None;
		RootNodes.Add(NoneNode);

		// Iterate all UScriptStruct types derived from FArcScalableFloatItemFragment
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Struct = *It;
			if (!Struct->IsChildOf(FArcScalableFloatItemFragment::StaticStruct()))
			{
				continue;
			}

			// Skip the base struct itself
			if (Struct == FArcScalableFloatItemFragment::StaticStruct())
			{
				continue;
			}

			TSharedPtr<FArcScalableCurveFloatNode> GroupNode;

			// Iterate properties of this struct
			for (TFieldIterator<FProperty> PropIt(Struct, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;

				EArcScalableType PropType = EArcScalableType::None;

				if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
				{
					if (StructProp->Struct && StructProp->Struct->IsChildOf(FArcScalableFloat::StaticStruct()))
					{
						PropType = EArcScalableType::Scalable;
					}
				}
				else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
				{
					if (ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(UCurveFloat::StaticClass()))
					{
						PropType = EArcScalableType::Curve;
					}
				}

				if (PropType == EArcScalableType::None)
				{
					continue;
				}

				// Apply filter
				if (!FilterString.IsEmpty())
				{
					const FString FullName = Struct->GetName() + TEXT(".") + Property->GetName();
					if (!FullName.Contains(FilterString))
					{
						continue;
					}
				}

				// Lazy-create group node
				if (!GroupNode.IsValid())
				{
					GroupNode = MakeShared<FArcScalableCurveFloatNode>();
					// Use DisplayName meta if available, otherwise struct name
					FString StructDisplayName = Struct->GetDisplayNameText().ToString();
					if (StructDisplayName.IsEmpty())
					{
						StructDisplayName = Struct->GetName();
					}
					GroupNode->DisplayName = StructDisplayName;
					GroupNode->OwnerStruct = Struct;
					RootNodes.Add(GroupNode);
				}

				// Create leaf node
				TSharedPtr<FArcScalableCurveFloatNode> LeafNode = MakeShared<FArcScalableCurveFloatNode>();
				LeafNode->DisplayName = Property->GetName();
				LeafNode->Property = Property;
				LeafNode->OwnerStruct = Struct;
				LeafNode->Type = PropType;
				LeafNode->Parent = GroupNode;
				GroupNode->Children.Add(LeafNode);
			}
		}
	}

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArcScalableCurveFloatNode> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SArcScalableCurveFloatTreeItem, OwnerTable)
			.HighlightText(SearchBoxPtr.IsValid() ? SearchBoxPtr->GetText() : FText::GetEmpty())
			.Node(Item);
	}

	void OnGetChildren(TSharedPtr<FArcScalableCurveFloatNode> Item, TArray<TSharedPtr<FArcScalableCurveFloatNode>>& OutChildren)
	{
		if (Item.IsValid())
		{
			OutChildren = Item->Children;
		}
	}

	void OnSelectionChanged(TSharedPtr<FArcScalableCurveFloatNode> Item, ESelectInfo::Type SelectInfo)
	{
		if (!Item.IsValid())
		{
			return;
		}

		// If a group node is clicked, don't fire the delegate - just toggle expansion
		if (Item->IsGroup())
		{
			const bool bIsExpanded = TreeView->IsItemExpanded(Item);
			TreeView->SetItemExpansion(Item, !bIsExpanded);
			TreeView->ClearSelection();
			return;
		}

		// Leaf node or "None"
		OnPropertyPicked.ExecuteIfBound(Item->Property, Item->OwnerStruct, Item->Type);
	}

	void OnFilterTextChanged(const FText& InFilterText)
	{
		RebuildNodes(InFilterText);

		if (TreeView.IsValid())
		{
			TreeView->RequestTreeRefresh();

			// Expand all when filtering
			for (const auto& Root : RootNodes)
			{
				TreeView->SetItemExpansion(Root, true);
			}
		}
	}

	FOnScalableCurveFloatPicked OnPropertyPicked;
	TSharedPtr<SSearchBox> SearchBoxPtr;
	TSharedPtr<STreeView<TSharedPtr<FArcScalableCurveFloatNode>>> TreeView;
	TArray<TSharedPtr<FArcScalableCurveFloatNode>> RootNodes;
};

// ---- SArcScalableCurveFloatWidget ----

void SArcScalableCurveFloatWidget::Construct(const FArguments& InArgs)
{
	OnSelectionChanged = InArgs._OnSelectionChanged;
	SelectedProperty = InArgs._DefaultProperty;
	SelectedOwnerStruct = InArgs._DefaultOwnerStruct;

	ChildSlot
	[
		SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent(this, &SArcScalableCurveFloatWidget::GeneratePickerContent)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ToolTipText(this, &SArcScalableCurveFloatWidget::GetSelectedValueAsString)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SArcScalableCurveFloatWidget::GetSelectedValueAsString)
		]
	];
}

TSharedRef<SWidget> SArcScalableCurveFloatWidget::GeneratePickerContent()
{
	FOnScalableCurveFloatPicked OnPicked(
		FOnScalableCurveFloatPicked::CreateRaw(this, &SArcScalableCurveFloatWidget::OnPropertyPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				SNew(SArcScalableCurveFloatTreeWidget)
				.OnPropertyPickedDelegate(OnPicked)
			]
		];
}

void SArcScalableCurveFloatWidget::OnPropertyPicked(FProperty* InProperty, UScriptStruct* InOwnerStruct, EArcScalableType InType)
{
	SelectedProperty = InProperty;
	SelectedOwnerStruct = InOwnerStruct;

	OnSelectionChanged.ExecuteIfBound(InProperty, InOwnerStruct, InType);

	ComboButton->SetIsOpen(false);
}

FText SArcScalableCurveFloatWidget::GetSelectedValueAsString() const
{
	if (SelectedProperty && SelectedOwnerStruct)
	{
		FString StructDisplayName = SelectedOwnerStruct->GetDisplayNameText().ToString();
		if (StructDisplayName.IsEmpty())
		{
			StructDisplayName = SelectedOwnerStruct->GetName();
		}
		return FText::FromString(FString::Printf(TEXT("%s.%s"), *StructDisplayName, *SelectedProperty->GetName()));
	}
	return FText::FromString(TEXT("None"));
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
