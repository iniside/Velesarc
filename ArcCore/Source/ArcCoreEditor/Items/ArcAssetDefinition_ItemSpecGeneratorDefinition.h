// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Factory/ArcItemSpecGeneratorDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ItemSpecGeneratorDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ItemSpecGeneratorDefinition"

UCLASS()
class UArcAssetDefinition_ItemSpecGeneratorDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Item Spec Generator Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(225, 145, 65));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcItemSpecGeneratorDefinition::StaticClass();
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
