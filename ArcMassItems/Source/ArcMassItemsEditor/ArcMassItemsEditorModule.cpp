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

#include "ArcMassItemsEditorModule.h"

#include "AssetToolsModule.h"
#include "Brushes/SlateImageBrush.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Interfaces/IPluginManager.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyleRegistry.h"
#include "Widgets/Docking/SDockTab.h"

#include "ArcAssetEditor_ItemDefinition.h"
#include "Editing/ArcJsonRecordEditing.h"
#include "Items/ArcItemDefinitionData.h"
#include "Customizations/ArcMutableIntParameterCustomization.h"
#include "Customizations/ArcMutableSkeletalMeshParameterCustomization.h"
#include "Customizations/ArcScalableCurveFloatCustomization.h"
#include "SArcItemsConfigEditor.h"

TSharedRef<SDockTab> FArcMassItemsEditorModule::SpawnItemsConfigEditor(const FSpawnTabArgs& TabArgs)
{
	TSharedRef<SArcItemsConfigEditor> NewItemsConfigEditor = SNew(SArcItemsConfigEditor);
	ItemsConfigEditor = NewItemsConfigEditor;
	return SAssignNew(ItemsConfigEditorTab, SDockTab).TabRole(ETabRole::NomadTab)[NewItemsConfigEditor];
}

void FArcMassItemsEditorModule::StartupModule()
{
	if (IsRunningGame())
	{
		return;
	}

	if (!StyleSet.IsValid())
	{
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);
		const FString IconRoot = IPluginManager::Get().FindPlugin(TEXT("ArcMassItems"))->GetBaseDir()
			+ TEXT("/Content/Editor/AssetIcons");

		StyleSet = MakeShared<FSlateStyleSet>(TEXT("ArcMassItemsEditor"));

		StyleSet->Set(TEXT("ClassThumbnail.ArcMassEquipmentSlotDefaultItems"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcMassEquipmentSlotDefaultItems.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcMassEquipmentSlotDefaultItems"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcMassEquipmentSlotDefaultItems.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcMassEquipmentSlotPreset"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcMassEquipmentSlotPreset.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcMassEquipmentSlotPreset"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcMassEquipmentSlotPreset.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcMassQuickBarConfig"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcMassQuickBarConfig.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcMassQuickBarConfig"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcMassQuickBarConfig.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcMassItemAttachmentConfig"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcMassItemAttachmentConfig.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcMassItemAttachmentConfig"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcMassItemAttachmentConfig.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcItemSocketSlotsPreset"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcItemSocketSlotsPreset.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcItemSocketSlotsPreset"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcItemSocketSlotsPreset.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcLootTable"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcLootTable.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcLootTable"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcLootTable.svg"), Icon16x16));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
	}

	FArcItemEditorCommands::Register();

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyEditor.RegisterCustomPropertyTypeLayout(
		"ArcMutableIntParameter",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcMutableIntParameterCustomization::MakeInstance));

	PropertyEditor.RegisterCustomPropertyTypeLayout(
		"ArcMutableSkeletalMeshParameter",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcMutableSkeletalMeshParameterCustomization::MakeInstance));

	PropertyEditor.RegisterCustomPropertyTypeLayout(
		"ArcScalableCurveFloat",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcScalableCurveFloatCustomization::MakeInstance));

	FGlobalTabmanager::Get()->RegisterTabSpawner(FName(TEXT("SArcItemsConfigEditorApp"))
		, FOnSpawnTab::CreateRaw(this, &FArcMassItemsEditorModule::SpawnItemsConfigEditor));

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	MenuExtender = MakeShareable(new FExtender());

	auto OpenItemsConfigEditor = [=]()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("SArcItemsConfigEditorApp")));
	};
	auto ItemsPullDownMenu = [=](class FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.AddMenuEntry(FText::FromString("Items Editor")
			, FText::FromString("Open Editor to configure Items.")
			, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Profiler.EventGraph.ExpandHotPath16")
			, FUIAction(FExecuteAction::CreateLambda(OpenItemsConfigEditor))
			, "ItemsConfigEditor");
	};

	MenuExtender->AddMenuExtension("ArcCoreExtension"
		, EExtensionHook::First
		, TSharedPtr<FUICommandList>()
		, FMenuExtensionDelegate::CreateLambda(ItemsPullDownMenu));

	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

	ArcJsonRecordEditing::RegisterRecordEditor(FArcItemDefinitionData::JsonExtension(),
		FArcJsonRecordEditorOpen::CreateStatic(&FArcAssetEditor_ItemDefinition::OpenRecord));
}

void FArcMassItemsEditorModule::ShutdownModule()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}

	ArcJsonRecordEditing::UnregisterRecordEditor(FArcItemDefinitionData::JsonExtension());

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditor.UnregisterCustomPropertyTypeLayout("ArcMutableIntParameter");
	PropertyEditor.UnregisterCustomPropertyTypeLayout("ArcMutableSkeletalMeshParameter");
	PropertyEditor.UnregisterCustomPropertyTypeLayout("ArcScalableCurveFloat");

	FGlobalTabmanager::Get()->UnregisterTabSpawner(FName(TEXT("SArcItemsConfigEditorApp")));
}

IMPLEMENT_MODULE(FArcMassItemsEditorModule, ArcMassItemsEditor)
