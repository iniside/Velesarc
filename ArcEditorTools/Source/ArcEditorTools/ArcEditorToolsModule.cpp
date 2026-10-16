/**
 * This file is part of Velesarc
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

#include "ArcEditorToolsModule.h"

#include "EdGraphUtilities.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapGraphNodeFactory.h"
#include "TerrainTool/ArcTerrainBakeBridge.h"
#include "TracePresets/ArcTracePresetsStatusBar.h"

#include "SArcPropertyBinding.h"
#include "MassProcessorBrowser/SArcMassProcessorBrowser.h"
#include "FoliageSkinning/ArcFoliageToSkinnedMeshConverter.h"
#include "FoliageSkinning/ArcFoliageToSkinnedMeshSettings.h"
#include "FoliageSkinning/ArcFoliageToStaticMeshConverter.h"
#include "FoliageSkinning/ArcFoliageToStaticMeshSettings.h"

#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Engine/SkeletalMesh.h"
#include "Dialog/SCustomDialog.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Misc/ScopedSlowTask.h"
#include "Engine/StaticMesh.h"

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FArcEditorToolsModule"

namespace ArcEditorToolsModule_Private
{
	const FName FoliageSkinningToolMenuOwner(TEXT("ArcEditorTools.FoliageSkinning"));

	void RunFoliageConversion(TArray<TWeakObjectPtr<UStaticMesh>> StaticMeshes)
	{
		UArcFoliageToSkinnedMeshSettings* Settings = GetMutableDefault<UArcFoliageToSkinnedMeshSettings>();
		Settings->SaveConfig();

		FScopedSlowTask SlowTask(static_cast<float>(StaticMeshes.Num()),
			LOCTEXT("ConvertingToSkinnedFoliage", "Converting to Skinned Foliage..."));
		SlowTask.MakeDialog();

		TArray<UObject*> CreatedAssets;
		for (const TWeakObjectPtr<UStaticMesh>& StaticMeshPtr : StaticMeshes)
		{
			SlowTask.EnterProgressFrame(1.0f);
			UStaticMesh* StaticMesh = StaticMeshPtr.Get();
			if (StaticMesh == nullptr)
			{
				continue;
			}
			USkeletalMesh* Created = UArcFoliageToSkinnedMeshConverter::ConvertStaticMeshToSkinnedFoliage(StaticMesh, Settings);
			if (Created != nullptr)
			{
				CreatedAssets.Add(Created);
			}
		}

		// Reveal and select the new assets so the user sees exactly what was produced (and where).
		if (CreatedAssets.Num() > 0)
		{
			IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
			ContentBrowser.SyncBrowserToAssets(CreatedAssets);
		}
	}

	void ExecuteConvertToSkinnedFoliage(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		if (Context == nullptr)
		{
			return;
		}

		// Capture the selected meshes now — the menu context is transient and the dialog is non-modal.
		TArray<UStaticMesh*> LoadedMeshes = Context->LoadSelectedObjectsIf<UStaticMesh>(
			[](const FAssetData& AssetData)
			{
				return true;
			});
		if (LoadedMeshes.Num() == 0)
		{
			return;
		}

		TArray<TWeakObjectPtr<UStaticMesh>> StaticMeshes;
		StaticMeshes.Reserve(LoadedMeshes.Num());
		for (UStaticMesh* StaticMesh : LoadedMeshes)
		{
			StaticMeshes.Add(StaticMesh);
		}

		UArcFoliageToSkinnedMeshSettings* Settings = GetMutableDefault<UArcFoliageToSkinnedMeshSettings>();

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

		TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		DetailsView->SetObject(Settings);

		// Shared weak handle filled in after SNew so the Close button can destroy the window it lives in
		// (AutoCloseOnButtonPress is off to keep the dialog open across repeated Convert clicks).
		TSharedRef<TWeakPtr<SWindow>> WeakDialog = MakeShared<TWeakPtr<SWindow>>();

		SCustomDialog::FButton ConvertButton(LOCTEXT("Convert", "Convert"));
		ConvertButton.SetPrimary(true);
		ConvertButton.SetOnClicked(FSimpleDelegate::CreateLambda(
			[StaticMeshes]()
			{
				RunFoliageConversion(StaticMeshes);
			}));

		SCustomDialog::FButton CloseButton(LOCTEXT("Close", "Close"));
		CloseButton.SetOnClicked(FSimpleDelegate::CreateLambda(
			[WeakDialog]()
			{
				if (TSharedPtr<SWindow> Window = WeakDialog->Pin())
				{
					Window->RequestDestroyWindow();
				}
			}));

		TSharedRef<SCustomDialog> Dialog = SNew(SCustomDialog)
			.Title(LOCTEXT("ConvertToSkinnedFoliageTitle", "Convert to Skinned Foliage"))
			// A non-zero ClientSize flips SCustomDialog's SizingRule from Autosized to UserSized,
			// which is what makes the window resizable. Content alignment defaults to Left/Center
			// with a 300px scroll-box height cap, so Fill both axes (and lift the cap) to make the
			// details view track the window size; the scroll box still scrolls when shrunk.
			.ClientSize(FVector2D(480.0, 640.0))
			.HAlignContent(HAlign_Fill)
			.VAlignContent(VAlign_Fill)
			.ScrollBoxMaxHeight(4096.0f)
			.AutoCloseOnButtonPress(false)
			.WindowArguments(SWindow::FArguments().IsTopmostWindow(true))
			.Content()
			[
				DetailsView
			]
			.Buttons({
				ConvertButton,
				CloseButton
			});

		*WeakDialog = Dialog;

		// Show() (not ShowModal()) keeps the dialog non-modal so the Content Browser stays interactive;
		// the window is topmost and stays open across Convert clicks so settings can be tweaked between runs.
		Dialog->Show();
	}

	void RunStaticMeshConversion(TArray<TWeakObjectPtr<UStaticMesh>> StaticMeshes)
	{
		UArcFoliageToStaticMeshSettings* Settings = GetMutableDefault<UArcFoliageToStaticMeshSettings>();
		Settings->SaveConfig();

		FScopedSlowTask SlowTask(static_cast<float>(StaticMeshes.Num()),
			LOCTEXT("ConvertingToNaniteStaticMesh", "Converting to Nanite Static Mesh..."));
		SlowTask.MakeDialog();

		TArray<UObject*> CreatedAssets;
		for (const TWeakObjectPtr<UStaticMesh>& StaticMeshPtr : StaticMeshes)
		{
			SlowTask.EnterProgressFrame(1.0f);
			UStaticMesh* StaticMesh = StaticMeshPtr.Get();
			if (StaticMesh == nullptr)
			{
				continue;
			}
			UStaticMesh* Created = UArcFoliageToStaticMeshConverter::ConvertStaticMeshToNaniteStaticMesh(StaticMesh, Settings);
			if (Created != nullptr)
			{
				CreatedAssets.Add(Created);
			}
		}

		// Reveal and select the new assets so the user sees exactly what was produced (and where).
		if (CreatedAssets.Num() > 0)
		{
			IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
			ContentBrowser.SyncBrowserToAssets(CreatedAssets);
		}
	}

	void ExecuteConvertToNaniteStaticMesh(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		if (Context == nullptr)
		{
			return;
		}

		// Capture the selected meshes now — the menu context is transient and the dialog is non-modal.
		TArray<UStaticMesh*> LoadedMeshes = Context->LoadSelectedObjectsIf<UStaticMesh>(
			[](const FAssetData& AssetData)
			{
				return true;
			});
		if (LoadedMeshes.Num() == 0)
		{
			return;
		}

		TArray<TWeakObjectPtr<UStaticMesh>> StaticMeshes;
		StaticMeshes.Reserve(LoadedMeshes.Num());
		for (UStaticMesh* StaticMesh : LoadedMeshes)
		{
			StaticMeshes.Add(StaticMesh);
		}

		UArcFoliageToStaticMeshSettings* Settings = GetMutableDefault<UArcFoliageToStaticMeshSettings>();

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

		TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		DetailsView->SetObject(Settings);

		// Shared weak handle filled in after SNew so the Close button can destroy the window it lives in
		// (AutoCloseOnButtonPress is off to keep the dialog open across repeated Convert clicks).
		TSharedRef<TWeakPtr<SWindow>> WeakDialog = MakeShared<TWeakPtr<SWindow>>();

		SCustomDialog::FButton ConvertButton(LOCTEXT("Convert", "Convert"));
		ConvertButton.SetPrimary(true);
		ConvertButton.SetOnClicked(FSimpleDelegate::CreateLambda(
			[StaticMeshes]()
			{
				RunStaticMeshConversion(StaticMeshes);
			}));

		SCustomDialog::FButton CloseButton(LOCTEXT("Close", "Close"));
		CloseButton.SetOnClicked(FSimpleDelegate::CreateLambda(
			[WeakDialog]()
			{
				if (TSharedPtr<SWindow> Window = WeakDialog->Pin())
				{
					Window->RequestDestroyWindow();
				}
			}));

		TSharedRef<SCustomDialog> Dialog = SNew(SCustomDialog)
			.Title(LOCTEXT("ConvertToNaniteStaticMeshTitle", "Convert to Nanite Static Mesh"))
			// A non-zero ClientSize flips SCustomDialog's SizingRule from Autosized to UserSized,
			// which is what makes the window resizable. Content alignment defaults to Left/Center
			// with a 300px scroll-box height cap, so Fill both axes (and lift the cap) to make the
			// details view track the window size; the scroll box still scrolls when shrunk.
			.ClientSize(FVector2D(480.0, 640.0))
			.HAlignContent(HAlign_Fill)
			.VAlignContent(VAlign_Fill)
			.ScrollBoxMaxHeight(4096.0f)
			.AutoCloseOnButtonPress(false)
			.WindowArguments(SWindow::FArguments().IsTopmostWindow(true))
			.Content()
			[
				DetailsView
			]
			.Buttons({
				ConvertButton,
				CloseButton
			});

		*WeakDialog = Dialog;

		// Show() (not ShowModal()) keeps the dialog non-modal so the Content Browser stays interactive;
		// the window is topmost and stays open across Convert clicks so settings can be tweaked between runs.
		Dialog->Show();
	}
}

const FName FArcEditorToolsModule::MassProcessorBrowserTabId = FName(TEXT("ArcMassProcessorBrowser"));

IArcEditorTools& IArcEditorTools::Get()
{
	return FModuleManager::LoadModuleChecked<FArcEditorToolsModule>(TEXT("ArcEditorTools"));
}

void FArcEditorToolsModule::StartupModule()
{
	if (!StyleSet.IsValid())
	{
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);

		const FString IconDir = IPluginManager::Get().FindPlugin(TEXT("ArcEditorTools"))->GetBaseDir()
			+ TEXT("/Content/Editor/AssetIcons");

		StyleSet = MakeShared<FSlateStyleSet>(TEXT("ArcEditorTools"));

		StyleSet->Set("ClassThumbnail.ArcMassProcessorBrowserConfig",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcMassProcessorBrowserConfig.svg"), Icon64x64));
		StyleSet->Set("ClassIcon.ArcMassProcessorBrowserConfig",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcMassProcessorBrowserConfig.svg"), Icon16x16));

		StyleSet->Set("ClassThumbnail.ArcSettlementNeedDataAsset",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcSettlementNeedDataAsset.svg"), Icon64x64));
		StyleSet->Set("ClassIcon.ArcSettlementNeedDataAsset",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcSettlementNeedDataAsset.svg"), Icon16x16));

		StyleSet->Set("ClassThumbnail.ArcAreaPopulationDefinition",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcAreaPopulationDefinition.svg"), Icon64x64));
		StyleSet->Set("ClassIcon.ArcAreaPopulationDefinition",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcAreaPopulationDefinition.svg"), Icon16x16));

		StyleSet->Set("ClassThumbnail.ArcKnowledgeEntryDefinition",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcKnowledgeEntryDefinition.svg"), Icon64x64));
		StyleSet->Set("ClassIcon.ArcKnowledgeEntryDefinition",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcKnowledgeEntryDefinition.svg"), Icon16x16));

		StyleSet->Set("ClassThumbnail.ArcKnowledgeQueryDefinition",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcKnowledgeQueryDefinition.svg"), Icon64x64));
		StyleSet->Set("ClassIcon.ArcKnowledgeQueryDefinition",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcKnowledgeQueryDefinition.svg"), Icon16x16));

		StyleSet->Set("ClassThumbnail.ArcTQSQueryDefinition",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcTQSQueryDefinition.svg"), Icon64x64));
		StyleSet->Set("ClassIcon.ArcTQSQueryDefinition",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcTQSQueryDefinition.svg"), Icon16x16));

		StyleSet->Set("ClassThumbnail.ArcAreaDefinition",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcAreaDefinition.svg"), Icon64x64));
		StyleSet->Set("ClassIcon.ArcAreaDefinition",
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcAreaDefinition.svg"), Icon16x16));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
	}

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MassProcessorBrowserTabId,
		FOnSpawnTab::CreateRaw(this, &FArcEditorToolsModule::SpawnMassProcessorBrowserTab))
		.SetDisplayName(LOCTEXT("MassProcessorBrowserTitle", "Mass Processor Browser"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetEditOptions());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		UToolMenu* EditMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Edit");
		FToolMenuSection& Section = EditMenu->FindOrAddSection("Configuration");
		Section.Label = LOCTEXT("ConfigurationSection", "Configuration");

		Section.AddMenuEntry(
			"MassProcessorBrowser",
			LOCTEXT("MassProcessorBrowserMenuEntry", "Mass Processor Browser"),
			LOCTEXT("MassProcessorBrowserMenuTooltip", "Browse and configure all Mass processors"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("ArcMassProcessorBrowser")));
			}))
		);
	}));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(ArcEditorToolsModule_Private::FoliageSkinningToolMenuOwner);

		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.StaticMesh.AssetActionsSubMenu");
		FToolMenuSection& Section = Menu->FindOrAddSection("AssetContextAdvancedActions");

		Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
			if (Context == nullptr)
			{
				return;
			}

			const TArray<FAssetData> SelectedStaticMeshes = Context->GetSelectedAssetsOfType(UStaticMesh::StaticClass());
			if (SelectedStaticMeshes.IsEmpty())
			{
				return;
			}

			const TAttribute<FText> Label = LOCTEXT("ConvertToSkinnedFoliage", "Convert to Skinned Foliage...");
			const TAttribute<FText> ToolTip = LOCTEXT("ConvertToSkinnedFoliageTooltip",
				"Cut this foliage card's geometry to its opacity mask and rebuild it as a skinned "
				"skeletal mesh with auto-placed clump bones.");
			const FSlateIcon Icon = FSlateIcon();

			FToolUIAction UIAction;
			UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(
				&ArcEditorToolsModule_Private::ExecuteConvertToSkinnedFoliage);
			InSection.AddMenuEntry("ArcConvertToSkinnedFoliage", Label, ToolTip, Icon, UIAction);

			const TAttribute<FText> StaticMeshLabel = LOCTEXT("ConvertToNaniteStaticMesh", "Convert to Nanite Static Mesh...");
			const TAttribute<FText> StaticMeshToolTip = LOCTEXT("ConvertToNaniteStaticMeshTooltip",
				"Cut this foliage card's geometry to its opacity mask and rebuild it as a Nanite static "
				"mesh (opaque geometry, no skeleton).");
			const FSlateIcon StaticMeshIcon = FSlateIcon();

			FToolUIAction StaticMeshUIAction;
			StaticMeshUIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(
				&ArcEditorToolsModule_Private::ExecuteConvertToNaniteStaticMesh);
			InSection.AddMenuEntry("ArcConvertToNaniteStaticMesh", StaticMeshLabel, StaticMeshToolTip, StaticMeshIcon, StaticMeshUIAction);
		}));
	}));

	HeightmapNodeFactory = MakeShareable(new FArcHeightmapGraphNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(HeightmapNodeFactory);

	// Service UArcTerrainBakedStamp::Bake() requests (one-way dep: ArcEditorTools -> ArcTerrain).
	ArcTerrainBakeBridge::Register();

	ArcTracePresetsStatusBar::Register();
}

void FArcEditorToolsModule::ShutdownModule()
{
	UToolMenus::UnregisterOwner(ArcEditorToolsModule_Private::FoliageSkinningToolMenuOwner);

	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		StyleSet.Reset();
	}

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MassProcessorBrowserTabId);

	if (HeightmapNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(HeightmapNodeFactory);
		HeightmapNodeFactory.Reset();
	}

	ArcTerrainBakeBridge::Unregister();

	ArcTracePresetsStatusBar::Unregister();
}

TSharedRef<SDockTab> FArcEditorToolsModule::SpawnMassProcessorBrowserTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(NomadTab)
		[
			SNew(SArcMassProcessorBrowser)
		];
}

TSharedRef<SWidget> FArcEditorToolsModule::MakePropertyBindingWidget(UBlueprint* InBlueprint
	, const FArcPropertyBindingWidgetArgs& InArgs) const
{
	TArray<FBindingContextStruct> BindingContextStructs;
	return SNew(SArcPropertyBinding, InBlueprint, BindingContextStructs)
		.Args(InArgs);
}

TSharedRef<SWidget> FArcEditorToolsModule::MakePropertyBindingWidget(const TArray<FBindingContextStruct>& InBindingContextStructs
	, const FArcPropertyBindingWidgetArgs& InArgs) const
{
	return SNew(SArcPropertyBinding, nullptr, InBindingContextStructs)
	.Args(InArgs);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FArcEditorToolsModule, ArcEditorTools)