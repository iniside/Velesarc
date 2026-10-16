// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcTacticsConfigDetails.h"

#include "ArcTacticsEditorViewModel.h"
#include "ArcTacticsNodeTypePicker.h"
#include "Customizations/ArcTacticsNodeDetails.h"
#include "Decision/ArcTacticsConfig.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcTacticsConfigDetails"

namespace ArcTacticsConfigDetailsPrivate
{
	TSharedPtr<IPropertyHandle> GetArrayElement(
		IDetailLayoutBuilder& DetailBuilder,
		const FName PropertyName,
		const int32 Index)
	{
		if (Index == INDEX_NONE)
		{
			return nullptr;
		}
		const TSharedRef<IPropertyHandle> ArrayHandle = DetailBuilder.GetProperty(
			PropertyName, UArcTacticsConfig::StaticClass());
		const TSharedPtr<IPropertyHandleArray> Array = ArrayHandle->AsArray();
		if (!Array.IsValid())
		{
			return nullptr;
		}
		return Array->GetElement(Index);
	}

	TSharedPtr<IPropertyHandle> GetBranchNodeElement(
		const TSharedPtr<IPropertyHandle>& OwnerHandle,
		const FName ArrayName,
		const int32 Index)
	{
		const TSharedPtr<IPropertyHandle> NodesHandle = OwnerHandle.IsValid()
			? OwnerHandle->GetChildHandle(ArrayName)
			: nullptr;
		const TSharedPtr<IPropertyHandleArray> Nodes = NodesHandle.IsValid()
			? NodesHandle->AsArray()
			: nullptr;
		if (!Nodes.IsValid())
		{
			return nullptr;
		}
		return Nodes->GetElement(Index);
	}

	template <typename DefinitionType>
	const TArray<FArcTacticsNodeDefinition>& GetBranchNodes(
		const DefinitionType& Definition,
		const EArcTacticsEditorItemKind Kind)
	{
		switch (Kind)
		{
		case EArcTacticsEditorItemKind::Step:
			return Definition.Steps;
		case EArcTacticsEditorItemKind::ActiveStep:
			return Definition.ActiveSteps;
		default:
			return Definition.Considerations;
		}
	}

	FName GetBranchNodesName(const EArcTacticsEditorItemKind Kind)
	{
		switch (Kind)
		{
		case EArcTacticsEditorItemKind::Step:
			return GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, Steps);
		case EArcTacticsEditorItemKind::ActiveStep:
			return GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, ActiveSteps);
		default:
			return GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, Considerations);
		}
	}

	TSharedPtr<IPropertyHandle> ResolveNodeHandle(
		IDetailLayoutBuilder& DetailBuilder,
		const UArcTacticsConfig& Config,
		const FArcTacticsEditorSelection& Selection)
	{
		if (Selection.Kind == EArcTacticsEditorItemKind::GlobalTask)
		{
			const int32 Index = Config.GlobalTasks.IndexOfByPredicate(
				[&Selection](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == Selection.ID;
				});
			return GetArrayElement(
				DetailBuilder, GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, GlobalTasks), Index);
		}
		const bool bBranchNode = Selection.Kind == EArcTacticsEditorItemKind::Step
			|| Selection.Kind == EArcTacticsEditorItemKind::ActiveStep
			|| Selection.Kind == EArcTacticsEditorItemKind::Consideration;
		const FName BranchNodesName = GetBranchNodesName(Selection.Kind);
		for (int32 Index = 0; Index < Config.Actions.Num(); ++Index)
		{
			const FArcTacticsActionDefinition& Action = Config.Actions[Index];
			if (Selection.Kind == EArcTacticsEditorItemKind::Action
				&& Action.Action.ID == Selection.ID)
			{
				const TSharedPtr<IPropertyHandle> ActionHandle = GetArrayElement(
					DetailBuilder, GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Actions), Index);
				return ActionHandle.IsValid()
					? ActionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, Action))
					: nullptr;
			}
			if (bBranchNode)
			{
				const int32 NodeIndex = GetBranchNodes(Action, Selection.Kind).IndexOfByPredicate(
					[&Selection](const FArcTacticsNodeDefinition& Node)
					{
						return Node.ID == Selection.ID;
					});
				if (NodeIndex != INDEX_NONE)
				{
					const TSharedPtr<IPropertyHandle> ActionHandle = GetArrayElement(
						DetailBuilder, GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Actions), Index);
					return GetBranchNodeElement(ActionHandle, BranchNodesName, NodeIndex);
				}
			}
		}
		if (!bBranchNode)
		{
			return nullptr;
		}
		for (int32 Index = 0; Index < Config.Selectors.Num(); ++Index)
		{
			const FArcTacticsSelectorDefinition& Selector = Config.Selectors[Index];
			const int32 NodeIndex = GetBranchNodes(Selector, Selection.Kind).IndexOfByPredicate(
				[&Selection](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == Selection.ID;
				});
			if (NodeIndex != INDEX_NONE)
			{
				const TSharedPtr<IPropertyHandle> SelectorHandle = GetArrayElement(
					DetailBuilder, GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Selectors), Index);
				return GetBranchNodeElement(SelectorHandle, BranchNodesName, NodeIndex);
			}
		}
		return nullptr;
	}

	TSharedPtr<IPropertyHandle> ResolveActionHandle(
		IDetailLayoutBuilder& DetailBuilder,
		const UArcTacticsConfig& Config,
		const FGuid& ID)
	{
		const int32 Index = Config.Actions.IndexOfByPredicate(
			[&ID](const FArcTacticsActionDefinition& Action)
			{
				return Action.Action.ID == ID;
			});
		return GetArrayElement(
			DetailBuilder, GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Actions), Index);
	}

	TSharedPtr<IPropertyHandle> ResolveSelectorHandle(
		IDetailLayoutBuilder& DetailBuilder,
		const UArcTacticsConfig& Config,
		const FGuid& ID)
	{
		const int32 Index = Config.Selectors.IndexOfByPredicate(
			[&ID](const FArcTacticsSelectorDefinition& Selector)
			{
				return Selector.Selector.ID == ID;
			});
		return GetArrayElement(
			DetailBuilder, GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Selectors), Index);
	}

	TSharedPtr<IPropertyHandle> ResolveSelectorActionOwnerHandle(
		IDetailLayoutBuilder& DetailBuilder,
		const UArcTacticsConfig& Config,
		const FGuid& ID)
	{
		const int32 Index = Config.Selectors.IndexOfByPredicate(
			[&ID](const FArcTacticsSelectorDefinition& Selector)
			{
				return Selector.Action.ID == ID;
			});
		return GetArrayElement(
			DetailBuilder, GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Selectors), Index);
	}

	void AddBranchNodesCategory(
		IDetailLayoutBuilder& DetailBuilder,
		const TWeakPtr<FArcTacticsEditorViewModel>& ViewModel,
		const FArcTacticsEditorSelection& Target,
		const EArcTacticsEditorItemKind Kind,
		const TSharedPtr<IPropertyHandle>& NodesHandle)
	{
		const TSharedPtr<IPropertyHandleArray> Nodes = NodesHandle.IsValid()
			? NodesHandle->AsArray()
			: nullptr;
		uint32 NumNodes = 0;
		if (Nodes.IsValid())
		{
			Nodes->GetNumElements(NumNodes);
		}

		FName CategoryName(TEXT("Considerations"));
		FText CategoryText = LOCTEXT("ConsiderationsCategory", "Considerations");
		FText EmptyText = LOCTEXT("NoConsiderations", "No considerations");
		FText AddTooltip = LOCTEXT("AddConsiderationTooltip", "Add Consideration");
		const FSlateBrush* AddIcon = ArcTacticsNodeTypePicker::GetAddConsiderationIcon();
		if (Kind == EArcTacticsEditorItemKind::Step)
		{
			CategoryName = TEXT("Steps");
			CategoryText = LOCTEXT("StepsCategory", "Steps");
			EmptyText = LOCTEXT("NoSteps", "No steps");
			AddTooltip = LOCTEXT("AddStepTooltip", "Add Step");
			AddIcon = FAppStyle::GetBrush(TEXT("Icons.Plus"));
		}
		else if (Kind == EArcTacticsEditorItemKind::ActiveStep)
		{
			CategoryName = TEXT("WhileActive");
			CategoryText = LOCTEXT("ActiveStepsCategory", "While Active");
			EmptyText = LOCTEXT("NoActiveSteps", "No while active steps");
			AddTooltip = LOCTEXT("AddActiveStepTooltip", "Add While Active Step");
			AddIcon = FAppStyle::GetBrush(TEXT("Icons.Plus"));
		}
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, CategoryText);
		Category.AddCustomRow(CategoryText)
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(EmptyText)
				.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				.Visibility(NumNodes == 0 ? EVisibility::Visible : EVisibility::Collapsed)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SComboButton)
				.IsEnabled_Lambda([ViewModel, Target]()
				{
					const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
					return Model.IsValid() && Model->IsSelectionValid(Target);
				})
				.IsFocusable(false)
				.OnGetMenuContent_Lambda([ViewModel, Target, Kind]()
				{
					return ArcTacticsNodeTypePicker::MakeNodeTypePicker(
						ViewModel,
						Kind,
						Target,
						false,
						EArcTacticsNodeTypePickerSurface::Details,
						true);
				})
				.HasDownArrow(false)
				.ContentPadding(FMargin(3.0f))
				.ToolTipText(AddTooltip)
				.ButtonContent()
				[
					SNew(SImage)
					.Image(AddIcon)
				]
			]
		];

		for (uint32 Index = 0; Index < NumNodes; ++Index)
		{
			const TSharedRef<IPropertyHandle> Element = Nodes->GetElement(Index);
			Category.AddProperty(Element);
		}
	}
}

TSharedRef<IDetailCustomization> FArcTacticsConfigDetails::MakeInstance(
	const TWeakPtr<FArcTacticsEditorViewModel> InViewModel)
{
	return MakeShareable(new FArcTacticsConfigDetails(InViewModel));
}

FArcTacticsConfigDetails::FArcTacticsConfigDetails(
	const TWeakPtr<FArcTacticsEditorViewModel> InViewModel)
	: ViewModel(InViewModel)
{
}

FArcTacticsConfigDetails::~FArcTacticsConfigDetails()
{
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		Model->OnSelectionChanged().Remove(SelectionChangedHandle);
		Model->OnModelChanged().Remove(ModelChangedHandle);
	}
}

void FArcTacticsConfigDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
	if (!Config)
	{
		return;
	}

	PropertyUtilities = DetailBuilder.GetPropertyUtilities();
	Model->OnSelectionChanged().Remove(SelectionChangedHandle);
	Model->OnModelChanged().Remove(ModelChangedHandle);
	SelectionChangedHandle = Model->OnSelectionChanged().AddSP(
		this, &FArcTacticsConfigDetails::HandleSelectionChanged);
	ModelChangedHandle = Model->OnModelChanged().AddSP(
		this, &FArcTacticsConfigDetails::HandleModelChanged);

	const FName HiddenProperties[] = {
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, DecisionSettingsID),
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, GlobalParametersID),
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, DecisionSettings),
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, GlobalParameters),
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, ContextID),
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, GlobalTasks),
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Actions),
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Selectors),
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, EditorBindings),
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, EditorContextData),
		GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, CompiledData)
	};
	for (const FName PropertyName : HiddenProperties)
	{
		DetailBuilder.HideProperty(DetailBuilder.GetProperty(PropertyName, UArcTacticsConfig::StaticClass()));
	}

	const FArcTacticsEditorSelection& Selection = Model->GetSelection();
	if (Selection.IsConfig())
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
			TEXT("Settings"), LOCTEXT("SettingsCategory", "Settings"));
		const TSharedRef<IPropertyHandle> DecisionSettings = DetailBuilder.GetProperty(
			GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, DecisionSettings), UArcTacticsConfig::StaticClass());
		const TSharedRef<IPropertyHandle> GlobalParameters = DetailBuilder.GetProperty(
			GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, GlobalParameters), UArcTacticsConfig::StaticClass());
		ArcTacticsNodeDetails::StampBindingScopeChildren(
			DecisionSettings, Config->DecisionSettingsID, EArcTacticsPropertyRole::Parameter);
		Category.AddProperty(DecisionSettings);
		Category.AddCustomBuilder(ArcTacticsNodeDetails::MakeParameterBagDetails(
			GlobalParameters, DetailBuilder.GetPropertyUtilities(), Config->GlobalParametersID));
		return;
	}

	if (Selection.Kind == EArcTacticsEditorItemKind::Context)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
			TEXT("Context"), LOCTEXT("ContextCategory", "Context"));
		Category.AddProperty(DetailBuilder.GetProperty(
			GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, EditorContextData), UArcTacticsConfig::StaticClass()));
		return;
	}

	if (Selection.Kind == EArcTacticsEditorItemKind::Selector)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
			TEXT("Selector"), LOCTEXT("SelectorCategory", "Selector"));
		const TSharedPtr<IPropertyHandle> Handle =
			ArcTacticsConfigDetailsPrivate::ResolveSelectorHandle(DetailBuilder, *Config, Selection.ID);
		if (!Handle.IsValid())
		{
			return;
		}
		if (const TSharedPtr<IPropertyHandle> SelectorHandle = Handle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcTacticsSelectorDefinition, Selector)))
		{
			Category.AddProperty(SelectorHandle.ToSharedRef());
		}
		return;
	}

	if (Selection.Kind == EArcTacticsEditorItemKind::Action)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
			TEXT("Action"), LOCTEXT("ActionCategory", "Action"));
		if (const TSharedPtr<IPropertyHandle> Handle =
			ArcTacticsConfigDetailsPrivate::ResolveActionHandle(DetailBuilder, *Config, Selection.ID))
		{
			const TSharedPtr<IPropertyHandle> ActionHandle = Handle->GetChildHandle(
				GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, Action));
			const TSharedPtr<IPropertyHandle> StepsHandle = Handle->GetChildHandle(
				GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, Steps));
			const TSharedPtr<IPropertyHandle> ActiveStepsHandle = Handle->GetChildHandle(
				GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, ActiveSteps));
			const TSharedPtr<IPropertyHandle> ConsiderationsHandle = Handle->GetChildHandle(
				GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, Considerations));
			const TSharedPtr<IPropertyHandle> ParametersHandle = Handle->GetChildHandle(
				GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, Parameters));
			const TSharedPtr<IPropertyHandle> ParameterBagHandle = Handle->GetChildHandle(
				GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, ParameterBag));
			if (ActionHandle.IsValid())
			{
				Category.AddProperty(ActionHandle.ToSharedRef());
			}
			const FArcTacticsActionDefinition* Action = Config->Actions.FindByPredicate(
				[&Selection](const FArcTacticsActionDefinition& Candidate)
				{
					return Candidate.Action.ID == Selection.ID;
				});
			if (ParametersHandle.IsValid() && Action)
			{
				ArcTacticsNodeDetails::StampBindingScopeChildren(
					ParametersHandle, Action->ParametersID, EArcTacticsPropertyRole::Parameter);
				Category.AddProperty(ParametersHandle.ToSharedRef());
			}
			if (ParameterBagHandle.IsValid() && Action)
			{
				Category.AddCustomBuilder(ArcTacticsNodeDetails::MakeParameterBagDetails(
					ParameterBagHandle,
					DetailBuilder.GetPropertyUtilities(),
					Action->ParameterBagID));
			}
			ArcTacticsConfigDetailsPrivate::AddBranchNodesCategory(
				DetailBuilder, ViewModel, Selection, EArcTacticsEditorItemKind::ActiveStep, ActiveStepsHandle);
			ArcTacticsConfigDetailsPrivate::AddBranchNodesCategory(
				DetailBuilder, ViewModel, Selection, EArcTacticsEditorItemKind::Step, StepsHandle);
			ArcTacticsConfigDetailsPrivate::AddBranchNodesCategory(
				DetailBuilder,
				ViewModel,
				Selection,
				EArcTacticsEditorItemKind::Consideration,
				ConsiderationsHandle);
		}
		return;
	}

	if (Selection.Kind == EArcTacticsEditorItemKind::SelectorAction)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
			TEXT("Node"), LOCTEXT("SelectorActionCategory", "Selector Action"));
		const TSharedPtr<IPropertyHandle> Handle =
			ArcTacticsConfigDetailsPrivate::ResolveSelectorActionOwnerHandle(DetailBuilder, *Config, Selection.ID);
		if (!Handle.IsValid())
		{
			return;
		}
		if (const TSharedPtr<IPropertyHandle> ActionHandle = Handle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcTacticsSelectorDefinition, Action)))
		{
			Category.AddProperty(ActionHandle.ToSharedRef());
		}
		const FArcTacticsSelectorDefinition* Selector = Config->Selectors.FindByPredicate(
			[&Selection](const FArcTacticsSelectorDefinition& Candidate)
			{
				return Candidate.Action.ID == Selection.ID;
			});
		const TSharedPtr<IPropertyHandle> ParametersHandle = Handle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcTacticsSelectorDefinition, Parameters));
		const TSharedPtr<IPropertyHandle> ParameterBagHandle = Handle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcTacticsSelectorDefinition, ParameterBag));
		if (ParametersHandle.IsValid() && Selector)
		{
			ArcTacticsNodeDetails::StampBindingScopeChildren(
				ParametersHandle, Selector->ParametersID, EArcTacticsPropertyRole::Parameter);
			Category.AddProperty(ParametersHandle.ToSharedRef());
		}
		if (ParameterBagHandle.IsValid() && Selector)
		{
			Category.AddCustomBuilder(ArcTacticsNodeDetails::MakeParameterBagDetails(
				ParameterBagHandle,
				DetailBuilder.GetPropertyUtilities(),
				Selector->ParameterBagID));
		}
		ArcTacticsConfigDetailsPrivate::AddBranchNodesCategory(
			DetailBuilder,
			ViewModel,
			Selection,
			EArcTacticsEditorItemKind::ActiveStep,
			Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcTacticsSelectorDefinition, ActiveSteps)));
		ArcTacticsConfigDetailsPrivate::AddBranchNodesCategory(
			DetailBuilder,
			ViewModel,
			Selection,
			EArcTacticsEditorItemKind::Step,
			Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcTacticsSelectorDefinition, Steps)));
		ArcTacticsConfigDetailsPrivate::AddBranchNodesCategory(
			DetailBuilder,
			ViewModel,
			Selection,
			EArcTacticsEditorItemKind::Consideration,
			Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcTacticsSelectorDefinition, Considerations)));
		return;
	}

	FText CategoryText = LOCTEXT("ConsiderationCategory", "Consideration");
	if (Selection.Kind == EArcTacticsEditorItemKind::GlobalTask)
	{
		CategoryText = LOCTEXT("GlobalTaskCategory", "Global Task");
	}
	else if (Selection.Kind == EArcTacticsEditorItemKind::Step)
	{
		CategoryText = LOCTEXT("StepCategory", "Step");
	}
	else if (Selection.Kind == EArcTacticsEditorItemKind::ActiveStep)
	{
		CategoryText = LOCTEXT("ActiveStepCategory", "While Active Step");
	}
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Node"), CategoryText);
	if (const TSharedPtr<IPropertyHandle> Handle =
		ArcTacticsConfigDetailsPrivate::ResolveNodeHandle(DetailBuilder, *Config, Selection))
	{
		Category.AddProperty(Handle.ToSharedRef());
	}
}

void FArcTacticsConfigDetails::HandleSelectionChanged(const FArcTacticsEditorSelection& Selection)
{
	RequestRefresh();
}

void FArcTacticsConfigDetails::HandleModelChanged(const EArcTacticsAuthoringChange Change)
{
	if (Change == EArcTacticsAuthoringChange::Structure)
	{
		RequestRefresh();
	}
}

void FArcTacticsConfigDetails::RequestRefresh() const
{
	if (const TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
	{
		Utilities->RequestForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
