// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/Commands/ArcMCPAssetCommands.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Commands/ECACommand.h"
#include "Engine/DataAsset.h"

REGISTER_ECA_COMMAND(FArcMCPCommand_CreateDataAsset)

FECACommandResult FArcMCPCommand_CreateDataAsset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetName;
	if (!GetStringParam(Params, TEXT("asset_name"), AssetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_name"));
	}

	FString PackagePath;
	if (!GetStringParam(Params, TEXT("package_path"), PackagePath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: package_path"));
	}

	FString AssetClassPath;
	if (!GetStringParam(Params, TEXT("asset_class"), AssetClassPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_class"));
	}

	// Load the specified class and validate it derives from UDataAsset
	UClass* AssetClass = LoadClass<UDataAsset>(nullptr, *AssetClassPath);
	if (!AssetClass)
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Class not found or not a UDataAsset subclass: %s"), *AssetClassPath));
	}

	// Create the asset via AssetTools
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, AssetClass, nullptr);

	if (!NewAsset)
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Failed to create asset '%s' at '%s'"), *AssetName, *PackagePath));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), NewAsset->GetPathName());
	Result->SetStringField(TEXT("asset_name"), NewAsset->GetName());
	Result->SetStringField(TEXT("asset_class"), AssetClass->GetPathName());

	return FECACommandResult::Success(Result);
}
