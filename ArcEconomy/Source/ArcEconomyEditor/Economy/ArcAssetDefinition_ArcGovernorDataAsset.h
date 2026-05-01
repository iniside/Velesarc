// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Data/ArcGovernorDataAsset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcGovernorDataAsset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcGovernorDataAsset"

UCLASS()
class UArcAssetDefinition_ArcGovernorDataAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Governor Data Asset");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(200, 180, 60));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcGovernorDataAsset::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Economy", "Economy"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
