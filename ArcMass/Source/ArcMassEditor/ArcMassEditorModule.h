// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetTypeCategories.h"
#include "Modules/ModuleManager.h"

class UArcPlacedEntityPlacementFactory;
class IAssetEditorInstance;
class UPackage;
class FObjectPostSaveContext;

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

	UArcPlacedEntityPlacementFactory* PlacementFactory = nullptr;

	FDelegateHandle ViewportDebugVizStartupHandle;
	FDelegateHandle PostEngineInitHandle;

	FDelegateHandle OnAssetOpenedHandle;
	FDelegateHandle OnPackageSavedHandle;

	void OnAssetOpenedInEditor(UObject* Asset, IAssetEditorInstance* EditorInstance);
	void OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext SaveContext);
};
