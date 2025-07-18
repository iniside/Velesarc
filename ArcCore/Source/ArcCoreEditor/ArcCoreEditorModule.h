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

#pragma once

#include "AssetToolsModule.h"

#include "Modules/ModuleManager.h"

#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Widgets/Docking/SDockTab.h"

#include "AssetToolsModule.h"
#include "Toolkits/AssetEditorToolkit.h"

#include "GameplayAbilitiesEditorModule.h"

DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<FExtender>
	, FArcOnPreGenerateToolbars
	, UObject*);


class FArcCoreEditorModule : public IModuleInterface
{
	friend class FArcAssetEditor_ItemData;

private:
	TSharedPtr<FExtender> MenuExtender;
	TArray<TSharedPtr<IAssetTypeActions>> CreatedAssetTypeActions;

	TSharedPtr<FExtensibilityManager> ItemEditorToolBarExtensibilityManager;
	FArcOnPreGenerateToolbars OnPreGenerateToolbars;

	TWeakPtr<class SArcItemsConfigEditor> ItemsConfigEditor;
	TWeakPtr<SDockTab> ItemsConfigEditorTab;

	static EAssetTypeCategories::Type ArcAssetCategory;
	static EAssetTypeCategories::Type InputAssetCategory;

	TSharedPtr<class FArcInstancedStructPinGraphPinFactory> InstancedStructPinFactory;

public:
	static EAssetTypeCategories::Type GetArcAssetCategory()
	{
		return ArcAssetCategory;
	}

	static EAssetTypeCategories::Type GetInputAssetCategory()
	{
		return InputAssetCategory;
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	TSharedPtr<FExtensibilityManager> GetItemEditorToolBarExtensibilityManager()
	{
		return ItemEditorToolBarExtensibilityManager;
	}

	void SetOnPreGenerateToolbars(const FArcOnPreGenerateToolbars& InDelegate)
	{
		OnPreGenerateToolbars = InDelegate;
	}

	TSharedPtr<FExtender> GetMenuExtender()
	{
		return MenuExtender;
	}

private:
	void RegisterAssetTypeAction(IAssetTools& AssetTools
								 , TSharedRef<IAssetTypeActions> Action);

	TSharedRef<SDockTab> SpawnItemsConfigEditor(const FSpawnTabArgs& TabArgs);

	float PreviewLevel = 0;
	float MaxPreviewLevel = 20;
	TSharedPtr<IPropertyHandle> ValueProperty;
	TSharedPtr<IPropertyHandle> CurveTableHandleProperty;
	TSharedPtr<IPropertyHandle> RegistryTypeProperty;
	TSharedPtr<IPropertyHandle> RowNameProperty;
	TSharedPtr<IPropertyHandle> CurveTableProperty;

	TSharedPtr<FSlateStyleSet> StyleSet;
};
