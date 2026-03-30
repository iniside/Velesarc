// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPToolset.h"
#include "ArcMCPToolsetUtils.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UObjectIterator.h"
#include "ArcCore/Items/ArcItemDefinition.h"

namespace ArcMCPTemplateHelpers
{
	/**
	 * Find a UScriptStruct that is a fragment type by name.
	 * Accepts names with or without the F prefix (UHT strips F from struct names).
	 */
	UScriptStruct* FindFragmentStructByName(const FString& InName)
	{
		FString LookupName = InName;
		LookupName.RemoveFromStart(TEXT("F"));

		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			if (It->GetName() == LookupName)
			{
				if (It->IsChildOf(FArcItemFragment::StaticStruct()) ||
					It->IsChildOf(FArcScalableFloatItemFragment::StaticStruct()))
				{
					return *It;
				}
			}
		}
		return nullptr;
	}
} // namespace ArcMCPTemplateHelpers

// ============================================================================
// CreateItemFromTemplate
// ============================================================================

FString UArcMCPToolset::CreateItemFromTemplate(const FString& TemplatePath, const FString& AssetName, const FString& PackagePath)
{
	if (TemplatePath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: TemplatePath"));
	}

	if (AssetName.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: AssetName"));
	}

	if (PackagePath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: PackagePath"));
	}

	UArcItemDefinitionTemplate* Template = LoadObject<UArcItemDefinitionTemplate>(nullptr, *TemplatePath);
	if (!Template)
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Template not found: %s"), *TemplatePath));
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UArcItemDefinition::StaticClass(), nullptr);
	if (!NewAsset)
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Failed to create item definition '%s' at '%s'"), *AssetName, *PackagePath));
	}

	UArcItemDefinition* NewItem = Cast<UArcItemDefinition>(NewAsset);

	Template->SetNewOrReplaceItemTemplate(NewItem);
	NewItem->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), NewItem->GetPathName());
	Result->SetStringField(TEXT("asset_name"), NewItem->GetName());
	Result->SetStringField(TEXT("template_path"), Template->GetPathName());

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}

// ============================================================================
// TemplateAddFragment
// ============================================================================

FString UArcMCPToolset::TemplateAddFragment(const FString& TemplatePath, const FString& FragmentType)
{
	if (TemplatePath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: TemplatePath"));
	}

	if (FragmentType.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: FragmentType"));
	}

	UArcItemDefinitionTemplate* Template = LoadObject<UArcItemDefinitionTemplate>(nullptr, *TemplatePath);
	if (!Template)
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Template not found: %s"), *TemplatePath));
	}

	UScriptStruct* FragmentStruct = ArcMCPTemplateHelpers::FindFragmentStructByName(FragmentType);
	if (!FragmentStruct)
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Fragment type not found: %s. Use describe_fragments to see available types."), *FragmentType));
	}

	if (!Template->AddFragmentByType(FragmentStruct))
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Fragment '%s' already exists on template or is invalid."), *FragmentType));
	}

	Template->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("template_path"), Template->GetPathName());
	Result->SetStringField(TEXT("fragment_added"), FragmentType);

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}

// ============================================================================
// TemplateRemoveFragment
// ============================================================================

FString UArcMCPToolset::TemplateRemoveFragment(const FString& TemplatePath, const FString& FragmentType)
{
	if (TemplatePath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: TemplatePath"));
	}

	if (FragmentType.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: FragmentType"));
	}

	UArcItemDefinitionTemplate* Template = LoadObject<UArcItemDefinitionTemplate>(nullptr, *TemplatePath);
	if (!Template)
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Template not found: %s"), *TemplatePath));
	}

	UScriptStruct* FragmentStruct = ArcMCPTemplateHelpers::FindFragmentStructByName(FragmentType);
	if (!FragmentStruct)
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Fragment type not found: %s"), *FragmentType));
	}

	if (!Template->RemoveFragmentByType(FragmentStruct))
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Fragment '%s' not found on template."), *FragmentType));
	}

	Template->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("template_path"), Template->GetPathName());
	Result->SetStringField(TEXT("fragment_removed"), FragmentType);

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}

// ============================================================================
// UpdateItemsFromTemplate
// ============================================================================

FString UArcMCPToolset::UpdateItemsFromTemplate(const FString& TemplatePath)
{
	if (TemplatePath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: TemplatePath"));
	}

	UArcItemDefinitionTemplate* Template = LoadObject<UArcItemDefinitionTemplate>(nullptr, *TemplatePath);
	if (!Template)
	{
		return ArcMCPToolsetPrivate::MakeError(
			FString::Printf(TEXT("Template not found: %s"), *TemplatePath));
	}

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
		if (AssetData.AssetClassPath == UArcItemDefinitionTemplate::StaticClass()->GetClassPathName())
		{
			continue;
		}

		UArcItemDefinition* ItemDef = Cast<UArcItemDefinition>(AssetData.GetAsset());
		if (!ItemDef)
		{
			continue;
		}

		if (ItemDef->GetSourceTemplate() != Template)
		{
			continue;
		}

		Template->UpdateFromTemplate(ItemDef);
		ItemDef->MarkPackageDirty();

		UpdatedArray.Add(MakeShared<FJsonValueString>(ItemDef->GetPathName()));
		++UpdatedCount;
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("template_path"), Template->GetPathName());
	Result->SetNumberField(TEXT("updated_count"), UpdatedCount);
	Result->SetArrayField(TEXT("updated_items"), UpdatedArray);

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}
