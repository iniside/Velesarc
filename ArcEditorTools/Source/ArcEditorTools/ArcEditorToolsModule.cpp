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

#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FArcEditorToolsModule"

const FName FArcEditorToolsModule::MinimapTabId = FName(TEXT("ArcIWMinimap"));

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
}

void FArcEditorToolsModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MinimapTabId);
}

TSharedRef<SDockTab> FArcEditorToolsModule::SpawnMinimapTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(NomadTab)
		[
			SNew(SArcIWMinimapTab)
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