// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuilderEditorModule.h"
#include "Modules/ModuleManager.h"

EAssetTypeCategories::Type FArcBuilderEditorModule::ArcBuilderAssetCategory;

void FArcBuilderEditorModule::StartupModule()
{
	if (IsRunningGame())
	{
		return;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	ArcBuilderAssetCategory = AssetTools.RegisterAdvancedAssetCategory(
		FName(TEXT("Arc Builder")),
		FText::FromString("Arc Builder"));
}

void FArcBuilderEditorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FArcBuilderEditorModule, ArcBuilderEditor)
