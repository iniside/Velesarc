// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAssetEditor_MassEntityConfig.h"

#include "SArcMassTraitList.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "MassAssortedFragmentsTrait.h"
#include "ArcMass/Config/ArcMassAssortedSparseFragmentsTrait.h"
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
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "Config/ArcMassEntityConfigHelpers.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SHyperlink.h"
#include "Subsystems/AssetEditorSubsystem.h"

namespace ArcAssetEditor_MassEntityConfigPrivate
{
	// Resolves the Fragments/Tags reflection properties on whichever assorted trait class
	// (engine UMassAssortedFragmentsTrait or project UArcMassAssortedSparseFragmentsTrait)
	// the given trait actually is. Either output may be null if the trait is not an
	// assorted variant we recognise.
	static void ResolveAssortedProperties(
		UMassEntityTraitBase* Trait,
		FProperty*& OutFragmentsProp,
		FProperty*& OutTagsProp)
	{
		OutFragmentsProp = nullptr;
		OutTagsProp = nullptr;
		if (!Trait)
		{
			return;
		}
		if (Trait->IsA<UArcMassAssortedSparseFragmentsTrait>())
		{
			OutFragmentsProp = UArcMassAssortedSparseFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("SparseFragments"));
			OutTagsProp = UArcMassAssortedSparseFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("SparseTags"));
		}
		else if (Trait->IsA<UMassAssortedFragmentsTrait>())
		{
			OutFragmentsProp = UMassAssortedFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("Fragments"));
			OutTagsProp = UMassAssortedFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("Tags"));
		}
	}
}

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
			this, &FArcAssetEditor_MassEntityConfig::OnTraitSelected))
		.OnTraitsModified(FArcOnTraitsModified::CreateSP(
			this, &FArcAssetEditor_MassEntityConfig::OnTraitsModified));

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

			// Template section — only for UArcMassEntityConfigAsset variants (not templates themselves)
			UMassEntityConfigAsset* Asset = GetEditingConfigAsset();
			UArcMassEntityConfigAsset* ArcAsset = Cast<UArcMassEntityConfigAsset>(Asset);
			if (Asset && !Asset->IsA<UArcMassEntityConfigTemplate>() && ArcAsset != nullptr)
			{
				ToolbarBuilder.BeginSection("Template");
				{
					ToolbarBuilder.AddWidget(
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.f, 0.f, 2.f, 0.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TemplateToolbarLabel", "Template:"))
							.Font(FAppStyle::GetFontStyle("BoldFont"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SHyperlink)
							.Text(this, &FArcAssetEditor_MassEntityConfig::GetTemplateNameText)
							.OnNavigate_Lambda([this]()
							{
								UArcMassEntityConfigAsset* NavAsset = GetEditingArcConfigAsset();
								if (NavAsset)
								{
									UArcMassEntityConfigTemplate* Template = NavAsset->ResolveSourceTemplate();
									if (Template)
									{
										GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Template);
									}
								}
							})
						]
					);

					ToolbarBuilder.AddToolBarButton(
						FUIAction(
							FExecuteAction::CreateSP(this, &FArcAssetEditor_MassEntityConfig::OnAddTemplateClicked)
						)
						, NAME_None
						, LOCTEXT("AddTemplateBtn", "Add Template")
						, LOCTEXT("AddTemplateBtnTip", "Additively stamp template traits onto this config (keeps existing traits; never removes)")
						, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import")
					);

					ToolbarBuilder.AddToolBarButton(
						FUIAction(
							FExecuteAction::CreateSP(this, &FArcAssetEditor_MassEntityConfig::OnReplaceTemplateClicked)
						)
						, NAME_None
						, LOCTEXT("ReplaceTemplateBtn", "Replace Template")
						, LOCTEXT("ReplaceTemplateBtnTip", "Replace template-origin traits with the selected template; preserve variant-only traits")
						, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Convert")
					);

					ToolbarBuilder.AddToolBarButton(
						FUIAction(
							FExecuteAction::CreateSP(this, &FArcAssetEditor_MassEntityConfig::OnUpdateFromTemplateClicked)
							, FCanExecuteAction::CreateSP(this, &FArcAssetEditor_MassEntityConfig::HasSourceTemplate)
						)
						, NAME_None
						, LOCTEXT("UpdateFromTemplateBtn", "Update")
						, LOCTEXT("UpdateFromTemplateBtnTip", "Update traits from the source template")
						, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh")
					);

					ToolbarBuilder.AddToolBarButton(
						FUIAction(
							FExecuteAction::CreateSP(this, &FArcAssetEditor_MassEntityConfig::OnPushToTemplateClicked)
							, FCanExecuteAction::CreateSP(this, &FArcAssetEditor_MassEntityConfig::HasSourceTemplate)
						)
						, NAME_None
						, LOCTEXT("PushToTemplateBtn", "Push to Template")
						, LOCTEXT("PushToTemplateBtnTip", "Overwrite the source template with this config's traits")
						, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Export")
					);

					ToolbarBuilder.AddToolBarButton(
						FUIAction(
							FExecuteAction::CreateSP(this, &FArcAssetEditor_MassEntityConfig::OnCreateTemplateFromVariantClicked)
						)
						, NAME_None
						, LOCTEXT("CreateTemplateBtn", "Create Template")
						, LOCTEXT("CreateTemplateBtnTip", "Create a new template from this config and link it as the source template")
						, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.PlusCircle")
					);
				}
				ToolbarBuilder.EndSection();
			}
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

		// Assorted Sparse Fragments sub-category: show only sparse fragments
		if (SelectedItem->bIsAssortedSparseFragmentSubCategory && SelectedItem->Trait.IsValid())
		{
			ShowAssortedFragmentsFlattened(SelectedItem->Trait.Get(), /*bShowFragments=*/true, /*bShowTags=*/false);
			return;
		}

		// Assorted Sparse Tags sub-category: show only sparse tags
		if (SelectedItem->bIsAssortedSparseTagSubCategory && SelectedItem->Trait.IsValid())
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

	// --- Assorted fragment or tag (regular or sparse): show struct properties in StructDetailsView ---
	if (SelectedItem->ItemType == EArcMassTraitListItemType::AssortedFragment
		|| SelectedItem->ItemType == EArcMassTraitListItemType::AssortedTag
		|| SelectedItem->ItemType == EArcMassTraitListItemType::AssortedSparseFragment
		|| SelectedItem->ItemType == EArcMassTraitListItemType::AssortedSparseTag)
	{
		UMassEntityTraitBase* AssortedTrait = SelectedItem->OwningAssortedTrait.Get();
		if (!AssortedTrait || SelectedItem->AssortedIndex == INDEX_NONE)
		{
			DetailsSwitcher->SetActiveWidgetIndex(0);
			return;
		}

		// Resolve the appropriate array property via the helper (handles both engine and sparse variants)
		FProperty* FragmentsProp = nullptr;
		FProperty* TagsProp = nullptr;
		ArcAssetEditor_MassEntityConfigPrivate::ResolveAssortedProperties(AssortedTrait, FragmentsProp, TagsProp);

		const bool bIsFragment = SelectedItem->ItemType == EArcMassTraitListItemType::AssortedFragment
			|| SelectedItem->ItemType == EArcMassTraitListItemType::AssortedSparseFragment;
		FProperty* Prop = bIsFragment ? FragmentsProp : TagsProp;
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

	// Resolve the correct property names for whichever assorted trait variant this is
	FProperty* FragmentsPropResolved = nullptr;
	FProperty* TagsPropResolved = nullptr;
	ArcAssetEditor_MassEntityConfigPrivate::ResolveAssortedProperties(AssortedTrait, FragmentsPropResolved, TagsPropResolved);

	// Access Fragments array via reflection (protected member)
	FProperty* FragmentsProp = bShowFragments ? FragmentsPropResolved : nullptr;
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
	FProperty* TagsProp = bShowTags ? TagsPropResolved : nullptr;
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

void FArcAssetEditor_MassEntityConfig::OnTraitsModified()
{
	// Trait list changed (add/remove trait or assorted fragment/tag). RebuildPreview is a
	// full idempotent clear-and-respawn, so this clears stale meshes from removed traits.
	if (PreviewViewport.IsValid())
	{
		PreviewViewport->RebuildPreview();
	}
}

// ---------- Template Operations ----------

UArcMassEntityConfigAsset* FArcAssetEditor_MassEntityConfig::GetEditingArcConfigAsset() const
{
	return Cast<UArcMassEntityConfigAsset>(GetEditingConfigAsset());
}

bool FArcAssetEditor_MassEntityConfig::HasSourceTemplate() const
{
	UArcMassEntityConfigAsset* ArcAsset = GetEditingArcConfigAsset();
	return ArcAsset && ArcAsset->SourceTemplateId.IsValid();
}

FText FArcAssetEditor_MassEntityConfig::GetTemplateNameText() const
{
	UArcMassEntityConfigAsset* ArcAsset = GetEditingArcConfigAsset();
	if (ArcAsset)
	{
		UArcMassEntityConfigTemplate* Template = ArcAsset->ResolveSourceTemplate();
		if (Template)
		{
			return FText::FromName(Template->GetFName());
		}
	}
	return LOCTEXT("None", "None");
}

void FArcAssetEditor_MassEntityConfig::OnAddTemplateClicked()
{
	UArcMassEntityConfigAsset* ArcAsset = GetEditingArcConfigAsset();
	if (!ArcAsset) return;

	TArray<UArcMassEntityConfigAsset*> Variants;
	Variants.Add(ArcAsset);

	UArcMassEntityConfigTemplate* SelectedTemplate = nullptr;
	const bool bPressedOk = FArcMassEntityConfigHelpers::PickTemplateWithPreview(
		SelectedTemplate, Variants, EArcMassConfigMergeMode::Set);

	if (bPressedOk && SelectedTemplate)
	{
		FScopedTransaction Tx(LOCTEXT("AddTemplateTx", "Add Source Template"));
		ArcAsset->Modify();
		SelectedTemplate->SetTemplate(ArcAsset);
		ArcAsset->MarkPackageDirty();
		RefreshAfterTemplateChange();
	}
}

void FArcAssetEditor_MassEntityConfig::OnReplaceTemplateClicked()
{
	UArcMassEntityConfigAsset* ArcAsset = GetEditingArcConfigAsset();
	if (!ArcAsset) return;

	TArray<UArcMassEntityConfigAsset*> Variants;
	Variants.Add(ArcAsset);

	UArcMassEntityConfigTemplate* SelectedTemplate = nullptr;
	const bool bPressedOk = FArcMassEntityConfigHelpers::PickTemplateWithPreview(
		SelectedTemplate, Variants, EArcMassConfigMergeMode::SetNewOrReplace);

	if (bPressedOk && SelectedTemplate)
	{
		FScopedTransaction Tx(LOCTEXT("ReplaceTemplateTx", "Replace Source Template"));
		ArcAsset->Modify();
		SelectedTemplate->SetNewOrReplaceTemplate(ArcAsset);
		ArcAsset->MarkPackageDirty();
		RefreshAfterTemplateChange();
	}
}

void FArcAssetEditor_MassEntityConfig::OnUpdateFromTemplateClicked()
{
	UArcMassEntityConfigAsset* ArcAsset = GetEditingArcConfigAsset();
	if (!ArcAsset) return;

	UArcMassEntityConfigTemplate* Template = ArcAsset->ResolveSourceTemplate();
	if (!Template) return;

	TArray<UArcMassEntityConfigAsset*> Variants;
	Variants.Add(ArcAsset);

	const bool bPressedOk = FArcMassEntityConfigHelpers::PreviewUpdate(Template, Variants);

	if (bPressedOk)
	{
		FScopedTransaction Tx(LOCTEXT("UpdateFromTemplateTx", "Update from Template"));
		ArcAsset->Modify();
		Template->UpdateFromTemplate(ArcAsset);
		ArcAsset->MarkPackageDirty();
		RefreshAfterTemplateChange();
	}
}

void FArcAssetEditor_MassEntityConfig::OnPushToTemplateClicked()
{
	UArcMassEntityConfigAsset* ArcAsset = GetEditingArcConfigAsset();
	if (!ArcAsset) return;

	UArcMassEntityConfigTemplate* Template = ArcAsset->ResolveSourceTemplate();
	if (!Template) return;

	const FText Message = LOCTEXT("PushToTemplateConfirm",
		"Pushing to template overwrites it and flattens this variant's override tracking. Continue?");

	if (FMessageDialog::Open(EAppMsgType::OkCancel, Message) != EAppReturnType::Ok)
	{
		return;
	}

	FScopedTransaction Tx(LOCTEXT("PushToTemplateTx", "Push to Template"));
	Template->Modify();
	ArcAsset->Modify();
	Template->PushFromVariant(ArcAsset);
	Template->MarkPackageDirty();
	ArcAsset->MarkPackageDirty();
}

void FArcAssetEditor_MassEntityConfig::OnCreateTemplateFromVariantClicked()
{
	UArcMassEntityConfigAsset* ArcAsset = GetEditingArcConfigAsset();
	if (!ArcAsset) return;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	const FString BasePath = FPaths::GetPath(ArcAsset->GetPackage()->GetName());
	const FString BaseName = ArcAsset->GetName() + TEXT("_Template");
	FString OutPackageName;
	FString OutAssetName;
	AssetTools.CreateUniqueAssetName(BasePath + TEXT("/") + BaseName, TEXT(""), OutPackageName, OutAssetName);

	UObject* NewObj = AssetTools.CreateAsset(
		OutAssetName,
		FPaths::GetPath(OutPackageName),
		UArcMassEntityConfigTemplate::StaticClass(),
		/*Factory=*/nullptr);

	UArcMassEntityConfigTemplate* NewTemplate = Cast<UArcMassEntityConfigTemplate>(NewObj);
	if (!NewTemplate) return;

	FScopedTransaction Tx(LOCTEXT("CreateTemplateTx", "Create Template from Variant"));
	NewTemplate->Modify();
	ArcAsset->Modify();
	NewTemplate->PushFromVariant(ArcAsset);
	ArcAsset->SourceTemplateId = NewTemplate->EnsurePrimaryAssetId();
	NewTemplate->MarkPackageDirty();
	ArcAsset->MarkPackageDirty();
	RefreshAfterTemplateChange();
}

void FArcAssetEditor_MassEntityConfig::RefreshAfterTemplateChange()
{
	if (TraitListWidget.IsValid())
	{
		TraitListWidget->RebuildTree();
	}

	if (ConfigDetailsView.IsValid())
	{
		ConfigDetailsView->ForceRefresh();
	}

	if (DetailsSwitcher.IsValid())
	{
		DetailsSwitcher->SetActiveWidgetIndex(0);
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
