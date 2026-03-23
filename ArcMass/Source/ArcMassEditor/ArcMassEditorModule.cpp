// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEditorModule.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"

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
}

void FArcMassEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMassEditorModule, ArcMassEditor)
