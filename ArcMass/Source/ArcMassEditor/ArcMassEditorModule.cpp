// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEditorModule.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "PropertyEditorModule.h"
#include "Customization/ArcEntityVisualizationTraitCustomization.h"
#include "Customization/ArcMassPhysicsBodyConfigCustomization.h"
#include "ArcMass/Visualization/ArcMassEntityVisualizationTrait.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"

#define LOCTEXT_NAMESPACE "ArcMassEditor"

EAssetTypeCategories::Type FArcMassEditorModule::ArcMassAssetCategory;

void FArcMassEditorModule::StartupModule()
{
    if (IsRunningGame())
    {
        return;
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    ArcMassAssetCategory = AssetTools.RegisterAdvancedAssetCategory(
        FName(TEXT("Arc Mass")),
        FText::FromString("Arc Mass"));

    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

    PropertyModule.RegisterCustomClassLayout(
        UArcEntityVisualizationTrait::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FArcEntityVisualizationTraitCustomization::MakeInstance));

    PropertyModule.RegisterCustomPropertyTypeLayout(
        FArcMassPhysicsBodyConfigFragment::StaticStruct()->GetFName(),
        FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcMassPhysicsBodyConfigCustomization::MakeInstance));
}

void FArcMassEditorModule::ShutdownModule()
{
    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
    {
        FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropertyModule.UnregisterCustomClassLayout(UArcEntityVisualizationTrait::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomPropertyTypeLayout(FArcMassPhysicsBodyConfigFragment::StaticStruct()->GetFName());
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMassEditorModule, ArcMassEditor)
