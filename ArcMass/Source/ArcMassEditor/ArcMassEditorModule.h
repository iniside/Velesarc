// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetTypeCategories.h"
#include "Modules/ModuleManager.h"

class FArcMassEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
	
	static EAssetTypeCategories::Type GetArcMassAssetCategory()
	{
		return ArcMassAssetCategory;
	}

private:
	static EAssetTypeCategories::Type ArcMassAssetCategory;
};
