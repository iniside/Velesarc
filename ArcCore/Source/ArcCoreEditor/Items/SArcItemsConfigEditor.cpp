/**
 * This file is part of ArcX.
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

#include "SArcItemsConfigEditor.h"
#include "Dialogs/Dialogs.h"
#include "Dialogs/DlgPickPath.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#include "Blueprint/BlueprintSupport.h"

#include "EditorAssetLibrary.h"
#include "Widgets/Input/SFilePathPicker.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

#include "ArcCore/Items/ArcItemDefinition.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Engine/AssetManager.h"
#include "GameplayTagsManager.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SSettingsEditorCheckoutNotice.h"

#define LOCTEXT_NAMESPACE ""

FArcItemEditorCommands::FArcItemEditorCommands()
	: TCommands<FArcItemEditorCommands>("NxItemsConfigEditor.Common"
		, FText::FromString("Common")
		, NAME_None
		, FName(TEXT("LiveLinkStyle")))
{
}

void FArcItemEditorCommands::RegisterCommands()
{
	UI_COMMAND(CreateFrom
		, "Create From"
		, "Create new item, using selected as template"
		, EUserInterfaceActionType::Button
		, FInputChord());
}

void SArcItemsConfigEditor::Construct(const FArguments& InArgs)
{
}

#undef LOCTEXT_NAMESPACE
