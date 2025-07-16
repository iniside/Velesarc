/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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

#include "ArcStaticItemHelpers.h"

#include "ArcCoreUtils.h"
#include "IContentBrowserSingleton.h"
#include "IDetailPropertyExtensionHandler.h"
#include "IDetailsViewPrivate.h"
#include "IPropertyTable.h"
#include "ISinglePropertyView.h"
#include "IStructureDetailsView.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"

bool FArcStaticItemHelpers::PickItemSourceTemplate(UArcItemDefinitionTemplate*& OutChosenClass)
{
	IContentBrowserSingleton& CBS = IContentBrowserSingleton::Get();
	FAssetPickerConfig Config;
	Config.Filter.ClassPaths.Add(UArcItemDefinitionTemplate::StaticClass()->GetClassPathName());
	Config.Filter.bRecursiveClasses = false;
	Config.Filter.bRecursivePaths = false;
	Config.SelectionMode = ESelectionMode::Single;
	Config.InitialAssetViewType = EAssetViewType::Column;
	Config.bAllowNullSelection = true;
	Config.bShowBottomToolbar = true;
	Config.bAddFilterUI = true;

	bool bPressedOk = false;
	
	FAssetData OutAssetData;

	Config.OnAssetSelected.BindLambda([&OutAssetData](const FAssetData& AssetData)
	{
		OutAssetData = AssetData;
	});
	
	Config.OnAssetDoubleClicked.BindLambda([&OutAssetData](const FAssetData& AssetData)
	{
		OutAssetData = AssetData;
	});

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
			.Title(FText::FromString("Pick Item Definition Template"))
			.SizingRule(ESizingRule::UserSized)
			.ClientSize(FVector2D(400.f, 500.f))
			.MinHeight(400.f)
			.MinWidth(500)
			.SupportsMaximize(false)
			.SupportsMinimize(false);
	
	auto OnClickedOk = [&bPressedOk, PickerWindow]()-> FReply
	{
		bPressedOk = true;
		PickerWindow->RequestDestroyWindow();

		return FReply::Handled();
	};
	auto OnClickedCancel = [&bPressedOk, PickerWindow]()-> FReply
	{
		bPressedOk = false;
		PickerWindow->RequestDestroyWindow();

		return FReply::Handled();
	};
	
	TSharedPtr<SWidget> AssetPicker = CBS.CreateAssetPicker(Config);
	TSharedPtr<SWidget> AssetPickerContainer =
		SNew (SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.MinDesiredHeight(400.f)
			[
				AssetPicker.ToSharedRef()
			]
		]
		+SVerticalBox::Slot()
		.HAlign(HAlign_Right)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateLambda(OnClickedOk))
				[
					SNew(STextBlock).Text(FText::FromString("Ok"))
				]
			]
			+SHorizontalBox::Slot()
			.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
			
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateLambda(OnClickedCancel))
				[
					SNew(STextBlock).Text(FText::FromString("Cancel"))
				]
			]
		];
	
	PickerWindow->SetContent(AssetPickerContainer.ToSharedRef());

	GEditor->EditorAddModalWindow(PickerWindow);

	if (OutAssetData.IsValid())
	{
		OutChosenClass = Cast<UArcItemDefinitionTemplate>(OutAssetData.GetAsset());	
	}
	else
	{
		// Make sure it is nullptr.
		OutChosenClass = nullptr;
	}	

	return bPressedOk;
}

class SArcFragmentMergeRow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SArcFragmentMergeRow ){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs);
	
};

void SArcFragmentMergeRow::Construct(const FArguments& InArgs)
{
}

struct FArcItemDefinitionEntry
{
	FText ItemDefinitionName;
	TWeakObjectPtr<const UArcItemDefinition> ItemDef;
	TWeakObjectPtr<UArcItemDefinitionTemplate> TemplateDef;
};

class SArcFragmentMergePreview : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SArcFragmentMergePreview ){}
		SLATE_ATTRIBUTE(EArcMergeMode, MergeMode)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args
		, const FGeometry& AllottedGeometry
		, const FSlateRect& MyCullingRect
		, FSlateWindowElementList& OutDrawElements
		, int32 LayerId
		, const FWidgetStyle& InWidgetStyle
		, bool bParentEnabled) const override;
	
	virtual ~SArcFragmentMergePreview() override;

	TArray<TSharedPtr<FArcItemDefinitionEntry>> ComboBoxSourceItems;
	TSharedPtr<SComboBox<TSharedPtr<FArcItemDefinitionEntry>>> ItemDefComboBox;
	
	TSharedPtr<FArcItemDefinitionEntry> ComboBoxSelectedItem;
	
	TWeakObjectPtr<UArcItemDefinition> PreviewObject = nullptr;
	
	EArcMergeMode MergeMode;

	TSharedPtr<class IDetailsView> TemplateDetailsViewPtr;
	TSharedPtr<class IDetailsView> OriginalDetailsViewPtr;

	TArray<TSharedRef<FDetailTreeNode>> OriginalMissingNodes;
	TArray<TSharedRef<FDetailTreeNode>> TemplateAddedNodes;
	
	TSharedPtr<SOverlay> OldItemFragmentsView;
	TSharedPtr<SOverlay> TemplateFragmentList;
	TSharedPtr<SOverlay> MergedFragmentList;
	
	TWeakObjectPtr<UArcItemDefinitionTemplate> Template;
	
	void SetObjects(TArray<UArcItemDefinition*> SelectedObjects, UArcItemDefinitionTemplate* InTemplate);
	void HandleOnItemDefSelected(TSharedPtr<FArcItemDefinitionEntry> InItem, ESelectInfo::Type);
};

void SArcFragmentMergePreview::Construct(const FArguments& InArgs)
{
	MergeMode = InArgs._MergeMode.Get();
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ItemDefComboBox, SComboBox<TSharedPtr<FArcItemDefinitionEntry>>)
			.OptionsSource(&ComboBoxSourceItems)
			.OnGenerateWidget_Lambda([this](TSharedPtr<FArcItemDefinitionEntry> InItem)
			{
				return SNew(STextBlock).Text(InItem->ItemDefinitionName);
			})
			.OnSelectionChanged(this, &SArcFragmentMergePreview::HandleOnItemDefSelected)
			[
				SNew(STextBlock).Text_Lambda([this]()
				{
					if (ComboBoxSelectedItem.IsValid())
					{
						return ComboBoxSelectedItem->ItemDefinitionName;
					}
					return FText::FromString("Select Item Definition");
				})
			]
		]
		+SVerticalBox::Slot()
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+SSplitter::Slot()
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)
				+SSplitter::Slot()
				[
					SAssignNew(OldItemFragmentsView, SOverlay)
				]
				+SSplitter::Slot()
				[
					SAssignNew(TemplateFragmentList, SOverlay)
				]
			]
			+SSplitter::Slot()
			[
				SAssignNew(MergedFragmentList, SOverlay)
			]
		]
	];
}

namespace Arcx::Private
{
	void GetChildren(TSharedRef<FDetailTreeNode> Node, TArray<TSharedRef<FDetailTreeNode>>& OutChildren)
	{
		TArray<TSharedRef<FDetailTreeNode>> NewChildren;
		Node->GetChildren(NewChildren);
		for (TSharedRef<FDetailTreeNode> ChildNode : NewChildren)
		{
			GetChildren(ChildNode, OutChildren);
		}
		OutChildren.Append(NewChildren);
	};

	bool IsNodeStruct(UScriptStruct* StructType, const FPropertyNode* PropertyNode)
	{
		while (PropertyNode)
		{
			if (const FComplexPropertyNode* ComplexNode = PropertyNode->AsComplexNode())
			{
				const UStruct* S = ComplexNode->GetBaseStructure();
				if (S->IsChildOf(StructType))
				{
					return true;
				}
				return false;
			}
			else if (const FProperty* CurrentProperty = PropertyNode->GetProperty())
			{
				FFieldClass* PropertyClass = CurrentProperty->GetClass();
				if (PropertyClass == FStructProperty::StaticClass())
				{
					const UStruct* S = static_cast<const FStructProperty*>(CurrentProperty)->Struct;
					if (S->IsChildOf(StructType))
					{
						return true;
					}
					return false;
				}
			}
			PropertyNode = PropertyNode->GetParentNode();
		}

		return false;
	}
	
	const UStruct* GetStruct(const FPropertyNode* PropertyNode)
	{
		while (PropertyNode)
		{
			if (const FComplexPropertyNode* ComplexNode = PropertyNode->AsComplexNode())
			{
				return ComplexNode->GetBaseStructure();
			}
			else if (const FProperty* CurrentProperty = PropertyNode->GetProperty())
			{
				FFieldClass* PropertyClass = CurrentProperty->GetClass();
				if (PropertyClass == FStructProperty::StaticClass())
				{

					return static_cast<const FStructProperty*>(CurrentProperty)->Struct;
				}
				// Object Properties seem to always show up as ObjectPropertyNodes, which are handled above
				//else if (PropertyClass == FObjectProperty::StaticClass())
				//{
				//}
			}
			PropertyNode = PropertyNode->GetParentNode();
		}
		return nullptr;
	}
	
	TArray<TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>> FindFragments(const TArray<TSharedRef<FDetailTreeNode>>& InNodes)
	{
		TArray<TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>> OriginalItemFragments;
		
		for (TSharedRef<FDetailTreeNode> Node : InNodes)
		{
			TSharedPtr<IPropertyHandle> NodeHandle = Node->CreatePropertyHandle();
			if (NodeHandle.IsValid())
			{
				TSharedPtr<IPropertyHandleStruct> StructHandle = NodeHandle->AsStruct();
				if (StructHandle.IsValid())
				{
					const UStruct* OS = StructHandle->GetStructData()->GetStruct();
					if (OS && OS->IsChildOf(FArcItemFragment::StaticStruct()))
					{
						bool bContains = OriginalItemFragments.ContainsByPredicate([OS](const TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>& Item)
						{
							return OS == Item.Value;	
						});
						if (bContains == false)
						{
							OriginalItemFragments.Add({Node, OS});
							continue;
						}
					}
				}
				else
				{
					TSharedPtr<IPropertyHandle> TryHandle = NodeHandle->GetChildHandle("InstancedStruct")->GetChildHandle(0);
					if (TryHandle.IsValid())
					{
						TSharedPtr<IPropertyHandleStruct> StructHandle2 = TryHandle->AsStruct();
						if (StructHandle2.IsValid())
						{
							const UStruct* OS = StructHandle2->GetStructData()->GetStruct();
							if (OS && OS->IsChildOf(FArcItemFragment::StaticStruct()))
							{
								bool bContains = OriginalItemFragments.ContainsByPredicate([OS](const TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>& Item)
								{
									return OS == Item.Value;	
								});
								if (bContains == false)
								{
									OriginalItemFragments.Add({Node, OS});
									continue;
								}
							}
						}
					}
				}
				TSharedPtr<IPropertyHandle> NodeParent = NodeHandle->GetParentHandle();
				if (NodeParent.IsValid())
				{
					TSharedPtr<IPropertyHandleStruct> ParentStructHandle = NodeParent->AsStruct();
					if (ParentStructHandle.IsValid())
					{
						const UStruct* OS = ParentStructHandle->GetStructData()->GetStruct();
						if (OS && OS->IsChildOf(FArcItemFragment::StaticStruct()))
						{
							bool bContains = OriginalItemFragments.ContainsByPredicate([OS](const TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>& Item)
							{
								return OS == Item.Value;	
							});
							if (bContains == false)
							{
								OriginalItemFragments.Add({Node, OS});
								continue;
							}
						}
					}
				}
			}
		}
		
		return OriginalItemFragments;
	}
	
	TArray<TSharedRef<FDetailTreeNode>> GetStructNodes(TSharedRef<IDetailsView> DetailsView)
	{
		TArray<TSharedRef<FDetailTreeNode>> OutNodes;
		TSharedPtr<class IDetailsViewPrivate> SelectedPrivateView = StaticCastSharedPtr<IDetailsViewPrivate>(DetailsView.ToSharedPtr());
		
		TArray<TWeakPtr<FDetailTreeNode>> Result;
		if (SelectedPrivateView.IsValid())
		{
			SelectedPrivateView->GetHeadNodes(Result);
		}
		TArray<TSharedRef<FDetailTreeNode>> OutChildren;
	
		for (TWeakPtr<FDetailTreeNode> Node : Result)
		{
			Arcx::Private::GetChildren(Node.Pin().ToSharedRef(), OutChildren);	
		}
		for (TSharedRef<FDetailTreeNode> Node : OutChildren)
		{
			const bool bIs = Arcx::Private::IsNodeStruct(FArcInstancedStruct::StaticStruct(), Node->GetPropertyNode().Get());
			if (bIs)
			{
				OutNodes.Add(Node);
			}
		}

		return OutNodes;
	}
}

int32 SArcFragmentMergePreview::OnPaint(const FPaintArgs& Args
	, const FGeometry& AllottedGeometry
	, const FSlateRect& MyCullingRect
	, FSlateWindowElementList& OutDrawElements
	, int32 LayerId
	, const FWidgetStyle& InWidgetStyle
	, bool bParentEnabled) const
{
	
	int32 ReturnLayer = SCompoundWidget::OnPaint(Args
		, AllottedGeometry
		, MyCullingRect
		, OutDrawElements
		, LayerId
		, InWidgetStyle
		, bParentEnabled);
	
	for (TSharedRef<FDetailTreeNode> Node : TemplateAddedNodes)
	{
		FSlateRect Rec = TemplateDetailsViewPtr->GetPaintSpacePropertyBounds(Node, true);
		if (Rec.IsValid() == false)
		{
			continue;
		}
		FVector2D TopLeft = Rec.GetTopLeft();
		FVector2D BottomLeft = Rec.GetBottomRight();
		FPaintGeometry Geometry(
					TopLeft + FVector2D{0.f,2.f},
					Rec.GetSize() - FVector2D{0.f,4.f},
					1.f
				);
		
		FSlateDrawElement::MakeBox(OutDrawElements, ReturnLayer+LayerId+10
			, Geometry
			, FAppStyle::GetBrush("WhiteBrush")
			, ESlateDrawEffect::None
			, FLinearColor(0.f, 1.f, 0.f, 0.35f));
	}
	for (TSharedRef<FDetailTreeNode> Node : OriginalMissingNodes)
	{
		FSlateRect Rec = OriginalDetailsViewPtr->GetPaintSpacePropertyBounds(Node, true);
		if (Rec.IsValid() == false)
		{
			continue;
		}
		FVector2D TopLeft = Rec.GetTopLeft();
		FVector2D BottomLeft = Rec.GetBottomRight();
		FPaintGeometry Geometry(
					TopLeft + FVector2D{0.f,2.f},
					Rec.GetSize() - FVector2D{0.f,4.f},
					1.f
				);
		
		FSlateDrawElement::MakeBox(OutDrawElements, ReturnLayer+LayerId+10
			, Geometry
			, FAppStyle::GetBrush("WhiteBrush")
			, ESlateDrawEffect::None
			, FLinearColor(1.f, 0.f, 0.f, 0.35f));
	}
	
	return ReturnLayer;
}

SArcFragmentMergePreview::~SArcFragmentMergePreview()
{
	if (PreviewObject.IsValid())
	{
		PreviewObject->MarkAsGarbage();
	}
}

void SArcFragmentMergePreview::HandleOnItemDefSelected(TSharedPtr<FArcItemDefinitionEntry> InItem, ESelectInfo::Type)
{
	// Garbage old preview object, since we can enter this function multiple times.
	if (PreviewObject.IsValid())
	{
		PreviewObject->MarkAsGarbage();
	}

	UArcItemDefinitionTemplate* LocalTemplate = nullptr;
	if (Template.IsValid())
	{
		LocalTemplate = Template.Get();
	}
	else
	{
		LocalTemplate = InItem->TemplateDef.Get();
	}
	ComboBoxSelectedItem = InItem;
	
	UArcItemDefinition* SelectedObject = const_cast<UArcItemDefinition*>(InItem->ItemDef.Get());
	
	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bShowScrollBar = true;
	TemplateFragmentList->ClearChildren();
	MergedFragmentList->ClearChildren();
	OldItemFragmentsView->ClearChildren();
	
	UArcItemDefinition* DuplicatePreview = DuplicateObject(SelectedObject, nullptr);
	PreviewObject = DuplicatePreview;
	DuplicatePreview->SetFlags(RF_Standalone | RF_Transient);
	switch (MergeMode)
	{
		case EArcMergeMode::Update:
			LocalTemplate->UpdateFromTemplate(DuplicatePreview);
			break;
		case EArcMergeMode::Set:
			LocalTemplate->SetItemTemplate(DuplicatePreview);
			break;
		case EArcMergeMode::SetNewOrReplace:
			LocalTemplate->SetNewOrReplaceItemTemplate(DuplicatePreview);
			break;
		default: ;
	}
	
			
	TSharedRef<class IDetailsView> TemplateDetailsView =  PropertyEditor.CreateDetailView(DetailsViewArgs);
	TemplateDetailsView->SetObject(LocalTemplate);
	
	TemplateFragmentList->AddSlot()
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString("Template"))
		]
		+SVerticalBox::Slot()
		[
			TemplateDetailsView
		]
	];
			
	
	
	TSharedRef<class IDetailsView> MergedDetailsView =  PropertyEditor.CreateDetailView(DetailsViewArgs);
	MergedDetailsView->SetObject(DuplicatePreview, true);

	MergedFragmentList->AddSlot()
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString("Merge Result"))
		]
		+SVerticalBox::Slot()
		[
			MergedDetailsView
		]
	];

	TSharedRef<class IDetailsView> SelectedItemDetailsView =  PropertyEditor.CreateDetailView(DetailsViewArgs);
	SelectedItemDetailsView->SetObject(SelectedObject);
	OldItemFragmentsView->AddSlot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString("Selected Item"))
			]
			+SVerticalBox::Slot()
			[
				SelectedItemDetailsView
			]
		];


	OriginalDetailsViewPtr = SelectedItemDetailsView;
	TemplateDetailsViewPtr = TemplateDetailsView;
	
	TArray<TSharedRef<FDetailTreeNode>> MergedNodes;
	MergedNodes = Arcx::Private::GetStructNodes(MergedDetailsView);

	TArray<TSharedRef<FDetailTreeNode>> TemplateNodes;
	TemplateNodes = Arcx::Private::GetStructNodes(TemplateDetailsView);

	TArray<TSharedRef<FDetailTreeNode>> OriginalNodes;
	OriginalNodes = Arcx::Private::GetStructNodes(SelectedItemDetailsView);
		
	TArray<TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>> OriginalItemFragments = Arcx::Private::FindFragments(OriginalNodes);
	TArray<TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>> TemplateItemFragments = Arcx::Private::FindFragments(TemplateNodes);
	TArray<TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>> MergedItemFragments = Arcx::Private::FindFragments(MergedNodes);
	
	TemplateAddedNodes.Empty();

	for (const TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>& Item : TemplateItemFragments)
	{
		int32 Idx = OriginalItemFragments.IndexOfByPredicate([Item](const TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>& InItem)
		{
			return InItem.Value == Item.Value;
		});

		if (Idx == INDEX_NONE)
		{
			TemplateAddedNodes.Add(Item.Key);
		}
	}

	OriginalMissingNodes.Empty();
	for (const TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>& Item : OriginalItemFragments)
	{
		int32 Idx = MergedItemFragments.IndexOfByPredicate([Item](const TTuple<TSharedRef<FDetailTreeNode>, const UStruct*>& InItem)
		{
			return InItem.Value == Item.Value;
		});

		if (Idx == INDEX_NONE)
		{
			OriginalMissingNodes.Add(Item.Key);
		}
	}
}

void SArcFragmentMergePreview::SetObjects(TArray<UArcItemDefinition*> SelectedObjects, UArcItemDefinitionTemplate* InTemplate)
{
	ComboBoxSourceItems.Empty();

	for (const UArcItemDefinition* ItemDef : SelectedObjects)
	{
		TSharedPtr<FArcItemDefinitionEntry> Item = MakeShareable(new FArcItemDefinitionEntry {
		FText::FromString(GetNameSafe(ItemDef)), ItemDef, ItemDef->GetSourceTemplate()});

		ComboBoxSourceItems.Add(Item);
	}

	Template = InTemplate;
	
	ComboBoxSelectedItem = ComboBoxSourceItems[0];
	HandleOnItemDefSelected(ComboBoxSelectedItem, ESelectInfo::Direct);
	ItemDefComboBox->RefreshOptions();
}

bool FArcStaticItemHelpers::PickItemSourceTemplateWithPreview(UArcItemDefinitionTemplate*& OutChosenClass, TArray<UArcItemDefinition*> InSelectedItemDefinition, EArcMergeMode MergeMode)
{
	IContentBrowserSingleton& CBS = IContentBrowserSingleton::Get();
	FAssetPickerConfig Config;
	Config.Filter.ClassPaths.Add(UArcItemDefinitionTemplate::StaticClass()->GetClassPathName());
	Config.Filter.bRecursiveClasses = false;
	Config.Filter.bRecursivePaths = false;
	Config.SelectionMode = ESelectionMode::Single;
	Config.InitialAssetViewType = EAssetViewType::Column;
	Config.bAllowNullSelection = true;
	Config.bShowBottomToolbar = false;
	Config.bShowPathInColumnView = false;
	Config.bShowTypeInColumnView = false;
	
	Config.bAddFilterUI = true;
	Config.HiddenColumnNames.Append( {"Type", "Class", "ItemDiskSize", "HasVirtualizedData", "NativeClass", "SourceTemplate", "ItemClass"});
	bool bPressedOk = false;

	FLinearColor UnChanged = FLinearColor::White;
	FLinearColor Added = FLinearColor::Green;
	FLinearColor Removed = FLinearColor::Red;

	FAssetData OutAssetData;
	TSharedPtr<SArcFragmentMergePreview> MergePreview = SNew(SArcFragmentMergePreview).MergeMode(MergeMode);
	
	Config.OnAssetSelected.BindLambda([MergePreview, &OutAssetData, InSelectedItemDefinition](const FAssetData& AssetData)
	{
		if (AssetData != OutAssetData && AssetData.IsValid())
		{
			UArcItemDefinitionTemplate* Template = Cast<UArcItemDefinitionTemplate>(AssetData.GetAsset());
			MergePreview->SetObjects(InSelectedItemDefinition, Template);
		}
		
		OutAssetData = AssetData;
	});
	
	Config.OnAssetDoubleClicked.BindLambda([MergePreview, &OutAssetData, InSelectedItemDefinition](const FAssetData& AssetData)
	{
		if (AssetData != OutAssetData && AssetData.IsValid())
		{
			UArcItemDefinitionTemplate* Template = Cast<UArcItemDefinitionTemplate>(AssetData.GetAsset());
			MergePreview->SetObjects(InSelectedItemDefinition, Template);
		}
		OutAssetData = AssetData;
	});

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
			.Title(FText::FromString("Pick Item Definition Template"))
			.SizingRule(ESizingRule::UserSized)
			.ClientSize(FVector2D(400.f, 500.f))
			.MinHeight(400.f)
			.MinWidth(500)
			.SupportsMaximize(false)
			.SupportsMinimize(false);
	
	auto OnClickedOk = [&bPressedOk, PickerWindow]()-> FReply
	{
		bPressedOk = true;
		PickerWindow->RequestDestroyWindow();

		return FReply::Handled();
	};
	auto OnClickedCancel = [&bPressedOk, PickerWindow]()-> FReply
	{
		bPressedOk = false;
		PickerWindow->RequestDestroyWindow();

		return FReply::Handled();
	};
	
	TSharedPtr<SWidget> AssetPicker = CBS.CreateAssetPicker(Config);
	TSharedPtr<SWidget> AssetPickerContainer =
		SNew(SSplitter)
		+SSplitter::Slot()
		.SizeRule(SSplitter::FractionOfParent)
		.Value(0.65)
		[
			MergePreview.ToSharedRef()
		]
		+SSplitter::Slot()
		.SizeRule(SSplitter::FractionOfParent)
		.Value(0.35)
		[
			SNew (SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Fill)
			[
				SNew(SBox)
				.MinDesiredHeight(400.f)
				[
					AssetPicker.ToSharedRef()
				]
			]
			+SVerticalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateLambda(OnClickedOk))
					[
						SNew(STextBlock).Text(FText::FromString("Ok"))
					]
				]
				+SHorizontalBox::Slot()
				.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateLambda(OnClickedCancel))
					[
						SNew(STextBlock).Text(FText::FromString("Cancel"))
					]
				]
			]
		];
	
	PickerWindow->SetContent(AssetPickerContainer.ToSharedRef());

	GEditor->EditorAddModalWindow(PickerWindow);

	if (OutAssetData.IsValid())
	{
		OutChosenClass = Cast<UArcItemDefinitionTemplate>(OutAssetData.GetAsset());	
	}
	else
	{
		// Make sure it is nullptr.
		OutChosenClass = nullptr;
	}	

	return bPressedOk;
}

bool FArcStaticItemHelpers::PreviewUpdate(UArcItemDefinitionTemplate* InUpdateTemplate
	, TArray<UArcItemDefinition*> InSelectedItemDefinition)
{
	TSharedPtr<SArcFragmentMergePreview> MergePreview = SNew(SArcFragmentMergePreview).MergeMode(EArcMergeMode::Update);
	MergePreview->SetObjects(InSelectedItemDefinition, InUpdateTemplate);
	
	bool bPressedOk = false;
	
	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
			.Title(FText::FromString("Pick Item Definition Template"))
			.SizingRule(ESizingRule::UserSized)
			.ClientSize(FVector2D(400.f, 500.f))
			.MinHeight(400.f)
			.MinWidth(500)
			.SupportsMaximize(false)
			.SupportsMinimize(false);
	
	auto OnClickedOk = [&bPressedOk, PickerWindow]()-> FReply
	{
		bPressedOk = true;
		PickerWindow->RequestDestroyWindow();

		return FReply::Handled();
	};
	auto OnClickedCancel = [&bPressedOk, PickerWindow]()-> FReply
	{
		bPressedOk = false;
		PickerWindow->RequestDestroyWindow();

		return FReply::Handled();
	};
	
	TSharedPtr<SWidget> AssetPickerContainer =
		SNew(SSplitter)
		+SSplitter::Slot()
		.SizeRule(SSplitter::FractionOfParent)
		.Value(0.65)
		[
			MergePreview.ToSharedRef()
		]
		+SSplitter::Slot()
		.SizeRule(SSplitter::FractionOfParent)
		.Value(0.35)
		[
			SNew (SVerticalBox)
			+SVerticalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateLambda(OnClickedOk))
					[
						SNew(STextBlock).Text(FText::FromString("Ok"))
					]
				]
				+SHorizontalBox::Slot()
				.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateLambda(OnClickedCancel))
					[
						SNew(STextBlock).Text(FText::FromString("Cancel"))
					]
				]
			]
		];
	
	PickerWindow->SetContent(AssetPickerContainer.ToSharedRef());

	GEditor->EditorAddModalWindow(PickerWindow);

	return bPressedOk;
}
