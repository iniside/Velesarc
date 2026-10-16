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

#include "ArcCraftEditorModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Editing/ArcJsonRecordEditing.h"

#include "Customizations/ArcRecipeModifierSource_RandomPoolCustomization.h"
#include "Customizations/ArcRecipeModifierSource_MaterialPropertiesCustomization.h"
#include "Customizations/ArcRecipeModifierSource_StaticCustomization.h"
#include "Customizations/ArcMaterialPropertyRuleCustomization.h"
#include "Customizations/ArcRandomPoolEntryCustomization.h"
#include "MaterialProperty/FArcAssetEditor_MaterialPropertyTable.h"
#include "RandomPool/FArcAssetEditor_RandomPoolDefinition.h"
#include "Static/FArcAssetEditor_StaticModifierSet.h"

#include "RecipeGraph/ArcRecipeGraphNodeFactory.h"
#include "RecipeGraph/SArcRecipeGraphView.h"

#include "EdGraphUtilities.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FArcCraftEditorModule"

static const FName ArcRecipeGraphTabId(TEXT("ArcRecipeGraph"));

namespace ArcCraftEditorModulePrivate
{
	const TCHAR* MaterialTableExtension = TEXT("arcmaterialtable");
	const TCHAR* RandomPoolExtension = TEXT("arcrandompool");
	const TCHAR* StaticModsExtension = TEXT("arcstaticmods");
}

void FArcCraftEditorModule::StartupModule()
{
	if (IsRunningGame())
	{
		return;
	}

	FModuleManager::Get().LoadModuleChecked(TEXT("ArcJsonAssetsEditor"));
	ArcJsonRecordEditing::RegisterRecordEditor(ArcCraftEditorModulePrivate::MaterialTableExtension,
		FArcJsonRecordEditorOpen::CreateStatic(&FArcAssetEditor_MaterialPropertyTable::OpenRecord));
	ArcJsonRecordEditing::RegisterRecordEditor(ArcCraftEditorModulePrivate::RandomPoolExtension,
		FArcJsonRecordEditorOpen::CreateStatic(&FArcAssetEditor_RandomPoolDefinition::OpenRecord));
	ArcJsonRecordEditing::RegisterRecordEditor(ArcCraftEditorModulePrivate::StaticModsExtension,
		FArcJsonRecordEditorOpen::CreateStatic(&FArcAssetEditor_StaticModifierSet::OpenRecord));

	// --- Property type customizations ---
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// UHT strips the leading 'F' from struct names for the registered FName.
	{
		const FName StructName(TEXT("ArcRecipeModifierSource_RandomPool"));
		PropertyModule.RegisterCustomPropertyTypeLayout(StructName,
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(
				&FArcRecipeModifierSource_RandomPoolCustomization::MakeInstance));
		RegisteredPropertyTypeLayouts.Add(StructName);
	}
	{
		const FName StructName(TEXT("ArcRecipeModifierSource_MaterialProperties"));
		PropertyModule.RegisterCustomPropertyTypeLayout(StructName,
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(
				&FArcRecipeModifierSource_MaterialPropertiesCustomization::MakeInstance));
		RegisteredPropertyTypeLayouts.Add(StructName);
	}
	{
		const FName StructName(TEXT("ArcRecipeModifierSource_Static"));
		PropertyModule.RegisterCustomPropertyTypeLayout(StructName,
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(
				&FArcRecipeModifierSource_StaticCustomization::MakeInstance));
		RegisteredPropertyTypeLayouts.Add(StructName);
	}
	{
		const FName StructName(TEXT("ArcMaterialPropertyRule"));
		PropertyModule.RegisterCustomPropertyTypeLayout(StructName,
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(
				&FArcMaterialPropertyRuleCustomization::MakeInstance));
		RegisteredPropertyTypeLayouts.Add(StructName);
	}
	{
		const FName StructName(TEXT("ArcRandomPoolEntry"));
		PropertyModule.RegisterCustomPropertyTypeLayout(StructName,
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(
				&FArcRandomPoolEntryCustomization::MakeInstance));
		RegisteredPropertyTypeLayouts.Add(StructName);
	}

	// --- Cell factory ---
	CellFactory.RegisterBuiltins();

	// --- Recipe dependency graph tab + visual node factory ---
	RecipeGraphNodeFactory = MakeShared<FArcRecipeGraphNodeFactory>();
	FEdGraphUtilities::RegisterVisualNodeFactory(RecipeGraphNodeFactory);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ArcRecipeGraphTabId,
		FOnSpawnTab::CreateRaw(this, &FArcCraftEditorModule::SpawnRecipeGraphTab))
		.SetDisplayName(LOCTEXT("RecipeGraphTitle", "Recipe Dependency Graph"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		// Attach to the "Arc Game" main-menu pulldown registered by ArcCoreEditor.
		UToolMenu* ArcGameMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.ArcGame");
		FToolMenuSection& Section = ArcGameMenu->FindOrAddSection("ArcCraft");
		Section.Label = LOCTEXT("ArcCraftSection", "ArcCraft");

		Section.AddMenuEntry(
			"ArcRecipeGraph",
			LOCTEXT("RecipeGraphMenuEntry", "Recipe Dependency Graph"),
			LOCTEXT("RecipeGraphMenuTooltip", "Open the ArcCraft recipe dependency graph."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(ArcRecipeGraphTabId);
			}))
		);
	}));
}

void FArcCraftEditorModule::ShutdownModule()
{
	ArcJsonRecordEditing::UnregisterRecordEditor(ArcCraftEditorModulePrivate::MaterialTableExtension);
	ArcJsonRecordEditing::UnregisterRecordEditor(ArcCraftEditorModulePrivate::RandomPoolExtension);
	ArcJsonRecordEditing::UnregisterRecordEditor(ArcCraftEditorModulePrivate::StaticModsExtension);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ArcRecipeGraphTabId);

	if (RecipeGraphNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(RecipeGraphNodeFactory);
		RecipeGraphNodeFactory.Reset();
	}

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		for (const FName& LayoutName : RegisteredPropertyTypeLayouts)
		{
			PropertyModule.UnregisterCustomPropertyTypeLayout(LayoutName);
		}
		RegisteredPropertyTypeLayouts.Empty();
	}
}

TSharedRef<SDockTab> FArcCraftEditorModule::SpawnRecipeGraphTab(const FSpawnTabArgs& /*Args*/)
{
	return SNew(SDockTab)
		.TabRole(NomadTab)
		[
			SNew(SArcRecipeGraphView)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcCraftEditorModule, ArcCraftEditor)
