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

#include "ArcAssetEditor_ItemDefinition.h"
#include "ArcCoreEditor/ArcCoreEditorModule.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "IDetailsView.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Docking/SDockTab.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"

#include "Factories/BlueprintFactory.h"

#include "ArcCore/Items/ArcItemTypes.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Subsystems/ImportSubsystem.h"

const FName FArcAssetEditor_ItemDefinition::ToolkitFName(TEXT("FNxEAssetEditor_ItemData_2"));

TSharedRef<FArcAssetEditor_ItemDefinition> FArcAssetEditor_ItemDefinition::CreateItemEditor(const EToolkitMode::Type Mode
																				, const TSharedPtr<IToolkitHost>&
																				InitToolkitHost
																				, UObject* ObjectToEdit
																				, FGetDetailsViewObjects
																				GetDetailsViewObjects)
{
	TSharedRef<FArcAssetEditor_ItemDefinition> NewEditor(new FArcAssetEditor_ItemDefinition());

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(ObjectToEdit);
	NewEditor->InitEditor(Mode
		, InitToolkitHost
		, ObjectsToEdit
		, GetDetailsViewObjects);

	return NewEditor;
}

TSharedRef<FArcAssetEditor_ItemDefinition> FArcAssetEditor_ItemDefinition::CreateItemEditor(const EToolkitMode::Type Mode
																				, const TSharedPtr<IToolkitHost>&
																				InitToolkitHost
																				, const TArray<UObject*>& ObjectsToEdit
																				, FGetDetailsViewObjects
																				GetDetailsViewObjects)
{
	TSharedRef<FArcAssetEditor_ItemDefinition> NewEditor(new FArcAssetEditor_ItemDefinition());
	NewEditor->InitEditor(Mode
		, InitToolkitHost
		, ObjectsToEdit
		, GetDetailsViewObjects);
	return NewEditor;
}
