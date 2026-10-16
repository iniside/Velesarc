// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcTacticsNodeDetails.h"

#include "ArcTacticsEditorStyle.h"
#include "ArcTacticsEditorViewModel.h"
#include "ArcTacticsModule.h"
#include "ArcTacticsNodePresentation.h"
#include "ArcTacticsNodeTypePicker.h"
#include "Decision/ArcTacticsConfig.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "InstancedStructDetails.h"
#include "PropertyBindingExtension.h"
#include "PropertyBagDetails.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcTacticsNodeDetails"

namespace ArcTacticsNodeDetailsPrivate
{
	FGuid GetNodeID(const TSharedRef<IPropertyHandle>& StructPropertyHandle)
	{
		FGuid ID;
		StructPropertyHandle->EnumerateConstRawData(
			[&ID](const void* RawData, const int32 DataIndex, const int32 NumDatas)
			{
				if (RawData)
				{
					ID = static_cast<const FArcTacticsNodeDefinition*>(RawData)->ID;
					return false;
				}
				return true;
			});
		return ID;
	}

	FArcTacticsEditorSelection ResolveSelection(const UArcTacticsConfig& Config, const FGuid& ID)
	{
		for (const FArcTacticsNodeDefinition& Task : Config.GlobalTasks)
		{
			if (Task.ID == ID)
			{
				return {EArcTacticsEditorItemKind::GlobalTask, ID};
			}
		}
		for (const FArcTacticsActionDefinition& Action : Config.Actions)
		{
			if (Action.Action.ID == ID)
			{
				return {EArcTacticsEditorItemKind::Action, ID};
			}
			if (Action.Steps.ContainsByPredicate(
				[&ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				return {EArcTacticsEditorItemKind::Step, ID};
			}
			if (Action.ActiveSteps.ContainsByPredicate(
				[&ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				return {EArcTacticsEditorItemKind::ActiveStep, ID};
			}
			if (Action.Considerations.ContainsByPredicate(
				[&ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				return {EArcTacticsEditorItemKind::Consideration, ID};
			}
		}
		for (const FArcTacticsSelectorDefinition& Selector : Config.Selectors)
		{
			if (Selector.Selector.ID == ID)
			{
				return {EArcTacticsEditorItemKind::Selector, ID};
			}
			if (Selector.Action.ID == ID)
			{
				return {EArcTacticsEditorItemKind::SelectorAction, ID};
			}
			if (Selector.Steps.ContainsByPredicate(
				[&ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				return {EArcTacticsEditorItemKind::Step, ID};
			}
			if (Selector.ActiveSteps.ContainsByPredicate(
				[&ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				return {EArcTacticsEditorItemKind::ActiveStep, ID};
			}
			if (Selector.Considerations.ContainsByPredicate(
				[&ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				return {EArcTacticsEditorItemKind::Consideration, ID};
			}
		}
		return FArcTacticsEditorSelection::Config();
	}

	class FNodeInstanceDataDetails final : public FInstancedStructDataDetails
	{
	public:
		FNodeInstanceDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, const FGuid& InID)
			: FInstancedStructDataDetails(InStructProperty)
			, ID(InID)
		{
		}

		virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override
		{
			ArcTacticsNodeDetails::StampBindingScope(ChildRow.GetPropertyHandle(), ID);
		}

		virtual void AddChildRows(
			IDetailChildrenBuilder& ChildBuilder,
			const TArray<TSharedPtr<IPropertyHandle>>& ChildProperties) override
		{
			for (const EArcTacticsPropertyRole Role : {
				EArcTacticsPropertyRole::Input,
				EArcTacticsPropertyRole::Parameter,
				EArcTacticsPropertyRole::Output,
				EArcTacticsPropertyRole::Invalid })
			{
				for (const TSharedPtr<IPropertyHandle>& ChildHandle : ChildProperties)
				{
					if (ChildHandle.IsValid() && ResolveRole(*ChildHandle) == Role)
					{
						AddChildRow(ChildBuilder, ChildHandle, nullptr);
					}
				}
			}
		}

	private:
		static EArcTacticsPropertyRole ResolveRole(const IPropertyHandle& Handle)
		{
			const FProperty* Property = Handle.GetProperty();
			return Property
				? ArcTacticsNode::ResolvePropertyRole(*Property)
				: EArcTacticsPropertyRole::Invalid;
		}

		FGuid ID;
	};
	class FParameterBagDetails final : public FPropertyBagInstanceDataDetails
	{
	public:
		FParameterBagDetails(
			TSharedPtr<IPropertyHandle> InBagHandle,
			TSharedPtr<IPropertyUtilities> InPropertyUtilities,
			const FGuid& InID)
			: FPropertyBagInstanceDataDetails(FConstructParams{
				.BagStructProperty = InBagHandle,
				.PropUtils = InPropertyUtilities,
				.bAllowContainers = true,
				.ChildRowFeatures = EPropertyBagChildRowFeatures::Extended })
			, BagHandle(MoveTemp(InBagHandle))
			, PropertyUtilities(MoveTemp(InPropertyUtilities))
			, ID(InID)
		{
		}

		virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override
		{
			NodeRow
			.NameContent()[BagHandle->CreatePropertyNameWidget()]
			.ValueContent()
			.MinDesiredWidth(125.0f)
			.MaxDesiredWidth(600.0f)
			[
				FPropertyBagDetails::MakeAddPropertyWidget(BagHandle, PropertyUtilities).ToSharedRef()
			]
			.ShouldAutoExpand(true);
		}

		virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override
		{
			FPropertyBagInstanceDataDetails::OnChildRowAdded(ChildRow);
			ArcTacticsNodeDetails::StampBindingScope(
				ChildRow.GetPropertyHandle(), ID, EArcTacticsPropertyRole::Parameter);
		}

	private:
		TSharedPtr<IPropertyHandle> BagHandle;
		TSharedPtr<IPropertyUtilities> PropertyUtilities;
		FGuid ID;
	};


	FArcTacticsNodePresentation GetPresentation(
		const TWeakPtr<FArcTacticsEditorViewModel>& ViewModel,
		const FArcTacticsEditorSelection& Item)
	{
		const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
		UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
		const FArcTacticsNodeDefinition* Node = Config ? Config->FindNodeByID(Item.ID) : nullptr;
		return Node && Config
			? FArcTacticsNodePresentation::Build(*Node, *Config)
			: FArcTacticsNodePresentation();
	}
}

TSharedRef<IDetailCustomNodeBuilder> ArcTacticsNodeDetails::MakeParameterBagDetails(
	TSharedPtr<IPropertyHandle> BagHandle,
	TSharedPtr<IPropertyUtilities> PropertyUtilities,
	const FGuid& ID)
{
	return MakeShared<ArcTacticsNodeDetailsPrivate::FParameterBagDetails>(
		MoveTemp(BagHandle), MoveTemp(PropertyUtilities), ID);
}

void ArcTacticsNodeDetails::StampBindingScope(
	const TSharedPtr<IPropertyHandle>& Handle,
	const FGuid& ID,
	const EArcTacticsPropertyRole Role)
{
	if (!Handle.IsValid() || !ID.IsValid())
	{
		return;
	}
	Handle->SetInstanceMetaData(UE::PropertyBinding::MetaDataStructIDName, LexToString(ID));
	if (Role == EArcTacticsPropertyRole::Parameter)
	{
		Handle->SetInstanceMetaData(TEXT("ArcTacticsParameter"), TEXT("true"));
	}
}

void ArcTacticsNodeDetails::StampBindingScopeChildren(
	const TSharedPtr<IPropertyHandle>& Handle,
	const FGuid& ID,
	const EArcTacticsPropertyRole Role)
{
	if (!Handle.IsValid())
	{
		return;
	}
	uint32 NumChildren = 0;
	Handle->GetNumChildren(NumChildren);
	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		StampBindingScope(Handle->GetChildHandle(Index), ID, Role);
	}
}

TSharedRef<IPropertyTypeCustomization> FArcTacticsNodeDetails::MakeInstance(
	const TWeakPtr<FArcTacticsEditorViewModel> InViewModel)
{
	return MakeShareable(new FArcTacticsNodeDetails(InViewModel));
}

FArcTacticsNodeDetails::FArcTacticsNodeDetails(
	const TWeakPtr<FArcTacticsEditorViewModel> InViewModel)
	: ViewModel(InViewModel)
{
}

void FArcTacticsNodeDetails::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
	Item = Config
		? ArcTacticsNodeDetailsPrivate::ResolveSelection(
			*Config, ArcTacticsNodeDetailsPrivate::GetNodeID(StructPropertyHandle))
		: FArcTacticsEditorSelection::Config();

	HeaderRow.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SImage)
			.Image(this, &FArcTacticsNodeDetails::GetIcon)
			.ColorAndOpacity(this, &FArcTacticsNodeDetails::GetIconColor)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SRichTextBlock)
			.Text(this, &FArcTacticsNodeDetails::GetDescription)
			.ToolTipText(this, &FArcTacticsNodeDetails::GetTooltip)
			.TextStyle(&FArcTacticsEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(
				TEXT("ArcTactics.Node.Normal")))
			+ SRichTextBlock::Decorator(FArcTacticsNodePresentation::CreateTextDecorator(NAME_None))
			+ SRichTextBlock::Decorator(FArcTacticsNodePresentation::CreateTextDecorator(TEXT("b")))
			+ SRichTextBlock::Decorator(FArcTacticsNodePresentation::CreateTextDecorator(TEXT("s")))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SComboButton)
			.OnGetMenuContent_Lambda([WeakViewModel = ViewModel, Selection = Item]()
			{
				return ArcTacticsNodeTypePicker::MakeNodeTypePicker(
					WeakViewModel, Selection.Kind, Selection, true, EArcTacticsNodeTypePickerSurface::Details);
			})
			.OnMenuOpenChanged_Lambda([this](const bool bIsOpen)
			{
				UE_LOGF(
					LogArcTactics,
					Log,
					"[ArcTacticsPicker] NodeType Surface=Details State=%ls Node=%ls",
					bIsOpen ? TEXT("Opened") : TEXT("Closed"),
					*Item.ID.ToString());
			})
			.IsEnabled(!Item.IsConfig())
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &FArcTacticsNodeDetails::GetTypeText)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Visibility(Item.Kind == EArcTacticsEditorItemKind::Step
				|| Item.Kind == EArcTacticsEditorItemKind::ActiveStep
				|| Item.Kind == EArcTacticsEditorItemKind::Consideration
				? EVisibility::Visible
				: EVisibility::Collapsed)
			.IsEnabled_Lambda([WeakViewModel = ViewModel, Selection = Item]()
			{
				const TSharedPtr<FArcTacticsEditorViewModel> PinnedModel = WeakViewModel.Pin();
				return PinnedModel.IsValid() && PinnedModel->CanDelete(Selection);
			})
			.OnClicked_Lambda([WeakViewModel = ViewModel, Selection = Item]()
			{
				if (const TSharedPtr<FArcTacticsEditorViewModel> PinnedModel = WeakViewModel.Pin())
				{
					PinnedModel->Delete(Selection);
				}
				return FReply::Handled();
			})
			.ContentPadding(FMargin(3.0f))
			.ToolTipText(Item.Kind == EArcTacticsEditorItemKind::Step
				? LOCTEXT("DeleteStepTooltip", "Delete Step")
				: Item.Kind == EArcTacticsEditorItemKind::ActiveStep
					? LOCTEXT("DeleteActiveStepTooltip", "Delete While Active Step")
					: LOCTEXT("DeleteConsiderationTooltip", "Delete Consideration"))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("Icons.Delete")))
			]
		]
	]
	.ShouldAutoExpand(true);
}

void FArcTacticsNodeDetails::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const TSharedPtr<IPropertyHandle> NameHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FArcTacticsNodeDefinition, Name));
	const TSharedPtr<IPropertyHandle> InstanceDataHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FArcTacticsNodeDefinition, InstanceData));
	if (NameHandle.IsValid())
	{
		StructBuilder.AddProperty(NameHandle.ToSharedRef());
	}
	if (InstanceDataHandle.IsValid())
	{
		StructBuilder.AddCustomBuilder(MakeShared<ArcTacticsNodeDetailsPrivate::FNodeInstanceDataDetails>(
			InstanceDataHandle, Item.ID));
	}
}

FText FArcTacticsNodeDetails::GetTypeText() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	const UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
	const FArcTacticsNodeDefinition* Node = Config ? Config->FindNodeByID(Item.ID) : nullptr;
	const UScriptStruct* StructType = Node ? Node->Node.GetScriptStruct() : nullptr;
	return StructType ? StructType->GetDisplayNameText() : LOCTEXT("NoNodeType", "None");
}

FText FArcTacticsNodeDetails::GetDescription() const
{
	return ArcTacticsNodeDetailsPrivate::GetPresentation(ViewModel, Item).RichText;
}

FText FArcTacticsNodeDetails::GetTooltip() const
{
	return ArcTacticsNodeDetailsPrivate::GetPresentation(ViewModel, Item).PlainText;
}

const FSlateBrush* FArcTacticsNodeDetails::GetIcon() const
{
	return ArcTacticsNodeDetailsPrivate::GetPresentation(ViewModel, Item).Icon.GetIcon();
}

FSlateColor FArcTacticsNodeDetails::GetIconColor() const
{
	return ArcTacticsNodeDetailsPrivate::GetPresentation(ViewModel, Item).IconColor;
}

#undef LOCTEXT_NAMESPACE
