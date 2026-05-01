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

#include "SArcPropertyBinding.h"
#include "SArcIWMinimapTab.h"
#include "MassProcessorBrowser/SArcMassProcessorBrowser.h"

#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FArcEditorToolsModule"

const FName FArcEditorToolsModule::MinimapTabId = FName(TEXT("ArcIWMinimap"));
const FName FArcEditorToolsModule::MassProcessorBrowserTabId = FName(TEXT("ArcMassProcessorBrowser"));

IArcEditorTools& IArcEditorTools::Get()
{
	return FModuleManager::LoadModuleChecked<FArcEditorToolsModule>(TEXT("ArcEditorTools"));
}

void FArcEditorToolsModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MinimapTabId,
		FOnSpawnTab::CreateRaw(this, &FArcEditorToolsModule::SpawnMinimapTab))
		.SetDisplayName(LOCTEXT("ArcIWMinimapTabTitle", "ArcIW Minimap"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

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
}

void FArcEditorToolsModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MinimapTabId);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MassProcessorBrowserTabId);
}

TSharedRef<SDockTab> FArcEditorToolsModule::SpawnMinimapTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(NomadTab)
		[
			SNew(SArcIWMinimapTab)
		];
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