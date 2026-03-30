#if 0

// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/Commands/ArcMCPTemplateCommands.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Commands/ECACommand.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UObjectIterator.h"
#include "ArcCore/Items/ArcItemDefinition.h"

REGISTER_ECA_COMMAND(FArcMCPCommand_CreateItemFromTemplate)
REGISTER_ECA_COMMAND(FArcMCPCommand_TemplateAddFragment)
REGISTER_ECA_COMMAND(FArcMCPCommand_TemplateRemoveFragment)
REGISTER_ECA_COMMAND(FArcMCPCommand_UpdateItemsFromTemplate)

// Helper: find UScriptStruct by name from the global iterator.
// Accepts names with or without the F prefix (UHT strips F from struct names).
static UScriptStruct* FindFragmentStructByName(const FString& InName)
{
	FString LookupName = InName;
	LookupName.RemoveFromStart(TEXT("F"));

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->GetName() == LookupName)
		{
			// Verify it's actually a fragment type
			if (It->IsChildOf(FArcItemFragment::StaticStruct()) ||
				It->IsChildOf(FArcScalableFloatItemFragment::StaticStruct()))
			{
				return *It;
			}
		}
	}
	return nullptr;
}

// ============================================================================
// create_item_from_template
// ============================================================================

FECACommandResult FArcMCPCommand_CreateItemFromTemplate::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TemplatePath;
	if (!GetStringParam(Params, TEXT("template_path"), TemplatePath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: template_path"));
	}

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

	// Load the template
	UArcItemDefinitionTemplate* Template = LoadObject<UArcItemDefinitionTemplate>(nullptr, *TemplatePath);
	if (!Template)
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Template not found: %s"), *TemplatePath));
	}

	// Create new item definition
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UArcItemDefinition::StaticClass(), nullptr);
	if (!NewAsset)
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Failed to create item definition '%s' at '%s'"), *AssetName, *PackagePath));
	}

	UArcItemDefinition* NewItem = Cast<UArcItemDefinition>(NewAsset);

	// Apply template fragments
	Template->SetNewOrReplaceItemTemplate(NewItem);
	NewItem->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), NewItem->GetPathName());
	Result->SetStringField(TEXT("asset_name"), NewItem->GetName());
	Result->SetStringField(TEXT("template_path"), Template->GetPathName());

	return FECACommandResult::Success(Result);
}

// ============================================================================
// template_add_fragment
// ============================================================================

FECACommandResult FArcMCPCommand_TemplateAddFragment::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TemplatePath;
	if (!GetStringParam(Params, TEXT("template_path"), TemplatePath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: template_path"));
	}

	FString FragmentType;
	if (!GetStringParam(Params, TEXT("fragment_type"), FragmentType))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: fragment_type"));
	}

	// Load the template
	UArcItemDefinitionTemplate* Template = LoadObject<UArcItemDefinitionTemplate>(nullptr, *TemplatePath);
	if (!Template)
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Template not found: %s"), *TemplatePath));
	}

	// Find the fragment struct
	UScriptStruct* FragmentStruct = FindFragmentStructByName(FragmentType);
	if (!FragmentStruct)
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Fragment type not found: %s. Use describe_fragments to see available types."), *FragmentType));
	}

	// Add fragment
	if (!Template->AddFragmentByType(FragmentStruct))
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Fragment '%s' already exists on template or is invalid."), *FragmentType));
	}

	Template->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("template_path"), Template->GetPathName());
	Result->SetStringField(TEXT("fragment_added"), FragmentType);

	return FECACommandResult::Success(Result);
}

// ============================================================================
// template_remove_fragment
// ============================================================================

FECACommandResult FArcMCPCommand_TemplateRemoveFragment::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TemplatePath;
	if (!GetStringParam(Params, TEXT("template_path"), TemplatePath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: template_path"));
	}

	FString FragmentType;
	if (!GetStringParam(Params, TEXT("fragment_type"), FragmentType))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: fragment_type"));
	}

	UArcItemDefinitionTemplate* Template = LoadObject<UArcItemDefinitionTemplate>(nullptr, *TemplatePath);
	if (!Template)
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Template not found: %s"), *TemplatePath));
	}

	UScriptStruct* FragmentStruct = FindFragmentStructByName(FragmentType);
	if (!FragmentStruct)
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Fragment type not found: %s"), *FragmentType));
	}

	if (!Template->RemoveFragmentByType(FragmentStruct))
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Fragment '%s' not found on template."), *FragmentType));
	}

	Template->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("template_path"), Template->GetPathName());
	Result->SetStringField(TEXT("fragment_removed"), FragmentType);

	return FECACommandResult::Success(Result);
}

// ============================================================================
// update_items_from_template
// ============================================================================

FECACommandResult FArcMCPCommand_UpdateItemsFromTemplate::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TemplatePath;
	if (!GetStringParam(Params, TEXT("template_path"), TemplatePath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: template_path"));
	}

	UArcItemDefinitionTemplate* Template = LoadObject<UArcItemDefinitionTemplate>(nullptr, *TemplatePath);
	if (!Template)
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("Template not found: %s"), *TemplatePath));
	}

	// Find all UArcItemDefinition assets
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UArcItemDefinition::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAssets(Filter, AllAssets);

	TArray<TSharedPtr<FJsonValue>> UpdatedArray;
	int32 UpdatedCount = 0;

	for (const FAssetData& AssetData : AllAssets)
	{
		// Skip templates themselves
		if (AssetData.AssetClassPath == UArcItemDefinitionTemplate::StaticClass()->GetClassPathName())
		{
			continue;
		}

		// Load the item definition
		UArcItemDefinition* ItemDef = Cast<UArcItemDefinition>(AssetData.GetAsset());
		if (!ItemDef)
		{
			continue;
		}

		// Check if it references this template
		if (ItemDef->GetSourceTemplate() != Template)
		{
			continue;
		}

		// Update from template
		Template->UpdateFromTemplate(ItemDef);
		ItemDef->MarkPackageDirty();

		UpdatedArray.Add(MakeShared<FJsonValueString>(ItemDef->GetPathName()));
		++UpdatedCount;
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("template_path"), Template->GetPathName());
	Result->SetNumberField(TEXT("updated_count"), UpdatedCount);
	Result->SetArrayField(TEXT("updated_items"), UpdatedArray);

	return FECACommandResult::Success(Result);
}

#endif
