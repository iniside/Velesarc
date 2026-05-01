// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Widgets/ArcSettingsCategoryAsset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcSettingsCategoryAsset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcSettingsCategoryAsset"

UCLASS()
class UArcAssetDefinition_ArcSettingsCategoryAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Settings Category Asset");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(150, 150, 170));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcSettingsCategoryAsset::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Config", "Config"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
