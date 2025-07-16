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

#include "ArcCoreEditorModule.h"
#include "EditorStyleSet.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#include "EdGraphUtilities.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"


#include "Items\ArcAssetDefinition_ArcItemDefinition.h"

#include "DetailWidgetRow.h"
#include "Engine/AssetManager.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SNumericEntryBox.h"

#include "Items/SArcItemsConfigEditor.h"

#include "DataRegistryEditorModule.h"
#include "DataRegistrySubsystem.h"

#include "ArcAssetTypeAction_InputPlayerSettings.h"
#include "ArcInstancedStructDetails.h"
#include "ArcInstancedStructPin.h"
#include "ArcNamedPrimaryAssetIdCustomization.h"
#include "ArcScalableFloatDetails.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

#include "ComboSystem/ArcAssetTypeActions_ComboTreeData.h"
#include "ComboSystem/ArcComboGraphTreeNode.h"
#include "ComboSystem/SArcComboTreeGraphNode.h"
#include "Items/ArcItemDefinitionDetailCustomization.h"
#include "ArcNamedPrimaryAssetIdCustomization.h"

TSharedRef<SDockTab> FArcCoreEditorModule::SpawnItemsConfigEditor(const FSpawnTabArgs& TabArgs)
{
	TSharedRef<SArcItemsConfigEditor> NewItemsConfigEditor = SNew(SArcItemsConfigEditor);
	ItemsConfigEditor = NewItemsConfigEditor;
	return SAssignNew(ItemsConfigEditorTab
		, SDockTab).TabRole(ETabRole::NomadTab)[NewItemsConfigEditor];
}

class FArcAbilityTreeNodeFactory : public FGraphPanelNodeFactory
{
private:
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if (UArcComboGraphTreeNode* BaseNode = Cast<UArcComboGraphTreeNode>(Node))
		{
			return SNew(SArcComboTreeGraphNode
				, BaseNode);
		}

		return nullptr;
	}
};

EAssetTypeCategories::Type FArcCoreEditorModule::ArcAssetCategory;
EAssetTypeCategories::Type FArcCoreEditorModule::InputAssetCategory;

void FArcCoreEditorModule::StartupModule()
{
	if (IsRunningGame())
	{
		return;
	}
	FArcItemEditorCommands::Register();
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	ArcAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Arc Core"))
		, FText::FromString("Arc Core"));
	InputAssetCategory = AssetTools.FindAdvancedAssetCategory(FName(TEXT("Input")));
	{
		RegisterAssetTypeAction(AssetTools
			, MakeShareable(new FArcAssetTypeActions_QuickBarComponent()));
		RegisterAssetTypeAction(AssetTools
			, MakeShareable(new FArcAssetTypeActions_ItemsStoreComponent()));
	}

	// Register asset types
	RegisterAssetTypeAction(AssetTools
		, MakeShareable(new FArcAssetTypeActions_ComboTreeData()));

	// Register custom graph nodes

	ItemEditorToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(
		"PropertyEditor");

	PropertyEditor.RegisterCustomPropertyTypeLayout("ArcInstancedStruct"
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcInstancedStructDetails::MakeInstance));
	
	PropertyEditor.RegisterCustomPropertyTypeLayout("ArcScalableFloat"
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcScalableFloatDetails::MakeInstance));

	PropertyEditor.RegisterCustomClassLayout("ArcItemDefinition"
		, FOnGetDetailCustomizationInstance::CreateStatic(&FArcItemDefinitionDetailCustomization::MakeInstance));

	PropertyEditor.RegisterCustomClassLayout("ArcItemFragment_MakeLocationInfo"
		, FOnGetDetailCustomizationInstance::CreateStatic(&FArcItemFragment_MakeLocationInfoDetailCustomization::MakeInstance));
	
	PropertyEditor.RegisterCustomPropertyTypeLayout("ArcNamedPrimaryAssetId"
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcNamedPrimaryAssetIdCustomization::MakeInstance));
	
	if (!StyleSet.IsValid())
	{
		const FVector2D Icon16x16(16.0f
			, 16.0f);
		const FVector2D Icon20x20(20.0f
			, 20.0f);
		const FVector2D Icon40x40(40.0f
			, 40.0f);
		const FVector2D Icon64x64(64.0f
			, 64.0f);
		FString HairStrandsContent = IPluginManager::Get().FindPlugin(TEXT("ArcCore"))->GetBaseDir() + "/Content";

		StyleSet = MakeShared<FSlateStyleSet>("ArcCore");
		StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
		StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		StyleSet->Set("ClassThumbnail.ArcInputActionConfig"
			, new FSlateImageBrush(HairStrandsContent + "/Editor/Slate/keyboard_icon_512px.png"
				, Icon20x20));

		StyleSet->Set("ClassThumbnail.SfxInputConfig"
			, new FSlateImageBrush(HairStrandsContent + "/Editor/Slate/keyboard_icon_512px.png"
				, Icon20x20));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
	}
	
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	MenuExtender = MakeShareable(new FExtender());
	auto PullDownMenu = [=](class FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection
		(
			"ArcCoreExtension",
			FText::FromString("Arc")
		);
	};
	
	auto ExtendMenu = [=](class FMenuBarBuilder& MenuBuilder)
	{
		MenuBuilder.AddPullDownMenu
		(
			FText::FromString("Arc"),
			FText::FromString("Arc"),
			FNewMenuDelegate::CreateLambda(PullDownMenu),
			"ArcMenu"
		);
	};
	// Add code editor extension to main menu
	MenuExtender->AddMenuBarExtension(
		"Actions",
		EExtensionHook::After,
		TSharedPtr<FUICommandList>(),
		FMenuBarExtensionDelegate::CreateLambda(ExtendMenu));
	
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	
	FGlobalTabmanager::Get()->RegisterTabSpawner(FName(TEXT("SArcItemsConfigEditorApp"))
		, FOnSpawnTab::CreateRaw(this
			, &FArcCoreEditorModule::SpawnItemsConfigEditor));
	
	auto OpenItemsConfigEditor = [=]()
	{
		// TryInvokeTab
		FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("SArcItemsConfigEditorApp")));
	};
	auto ItemsPullDownMenu = [=] (class FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.AddMenuEntry(FText::FromString("Items Editor")
			, FText::FromString("Open Editor to configure Items.")
			, FSlateIcon(FAppStyle::GetAppStyleSetName()
				, "Profiler.EventGraph.ExpandHotPath16")
			, FUIAction(FExecuteAction::CreateLambda(OpenItemsConfigEditor))
			, "ItemsConfigEditor");
	};

	GetMenuExtender()->AddMenuExtension("ArcCoreExtension"
		, EExtensionHook::First
		, TSharedPtr<FUICommandList>()
		, FMenuExtensionDelegate::CreateLambda(ItemsPullDownMenu));

	
	//InstancedStructPinFactory = MakeShareable(new FArcInstancedStructPinGraphPinFactory());
	//FEdGraphUtilities::RegisterVisualPinFactory(InstancedStructPinFactory);

	// This code will execute after your module is loaded into memory; the exact timing is
	// specified in the .uplugin file per-module
}

void FArcCoreEditorModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(
		"PropertyEditor");
	PropertyEditor.UnregisterCustomPropertyTypeLayout("ArcItemDataAttributeContainer");
	PropertyEditor.UnregisterCustomPropertyTypeLayout("ArcItemGameplayCueContainer");
	PropertyEditor.UnregisterCustomPropertyTypeLayout("ArcEquipmentSlotConfigContainer");
	PropertyEditor.UnregisterCustomPropertyTypeLayout("ArcAttribute");
	PropertyEditor.UnregisterCustomPropertyTypeLayout("ArcItemSlot");
	PropertyEditor.UnregisterCustomPropertyTypeLayout("ArcItemGrantedAbility");

	CreatedAssetTypeActions.Empty();
	// This function may be called during shutdown to clean up your module.  For modules
	// that support dynamic reloading, we call this function before unloading the module.
}

void FArcCoreEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools
												   , TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcCoreEditorModule, ArcCoreEditor)
