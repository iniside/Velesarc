// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"

class FArcBuilderEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static EAssetTypeCategories::Type GetArcBuilderAssetCategory()
	{
		return ArcBuilderAssetCategory;
	}

private:
	static EAssetTypeCategories::Type ArcBuilderAssetCategory;
};
