// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAssetEditor_MassEntityConfig.h"

#include "SArcMassTraitList.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "MassAssortedFragmentsTrait.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SSplitter.h"
#include "UObject/StructOnScope.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Preview/SArcMassEntityConfigPreviewViewport.h"

#define LOCTEXT_NAMESPACE "ArcAssetEditor_MassEntityConfig"

const FName FArcAssetEditor_MassEntityConfig::TraitListTabId(TEXT("MassConfigEditor_TraitList"));
const FName FArcAssetEditor_MassEntityConfig::DetailsTabId(TEXT("MassConfigEditor_Details"));
const FName FArcAssetEditor_MassEntityConfig::PreviewTabId(TEXT("MassConfigEditor_Preview"));
const FName FArcAssetEditor_MassEntityConfig::ToolkitFName(TEXT("ArcMassEntityConfigEditor"));
const FName FArcAssetEditor_MassEntityConfig::AppIdentifier(TEXT("ArcMassEntityConfigEditorApp"));

// ---------- Factory ----------

TSharedRef<FArcAssetEditor_MassEntityConfig> FArcAssetEditor_MassEntityConfig::CreateEditor(
	const EToolkitMode::Type Mode
	, const TSharedPtr<IToolkitHost>& InitToolkitHost
	, UObject* ObjectToEdit)
{
	TSharedRef<FArcAssetEditor_MassEntityConfig> NewEditor(new FArcAssetEditor_MassEntityConfig());
	UMassEntityConfigAsset* ConfigAsset = CastChecked<UMassEntityConfigAsset>(ObjectToEdit);
	NewEditor->InitEditor(Mode, InitToolkitHost, ConfigAsset);
	return NewEditor;
}

// ---------- Init ----------

void FArcAssetEditor_MassEntityConfig::InitEditor(
	const EToolkitMode::Type Mode
	, const TSharedPtr<IToolkitHost>& InitToolkitHost
	, UMassEntityConfigAsset* ObjectToEdit)
{
	FPropertyEditorModule& PropModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Create trait details view (shows selected trait UObject properties)
	FDetailsViewArgs TraitArgs;
	TraitArgs.bAllowSearch = true;
	TraitArgs.bShowOptions = false;
	TraitArgs.bShowObjectLabel = false;
	TraitArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	TraitDetailsView = PropModule.CreateDetailView(TraitArgs);

	// Create struct details view (for assorted fragment/tag struct properties)
	FDetailsViewArgs StructArgs;
	StructArgs.bShowOptions = false;
	StructArgs.bAllowSearch = false;
	StructArgs.bShowObjectLabel = false;
	StructArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FStructureDetailsViewArgs StructViewArgs;
	StructDetailsView = PropModule.CreateStructureDetailView(StructArgs, StructViewArgs, nullptr);

	// Create config details view (for FMassEntityConfig properties: Parent, ConfigGuid)
	FDetailsViewArgs ConfigArgs;
	ConfigArgs.bAllowSearch = false;
	ConfigArgs.bShowOptions = false;
	ConfigArgs.bShowObjectLabel = false;
	ConfigArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	ConfigDetailsView = PropModule.CreateDetailView(ConfigArgs);
	ConfigDetailsView->SetObject(ObjectToEdit);
	// Only show Parent and ConfigGuid in the bottom panel
	ConfigDetailsView->SetIsPropertyVisibleDelegate(
		FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& PropertyAndParent) -> bool
		{
			FName PropName = PropertyAndParent.Property.GetFName();
			return PropName == TEXT("Parent")
				|| PropName == TEXT("ConfigGuid");
		}));
	ConfigDetailsView->SetRootExpansionStates(true, true);

	// Hook config changes to detect parent changes and rebuild tree
	ConfigDetailsView->OnFinishedChangingProperties().AddSP(
		this, &FArcAssetEditor_MassEntityConfig::OnConfigPropertyChanged);

	// Create trait list widget (left panel)
	TraitListWidget = SNew(SArcMassTraitList, ObjectToEdit)
		.OnTraitSelected(FArcOnTraitSelected::CreateSP(
			this, &FArcAssetEditor_MassEntityConfig::OnTraitSelected));

	// Define two-panel layout: left trait list (30%), right details+preview (70%)
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ArcMassEntityConfigEditor_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->SetHideTabWell(true)
					->AddTab(TraitListTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
					->AddTab(PreviewTabId, ETabState::OpenedTab)
					->SetForegroundTab(DetailsTabId)
				)
			)
		);

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(ObjectToEdit);

	FAssetEditorToolkit::InitAssetEditor(
		Mode
		, InitToolkitHost
		, AppIdentifier
		, Layout
		, true   // bCreateDefaultStandaloneMenu
		, true   // bCreateDefaultToolbar
		, ObjectsToEdit);

	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

// ---------- Tab Spawners ----------

void FArcAssetEditor_MassEntityConfig::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu_MassConfigEditor", "Mass Entity Config Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(TraitListTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_MassEntityConfig::SpawnTab_TraitList))
		.SetDisplayName(LOCTEXT("TraitListTab", "Traits"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DetailsTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_MassEntityConfig::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(PreviewTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_MassEntityConfig::SpawnTab_Preview))
		.SetDisplayName(LOCTEXT("PreviewTab", "Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));
}

void FArcAssetEditor_MassEntityConfig::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(TraitListTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(PreviewTabId);
}

TSharedRef<SDockTab> FArcAssetEditor_MassEntityConfig::SpawnTab_TraitList(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("TraitListTitle", "Traits"))
		[
			TraitListWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcAssetEditor_MassEntityConfig::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTitle", "Details"))
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.Value(0.8f)
			[
				SAssignNew(DetailsSwitcher, SWidgetSwitcher)
				// Slot 0: No selection placeholder
				+ SWidgetSwitcher::Slot()
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NoSelection", "Select a trait to edit its properties."))
					]
				]
				// Slot 1: Trait UObject details
				+ SWidgetSwitcher::Slot()
				[
					TraitDetailsView.ToSharedRef()
				]
				// Slot 2: Struct details (single assorted fragment/tag)
				+ SWidgetSwitcher::Slot()
				[
					StructDetailsView->GetWidget().ToSharedRef()
				]
				// Slot 3: Flattened assorted fragments scroll box
				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(AssortedFragmentsScrollBox, SScrollBox)
				]
			]
			+ SSplitter::Slot()
			.Value(0.2f)
			[
				ConfigDetailsView.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> FArcAssetEditor_MassEntityConfig::SpawnTab_Preview(const FSpawnTabArgs& Args)
{
	PreviewViewport = SNew(SArcMassEntityConfigPreviewViewport)
		.ConfigAsset(GetEditingConfigAsset());

	return SNew(SDockTab)
		.Label(LOCTEXT("PreviewTitle", "Preview"))
		[
			PreviewViewport.ToSharedRef()
		];
}

void FArcAssetEditor_MassEntityConfig::PostRegenerateMenusAndToolbars()
{
	// Intentionally skip FSimpleAssetEditor::PostRegenerateMenusAndToolbars()
	// which sets up a default property panel using its private EditingObjects.
	// We have our own custom trait/details views instead.
}

// ---------- Toolbar ----------

void FArcAssetEditor_MassEntityConfig::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset"
		, EExtensionHook::After
		, GetToolkitCommands()
		, FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Validation");
			{
				ToolbarBuilder.AddToolBarButton(
					FUIAction(
						FExecuteAction::CreateLambda([this]() { OnVerifyClicked(); })
					)
					, NAME_None
					, LOCTEXT("VerifyBtn", "Verify")
					, LOCTEXT("VerifyBtnTip", "Validate the entity config template")
					, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Check")
				);
			}
			ToolbarBuilder.EndSection();
		})
	);

	AddToolbarExtender(ToolbarExtender);
}

void FArcAssetEditor_MassEntityConfig::OnVerifyClicked()
{
	UMassEntityConfigAsset* Asset = GetEditingConfigAsset();
	if (!Asset) return;

	Asset->ValidateEntityConfig();

	FNotificationInfo Info(LOCTEXT("VerifyComplete", "Entity config validation complete."));
	Info.ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

// ---------- Trait Selection ----------

void FArcAssetEditor_MassEntityConfig::OnTraitSelected(TSharedPtr<FArcMassTraitListItem> SelectedItem)
{
	if (!SelectedItem.IsValid())
	{
		DetailsSwitcher->SetActiveWidgetIndex(0);
		return;
	}

	// --- Category node: show all child traits in details panel ---
	if (SelectedItem->IsGroup())
	{
		TArray<UObject*> ChildObjects;
		TFunction<void(TSharedPtr<FArcMassTraitListItem>)> CollectTraits =
			[&CollectTraits, &ChildObjects](TSharedPtr<FArcMassTraitListItem> Item)
		{
			if (!Item.IsValid()) return;

			// Assorted category nodes hold a reference to their UMassAssortedFragmentsTrait
			if (Item->bIsAssortedCategory && Item->Trait.IsValid())
			{
				ChildObjects.AddUnique(Item->Trait.Get());
			}

			// Trait leaf nodes
			if (Item->ItemType == EArcMassTraitListItemType::Trait && Item->Trait.IsValid())
			{
				ChildObjects.AddUnique(Item->Trait.Get());
			}

			// Recurse into children for groups
			for (const TSharedPtr<FArcMassTraitListItem>& Child : Item->Children)
			{
				CollectTraits(Child);
			}
		};

		// Assorted category: show flattened fragments + tags in scroll box
		if (SelectedItem->bIsAssortedCategory && SelectedItem->Trait.IsValid())
		{
			ShowAssortedFragmentsFlattened(SelectedItem->Trait.Get());
			return;
		}

		// Assorted Fragments sub-category: show only fragments
		if (SelectedItem->bIsAssortedFragmentSubCategory && SelectedItem->Trait.IsValid())
		{
			ShowAssortedFragmentsFlattened(SelectedItem->Trait.Get(), /*bShowFragments=*/true, /*bShowTags=*/false);
			return;
		}

		// Assorted Tags sub-category: show only tags
		if (SelectedItem->bIsAssortedTagSubCategory && SelectedItem->Trait.IsValid())
		{
			ShowAssortedFragmentsFlattened(SelectedItem->Trait.Get(), /*bShowFragments=*/false, /*bShowTags=*/true);
			return;
		}

		// Regular/parent category: collect all child trait UObjects
		for (const TSharedPtr<FArcMassTraitListItem>& Child : SelectedItem->Children)
		{
			CollectTraits(Child);
		}

		if (ChildObjects.Num() > 0)
		{
			TraitDetailsView->SetObjects(ChildObjects);
			TraitDetailsView->SetRootExpansionStates(true, true);
			DetailsSwitcher->SetActiveWidgetIndex(1);
		}
		else
		{
			DetailsSwitcher->SetActiveWidgetIndex(0);
		}
		return;
	}

	// --- Regular trait: show UObject properties in TraitDetailsView ---
	if (SelectedItem->ItemType == EArcMassTraitListItemType::Trait)
	{
		UMassEntityTraitBase* Trait = SelectedItem->Trait.Get();
		if (!Trait)
		{
			DetailsSwitcher->SetActiveWidgetIndex(0);
			return;
		}

		TraitDetailsView->SetObject(Trait);
		TraitDetailsView->SetRootExpansionStates(true, true);

		// Respect parent read-only: disable editing if this is a parent trait
		bool bIsParent = SelectedItem->bIsParentTrait;
		TraitDetailsView->SetIsPropertyEditingEnabledDelegate(
			FIsPropertyEditingEnabled::CreateLambda([bIsParent]() -> bool
			{
				return !bIsParent;
			}));

		DetailsSwitcher->SetActiveWidgetIndex(1);
		return;
	}

	// --- Assorted fragment or tag: show struct properties in StructDetailsView ---
	if (SelectedItem->ItemType == EArcMassTraitListItemType::AssortedFragment
		|| SelectedItem->ItemType == EArcMassTraitListItemType::AssortedTag)
	{
		UMassEntityTraitBase* AssortedTrait = SelectedItem->OwningAssortedTrait.Get();
		if (!AssortedTrait || SelectedItem->AssortedIndex == INDEX_NONE)
		{
			DetailsSwitcher->SetActiveWidgetIndex(0);
			return;
		}

		// Access the appropriate array via FProperty reflection
		FName ArrayPropertyName = (SelectedItem->ItemType == EArcMassTraitListItemType::AssortedFragment)
			? TEXT("Fragments")
			: TEXT("Tags");

		FProperty* Prop = UMassAssortedFragmentsTrait::StaticClass()->FindPropertyByName(ArrayPropertyName);
		if (!Prop)
		{
			DetailsSwitcher->SetActiveWidgetIndex(0);
			return;
		}

		TArray<FInstancedStruct>* InstanceArray = Prop->ContainerPtrToValuePtr<TArray<FInstancedStruct>>(AssortedTrait);
		if (!InstanceArray || !InstanceArray->IsValidIndex(SelectedItem->AssortedIndex))
		{
			DetailsSwitcher->SetActiveWidgetIndex(0);
			return;
		}

		FInstancedStruct& InstancedStruct = (*InstanceArray)[SelectedItem->AssortedIndex];
		if (!InstancedStruct.IsValid())
		{
			DetailsSwitcher->SetActiveWidgetIndex(0);
			return;
		}

		TSharedPtr<FStructOnScope> StructData = MakeShared<FStructOnScope>(
			InstancedStruct.GetScriptStruct()
			, InstancedStruct.GetMutableMemory());

		StructDetailsView->SetStructureData(StructData);
		DetailsSwitcher->SetActiveWidgetIndex(2);
		return;
	}

	// Fallback: NullTrait or unknown
	DetailsSwitcher->SetActiveWidgetIndex(0);
}

// ---------- Assorted Fragments Flattened View ----------

void FArcAssetEditor_MassEntityConfig::ShowAssortedFragmentsFlattened(UMassEntityTraitBase* AssortedTrait, bool bShowFragments, bool bShowTags)
{
	// Clear previous state
	ActiveStructScopes.Empty();
	ActiveStructDetailViews.Empty();
	AssortedFragmentsScrollBox->ClearChildren();

	FPropertyEditorModule& PropModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Access Fragments array via reflection (protected member)
	FProperty* FragmentsProp = bShowFragments
		? UMassAssortedFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("Fragments"))
		: nullptr;
	if (FragmentsProp)
	{
		TArray<FInstancedStruct>* FragmentsArray = FragmentsProp->ContainerPtrToValuePtr<TArray<FInstancedStruct>>(AssortedTrait);
		if (FragmentsArray)
		{
			for (int32 Idx = 0; Idx < FragmentsArray->Num(); ++Idx)
			{
				FInstancedStruct& Fragment = (*FragmentsArray)[Idx];
				if (!Fragment.IsValid()) continue;

				const UScriptStruct* ScriptStruct = Fragment.GetScriptStruct();

				// Header label with struct name
				FString DisplayName = ScriptStruct->GetMetaData(TEXT("DisplayName"));
				if (DisplayName.IsEmpty())
				{
					DisplayName = ScriptStruct->GetName();
				}

				AssortedFragmentsScrollBox->AddSlot()
					.Padding(0.f, 2.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(DisplayName))
						.Font(FAppStyle::GetFontStyle("BoldFont"))
					];

				// Create struct detail view for this fragment
				TSharedPtr<FStructOnScope> StructScope = MakeShared<FStructOnScope>(
					ScriptStruct
					, Fragment.GetMutableMemory());
				ActiveStructScopes.Add(StructScope);

				FDetailsViewArgs FragArgs;
				FragArgs.bShowOptions = false;
				FragArgs.bAllowSearch = false;
				FragArgs.bShowObjectLabel = false;
				FragArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

				FStructureDetailsViewArgs StructViewArgs;
				TSharedPtr<IStructureDetailsView> FragDetailView = PropModule.CreateStructureDetailView(FragArgs, StructViewArgs, StructScope);
				ActiveStructDetailViews.Add(FragDetailView);

				AssortedFragmentsScrollBox->AddSlot()
					.Padding(8.f, 0.f, 0.f, 4.f)
					[
						FragDetailView->GetWidget().ToSharedRef()
					];
			}
		}
	}

	// Access Tags array — show as-is via the trait UObject details
	// Tags are typically empty structs (just type markers), so showing the array is fine
	FProperty* TagsProp = bShowTags
		? UMassAssortedFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("Tags"))
		: nullptr;
	if (TagsProp)
	{
		TArray<FInstancedStruct>* TagsArray = TagsProp->ContainerPtrToValuePtr<TArray<FInstancedStruct>>(AssortedTrait);
		if (TagsArray && TagsArray->Num() > 0)
		{
			AssortedFragmentsScrollBox->AddSlot()
				.Padding(0.f, 8.f, 0.f, 2.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TagsHeader", "Tags"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				];

			for (int32 Idx = 0; Idx < TagsArray->Num(); ++Idx)
			{
				FInstancedStruct& Tag = (*TagsArray)[Idx];
				if (!Tag.IsValid()) continue;

				const UScriptStruct* TagStruct = Tag.GetScriptStruct();
				FString TagName = TagStruct->GetMetaData(TEXT("DisplayName"));
				if (TagName.IsEmpty())
				{
					TagName = TagStruct->GetName();
				}

				AssortedFragmentsScrollBox->AddSlot()
					.Padding(8.f, 0.f, 0.f, 2.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TagName))
					];
			}
		}
	}

	DetailsSwitcher->SetActiveWidgetIndex(3);
}

// ---------- Config Property Changes ----------

void FArcAssetEditor_MassEntityConfig::OnConfigPropertyChanged(const FPropertyChangedEvent& Event)
{
	UMassEntityConfigAsset* Asset = GetEditingConfigAsset();
	if (!Asset) return;

	Asset->MarkPackageDirty();

	// Rebuild the trait list (parent may have changed)
	if (TraitListWidget.IsValid())
	{
		TraitListWidget->RebuildTree();
	}

	// Clear selection since tree structure may have changed
	DetailsSwitcher->SetActiveWidgetIndex(0);

	if (PreviewViewport.IsValid())
	{
		PreviewViewport->RebuildPreview();
	}
}

// ---------- Helpers ----------

UMassEntityConfigAsset* FArcAssetEditor_MassEntityConfig::GetEditingConfigAsset() const
{
	const TArray<UObject*>& Objects = GetEditingObjects();
	return Objects.Num() > 0 ? Cast<UMassEntityConfigAsset>(Objects[0]) : nullptr;
}

FText FArcAssetEditor_MassEntityConfig::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Mass Entity Config Editor");
}

FText FArcAssetEditor_MassEntityConfig::GetToolkitName() const
{
	if (UMassEntityConfigAsset* Asset = GetEditingConfigAsset())
	{
		return FText::FromString(Asset->GetName());
	}
	return GetBaseToolkitName();
}

FLinearColor FArcAssetEditor_MassEntityConfig::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.6f, 1.0f, 0.5f); // Blue tint
}

FString FArcAssetEditor_MassEntityConfig::GetWorldCentricTabPrefix() const
{
	return TEXT("MassConfig ");
}

#undef LOCTEXT_NAMESPACE
