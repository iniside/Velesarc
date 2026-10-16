// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsConfigEditor.h"

#include "ArcPropertyFunctionEditor.h"
#include "ArcTacticsBindingExtension.h"
#include "ArcTacticsEditorViewModel.h"
#include "Customizations/ArcTacticsConfigDetails.h"
#include "Customizations/ArcTacticsNodeDetails.h"
#include "Decision/ArcTacticsConfig.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IDetailTreeNode.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyBindingExtension.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "PropertyPath.h"
#include "Simulator/SArcTacticsSimulator.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SArcTacticsBindings.h"
#include "Widgets/SArcTacticsCompileResults.h"
#include "Widgets/SArcTacticsMemory.h"
#include "Widgets/SArcTacticsView.h"

#define LOCTEXT_NAMESPACE "ArcTacticsConfigEditor"

namespace ArcTacticsConfigEditorPrivate
{
	TSharedPtr<IPropertyHandle> ResolveBindingHandle(
		const TSharedPtr<IPropertyHandle>& Candidate,
		const FPropertyBindingPath& BindingPath)
	{
		if (!Candidate.IsValid())
		{
			return nullptr;
		}
		const FString* StructIDText = Candidate->GetInstanceMetaData(
			UE::PropertyBinding::MetaDataStructIDName);
		if (!StructIDText)
		{
			return nullptr;
		}
		FGuid StructID;
		LexFromString(StructID, **StructIDText);
		if (StructID != BindingPath.GetStructID() || BindingPath.IsPathEmpty())
		{
			return nullptr;
		}

		TSharedPtr<IPropertyHandle> Current = Candidate;
		for (int32 SegmentIndex = 0; SegmentIndex < BindingPath.NumSegments(); ++SegmentIndex)
		{
			const FPropertyBindingPathSegment& Segment = BindingPath.GetSegment(SegmentIndex);
			const FProperty* CurrentProperty = Current.IsValid() ? Current->GetProperty() : nullptr;
			if (!CurrentProperty || CurrentProperty->GetFName() != Segment.GetName())
			{
				Current = Current.IsValid()
					? Current->GetChildHandle(Segment.GetName(), true)
					: nullptr;
			}
			if (!Current.IsValid())
			{
				return nullptr;
			}
			if (Segment.GetArrayIndex() != INDEX_NONE)
			{
				const TSharedPtr<IPropertyHandleArray> ArrayHandle = Current->AsArray();
				if (!ArrayHandle.IsValid())
				{
					return nullptr;
				}
				Current = ArrayHandle->GetElement(Segment.GetArrayIndex());
			}
		}
		return Current;
	}

	TSharedPtr<IPropertyHandle> FindBindingHandle(
		const TArray<TSharedRef<IDetailTreeNode>>& Nodes,
		const FPropertyBindingPath& BindingPath)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			if (const TSharedPtr<IPropertyHandle> Handle = ResolveBindingHandle(
				Node->CreatePropertyHandle(), BindingPath))
			{
				return Handle;
			}
			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children, true);
			if (const TSharedPtr<IPropertyHandle> Handle = FindBindingHandle(Children, BindingPath))
			{
				return Handle;
			}
		}
		return nullptr;
	}

	TSharedPtr<IPropertyHandle> FindNamedHandle(
		const TArray<TSharedRef<IDetailTreeNode>>& Nodes,
		const FName PropertyName)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			const TSharedPtr<IPropertyHandle> Handle = Node->CreatePropertyHandle();
			const FProperty* Property = Handle.IsValid() ? Handle->GetProperty() : nullptr;
			if (Property && Property->GetFName() == PropertyName)
			{
				return Handle;
			}
			if (Handle.IsValid())
			{
				if (const TSharedPtr<IPropertyHandle> Child = Handle->GetChildHandle(PropertyName, true))
				{
					return Child;
				}
			}
			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children, true);
			if (const TSharedPtr<IPropertyHandle> Child = FindNamedHandle(Children, PropertyName))
			{
				return Child;
			}
		}
		return nullptr;
	}
}

const FName FArcTacticsConfigEditor::EditorTabID(TEXT("ArcTacticsConfigEditor_Main"));
const FName FArcTacticsConfigEditor::SimulatorTabID(TEXT("ArcTacticsConfigEditor_Simulator"));

void FArcTacticsConfigEditor::Init(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcTacticsConfig* InConfig)
{
	InConfig->SetFlags(RF_Transactional);
	InConfig->EnsureValidNodeIDs();
	InConfig->RefreshEditorBindingData();
	ViewModel = FArcTacticsEditorViewModel::Create(InConfig);

	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);
	const TWeakPtr<FArcTacticsEditorViewModel> WeakViewModel = ViewModel;
	DetailsView->RegisterInstancedCustomPropertyLayout(
		UArcTacticsConfig::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(
			&FArcTacticsConfigDetails::MakeInstance, WeakViewModel));
	DetailsView->RegisterInstancedCustomPropertyTypeLayout(
		FArcTacticsNodeDefinition::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(
			&FArcTacticsNodeDetails::MakeInstance, WeakViewModel));
	DetailsView->SetExtensionHandler(MakeShared<FArcTacticsBindingExtension>());
	DetailsView->SetChildrenCustomizationHandler(MakeShared<FArcPropertyFunctionChildrenCustomization>(
		UArcTacticsConfig::StaticClass()));
	DetailsView->SetObject(InConfig);

	const TSharedRef<FTabManager::FLayout> Layout =
		FTabManager::NewLayout(TEXT("ArcTacticsConfigEditor_Layout_v3"))
		->AddArea(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->SetHideTabWell(true)
				->AddTab(EditorTabID, ETabState::OpenedTab))
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->AddTab(SimulatorTabID, ETabState::OpenedTab)));

	InitAssetEditor(
		Mode,
		InitToolkitHost,
		TEXT("ArcTacticsConfigEditorApp"),
		Layout,
		true,
		true,
		InConfig);
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

FName FArcTacticsConfigEditor::GetToolkitFName() const
{
	return TEXT("ArcTacticsConfigEditor");
}

FText FArcTacticsConfigEditor::GetBaseToolkitName() const
{
	return LOCTEXT("EditorName", "Arc Tactics Configuration");
}

FString FArcTacticsConfigEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricPrefix", "Arc Tactics ").ToString();
}

FLinearColor FArcTacticsConfigEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.12f, 0.28f, 0.18f, 0.5f);
}

void FArcTacticsConfigEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	InTabManager->RegisterTabSpawner(
		EditorTabID,
		FOnSpawnTab::CreateSP(this, &FArcTacticsConfigEditor::SpawnEditorTab))
		.SetDisplayName(LOCTEXT("ConfigurationTab", "Configuration"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.Tabs.Details")));
	InTabManager->RegisterTabSpawner(
		SimulatorTabID,
		FOnSpawnTab::CreateSP(this, &FArcTacticsConfigEditor::SpawnSimulatorTab))
		.SetDisplayName(LOCTEXT("SimulatorTab", "Simulator"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.Tabs.StatsViewer")));
}

void FArcTacticsConfigEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(SimulatorTabID);
	InTabManager->UnregisterTabSpawner(EditorTabID);
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

TSharedRef<SDockTab> FArcTacticsConfigEditor::SpawnEditorTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.Label(LOCTEXT("ConfigurationTab", "Configuration"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(0.72f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)
				+ SSplitter::Slot()
				.Value(0.46f)
				[
					SAssignNew(TacticsView, SArcTacticsView, ViewModel.ToSharedRef(), GetToolkitCommands())
				]
				+ SSplitter::Slot()
				.Value(0.54f)
				[
					DetailsView.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot()
			.FillHeight(0.28f)
			.MaxHeight(320.0f)
			.Padding(4.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("BindingsTab", "Bindings"))
						.OnClicked_Lambda([this]()
						{
							BottomPanelSwitcher->SetActiveWidgetIndex(0);
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("CompileResultsTab", "Compile Results"))
						.OnClicked_Lambda([this]()
						{
							BottomPanelSwitcher->SetActiveWidgetIndex(1);
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("MemoryTab", "Memory"))
						.OnClicked_Lambda([this]()
						{
							BottomPanelSwitcher->SetActiveWidgetIndex(2);
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(BottomPanelSwitcher, SWidgetSwitcher)
					.WidgetIndex(0)
					+ SWidgetSwitcher::Slot()
					[
						SNew(SArcTacticsBindings, ViewModel.ToSharedRef())
						.OnBindingPathClicked(this, &FArcTacticsConfigEditor::NavigateToBindingPath)
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SArcTacticsCompileResults, ViewModel.ToSharedRef())
						.OnDiagnosticClicked(this, &FArcTacticsConfigEditor::NavigateToDiagnostic)
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SArcTacticsMemory, ViewModel.ToSharedRef())
						.OnMemoryUserClicked(this, &FArcTacticsConfigEditor::NavigateToMemoryUser)
					]
				]
			]
		];
	return Tab;
}

TSharedRef<SDockTab> FArcTacticsConfigEditor::SpawnSimulatorTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("SimulatorTab", "Simulator"))
		[
			SNew(SArcTacticsSimulator, ViewModel.IsValid() ? ViewModel->GetConfig() : nullptr)
		];
}

void FArcTacticsConfigEditor::OpenSimulatorTab()
{
	if (const TSharedPtr<FTabManager> Manager = GetTabManager())
	{
		Manager->TryInvokeTab(SimulatorTabID);
	}
}

void FArcTacticsConfigEditor::ExtendToolbar()
{
	const TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension(
		TEXT("Asset"),
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda(
			[this](FToolBarBuilder& ToolbarBuilder)
			{
				ToolbarBuilder.BeginSection(TEXT("ArcTactics"));
				ToolbarBuilder.AddToolBarButton(
					FUIAction(FExecuteAction::CreateSP(this, &FArcTacticsConfigEditor::CompileConfig)),
					NAME_None,
					LOCTEXT("Compile", "Compile"),
					LOCTEXT("CompileTooltip", "Compile the authored tasks, actions, selectors, and property bindings."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")));
				ToolbarBuilder.AddToolBarButton(
					FUIAction(FExecuteAction::CreateSP(this, &FArcTacticsConfigEditor::OpenSimulatorTab)),
					NAME_None,
					LOCTEXT("Simulator", "Simulator"),
					LOCTEXT("SimulatorTooltip", "Open the decision simulator: set consideration inputs and see each action's chance of being picked."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.Tabs.StatsViewer")));
				ToolbarBuilder.EndSection();
			}));
	AddToolbarExtender(ToolbarExtender);
}

void FArcTacticsConfigEditor::CompileConfig()
{
	if (!ViewModel.IsValid())
	{
		return;
	}
	const bool bCompiled = ViewModel->Compile();
	if (BottomPanelSwitcher.IsValid())
	{
		BottomPanelSwitcher->SetActiveWidgetIndex(1);
	}

	FNotificationInfo Notification(
		bCompiled
			? LOCTEXT("CompileSucceeded", "Arc Tactics configuration compiled successfully.")
			: LOCTEXT("CompileFailed", "Arc Tactics compilation failed. Review Compile Results."));
	Notification.ExpireDuration = 4.0f;
	const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Notification);
	if (Item.IsValid())
	{
		Item->SetCompletionState(
			bCompiled ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

void FArcTacticsConfigEditor::NavigateToBindingPath(
	const bool bIsStruct,
	const FPropertyBindingPath& BindingPath)
{
	if (!ViewModel.IsValid())
	{
		return;
	}
	FArcTacticsEditorSelection Selection;
	if (!ViewModel->FindSelectionForStructID(BindingPath.GetStructID(), Selection))
	{
		Selection = FArcTacticsEditorSelection::Config();
	}
	FocusSelectionProperty(
		Selection,
		bIsStruct || BindingPath.IsPathEmpty() ? nullptr : &BindingPath,
		NAME_None);
}

void FArcTacticsConfigEditor::NavigateToDiagnostic(const FGuid NodeID)
{
	if (!ViewModel.IsValid())
	{
		return;
	}
	FArcTacticsEditorSelection Selection;
	if (!ViewModel->FindSelectionForStructID(NodeID, Selection))
	{
		Selection = FArcTacticsEditorSelection::Config();
	}
	ViewModel->SetSelection(Selection);
	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}
}

void FArcTacticsConfigEditor::NavigateToMemoryUser(const FGuid StructID, const FName PropertyName)
{
	if (!ViewModel.IsValid())
	{
		return;
	}
	FArcTacticsEditorSelection Selection;
	if (!ViewModel->FindSelectionForStructID(StructID, Selection))
	{
		Selection = FArcTacticsEditorSelection::Config();
	}
	FocusSelectionProperty(Selection, nullptr, PropertyName);
}

void FArcTacticsConfigEditor::FocusSelectionProperty(
	const FArcTacticsEditorSelection& Selection,
	const FPropertyBindingPath* BindingPath,
	const FName PropertyName)
{
	if (!ViewModel.IsValid() || !DetailsView.IsValid())
	{
		return;
	}
	ViewModel->SetSelection(Selection);
	DetailsView->ForceRefresh();

	TArray<TSharedRef<IDetailTreeNode>> RootNodes = DetailsView->GetRootTreeNodes();
	const TSharedPtr<IPropertyHandle> Handle = BindingPath
		? ArcTacticsConfigEditorPrivate::FindBindingHandle(RootNodes, *BindingPath)
		: PropertyName.IsNone()
			? nullptr
			: ArcTacticsConfigEditorPrivate::FindNamedHandle(RootNodes, PropertyName);
	const TSharedPtr<FPropertyPath> PropertyPath = Handle.IsValid()
		? Handle->CreateFPropertyPath()
		: nullptr;
	if (PropertyPath.IsValid() && PropertyPath->IsValid())
	{
		DetailsView->ScrollPropertyIntoView(*PropertyPath, true);
		DetailsView->HighlightProperty(*PropertyPath);
	}
}

#undef LOCTEXT_NAMESPACE
