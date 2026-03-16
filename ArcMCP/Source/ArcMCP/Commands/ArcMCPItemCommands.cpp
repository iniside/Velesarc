// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/Commands/ArcMCPItemCommands.h"
#include "Items/ArcItemJsonSerializer.h"

#include "ArcCore/Items/ArcItemDefinition.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"

REGISTER_ECA_COMMAND(FArcMCPCommand_ExportItemToJson)
REGISTER_ECA_COMMAND(FArcMCPCommand_ImportItemFromJson)

//------------------------------------------------------------------------------
// export_item_to_json
//------------------------------------------------------------------------------
FECACommandResult FArcMCPCommand_ExportItemToJson::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ItemPath;
	if (!GetStringParam(Params, TEXT("item_path"), ItemPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: item_path"));
	}

	UArcItemDefinition* ItemDef = LoadObject<UArcItemDefinition>(nullptr, *ItemPath);
	if (!ItemDef)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Item definition not found: %s"), *ItemPath));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("item_path"), ItemPath);
	Result->SetObjectField(TEXT("item_json"), FArcItemJsonSerializer::ExportToJson(ItemDef));
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// import_item_from_json
//------------------------------------------------------------------------------
FECACommandResult FArcMCPCommand_ImportItemFromJson::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ItemPath;
	if (!GetStringParam(Params, TEXT("item_path"), ItemPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: item_path"));
	}

	const TSharedPtr<FJsonObject>* ItemJsonPtr;
	if (!GetObjectParam(Params, TEXT("item_json"), ItemJsonPtr))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: item_json"));
	}

	// Load or create the item definition
	UArcItemDefinition* ItemDef = LoadObject<UArcItemDefinition>(nullptr, *ItemPath);
	bool bCreatedNew = false;

	if (!ItemDef)
	{
		// Create new item definition
		FString PackagePath, AssetName;
		ItemPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		if (AssetName.IsEmpty())
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Invalid item_path: %s"), *ItemPath));
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		ItemDef = Cast<UArcItemDefinition>(
			AssetTools.CreateAsset(AssetName, PackagePath, UArcItemDefinition::StaticClass(), nullptr));

		if (!ItemDef)
		{
			return FECACommandResult::Error(TEXT("Failed to create item definition"));
		}
		bCreatedNew = true;
	}

	// Import from JSON
	FString Error;
	bool bSuccess = FArcItemJsonSerializer::ImportFromJson(ItemDef, *ItemJsonPtr, Error);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("item_path"), ItemDef->GetPathName());
	Result->SetBoolField(TEXT("created_new"), bCreatedNew);

	if (!bSuccess)
	{
		Result->SetStringField(TEXT("warnings"), Error);
	}

	// Return the current state after import
	Result->SetObjectField(TEXT("item_json"), FArcItemJsonSerializer::ExportToJson(ItemDef));
	return FECACommandResult::Success(Result);
}
