// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPToolset.h"
#include "ArcMCPToolsetUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UObjectIterator.h"

#include "ArcCore/Items/Fragments/ArcItemFragment.h"
#include "ArcCore/Items/Fragments/ArcFragmentDescription.h"
#include "ArcCore/Items/ArcItemDefinition.h"
#include "Items/ArcItemJsonSerializer.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"

//------------------------------------------------------------------------------
// DescribeFragments
//------------------------------------------------------------------------------

FString UArcMCPToolset::DescribeFragments(const FString& StructName, const FString& BaseType)
{
	FString StructNameFilter = StructName;
	StructNameFilter.RemoveFromStart(TEXT("F"));

	const UScriptStruct* FragmentBase = FArcItemFragment::StaticStruct();
	const UScriptStruct* ScalableBase = FArcScalableFloatItemFragment::StaticStruct();

	TArray<TSharedPtr<FJsonValue>> ResultArray;

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;

		const bool bIsFragment = Struct->IsChildOf(FragmentBase) && Struct != FragmentBase;
		const bool bIsScalable = Struct->IsChildOf(ScalableBase) && Struct != ScalableBase;

		if (!bIsFragment && !bIsScalable)
		{
			continue;
		}

		// Skip hidden/base structs
		if (Struct->HasMetaData(TEXT("Hidden")))
		{
			continue;
		}

		// Apply base_type filter
		if (!BaseType.IsEmpty())
		{
			if (BaseType == TEXT("fragment") && !bIsFragment)
			{
				continue;
			}
			if (BaseType == TEXT("scalable_float") && !bIsScalable)
			{
				continue;
			}
		}

		// Apply struct_name filter
		if (!StructNameFilter.IsEmpty() && Struct->GetName() != StructNameFilter)
		{
			continue;
		}

		// Allocate a default instance, call GetDescription, then destroy
		const int32 StructSize = Struct->GetStructureSize();
		const int32 StructAlign = Struct->GetMinAlignment();
		uint8* Memory = static_cast<uint8*>(FMemory::Malloc(StructSize, StructAlign));
		Struct->InitializeStruct(Memory);

		FArcFragmentDescription Desc;
		if (bIsFragment)
		{
			const FArcItemFragment* Fragment = reinterpret_cast<const FArcItemFragment*>(Memory);
			Desc = Fragment->GetDescription(Struct);
		}
		else
		{
			const FArcScalableFloatItemFragment* Fragment = reinterpret_cast<const FArcScalableFloatItemFragment*>(Memory);
			Desc = Fragment->GetDescription(Struct);
		}

		Struct->DestroyStruct(Memory);
		FMemory::Free(Memory);

		ResultArray.Add(MakeShared<FJsonValueObject>(Desc.ToJson()));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("fragments"), ResultArray);
	Result->SetNumberField(TEXT("count"), ResultArray.Num());

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}

//------------------------------------------------------------------------------
// ExportItemToJson
//------------------------------------------------------------------------------

FString UArcMCPToolset::ExportItemToJson(const FString& ItemPath)
{
	if (ItemPath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: ItemPath"));
	}

	UArcItemDefinition* ItemDef = LoadObject<UArcItemDefinition>(nullptr, *ItemPath);
	if (!ItemDef)
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Item definition not found: %s"), *ItemPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("item_path"), ItemPath);
	Result->SetObjectField(TEXT("item_json"), FArcItemJsonSerializer::ExportToJson(ItemDef));

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}

//------------------------------------------------------------------------------
// ImportItemFromJson
//------------------------------------------------------------------------------

FString UArcMCPToolset::ImportItemFromJson(const FString& ItemPath, const FString& ItemJson)
{
	if (ItemPath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: ItemPath"));
	}

	if (ItemJson.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: ItemJson"));
	}

	TSharedPtr<FJsonObject> ParsedJson = ArcMCPToolsetPrivate::StringToJsonObj(ItemJson);
	if (!ParsedJson.IsValid())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Failed to parse ItemJson as a JSON object"));
	}

	// Load or create the item definition
	UArcItemDefinition* ItemDef = LoadObject<UArcItemDefinition>(nullptr, *ItemPath);
	bool bCreatedNew = false;

	if (!ItemDef)
	{
		FString PackagePath;
		FString AssetName;
		ItemPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		if (AssetName.IsEmpty())
		{
			return ArcMCPToolsetPrivate::MakeError(
				FString::Printf(TEXT("Invalid ItemPath: %s"), *ItemPath));
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		ItemDef = Cast<UArcItemDefinition>(
			AssetTools.CreateAsset(AssetName, PackagePath, UArcItemDefinition::StaticClass(), nullptr));

		if (!ItemDef)
		{
			return ArcMCPToolsetPrivate::MakeError(TEXT("Failed to create item definition"));
		}
		bCreatedNew = true;
	}

	// Import from JSON
	FString Error;
	const bool bSuccess = FArcItemJsonSerializer::ImportFromJson(ItemDef, ParsedJson, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("item_path"), ItemDef->GetPathName());
	Result->SetBoolField(TEXT("created_new"), bCreatedNew);

	if (!bSuccess)
	{
		Result->SetStringField(TEXT("warnings"), Error);
	}

	// Return the current state after import
	Result->SetObjectField(TEXT("item_json"), FArcItemJsonSerializer::ExportToJson(ItemDef));

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}
