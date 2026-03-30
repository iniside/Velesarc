// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPToolset.h"
#include "ArcMCPToolsetUtils.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/DataAsset.h"

FString UArcMCPToolset::CreateDataAsset(const FString& AssetName, const FString& PackagePath, const FString& AssetClass)
{
	// Validate parameters
	if (AssetName.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: AssetName"));
	}

	if (PackagePath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: PackagePath"));
	}

	if (AssetClass.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: AssetClass"));
	}

	// Load the specified class and validate it derives from UDataAsset
	UClass* LoadedAssetClass = LoadClass<UDataAsset>(nullptr, *AssetClass);
	if (!LoadedAssetClass)
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Class not found or not a UDataAsset subclass: %s"), *AssetClass));
	}

	// Create the asset via AssetTools
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, LoadedAssetClass, nullptr);

	if (!NewAsset)
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Failed to create asset '%s' at '%s'"), *AssetName, *PackagePath));
	}

	// Build result JSON
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), NewAsset->GetPathName());
	Result->SetStringField(TEXT("asset_name"), NewAsset->GetName());
	Result->SetStringField(TEXT("asset_class"), LoadedAssetClass->GetPathName());

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}
