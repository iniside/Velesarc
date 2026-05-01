// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Factory/ArcItemSpecGeneratorDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ItemGeneratorDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ItemGeneratorDefinition"

UCLASS()
class UArcAssetDefinition_ItemGeneratorDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Item Generator Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(235, 160, 40));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcItemGeneratorDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Items", "Items"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
