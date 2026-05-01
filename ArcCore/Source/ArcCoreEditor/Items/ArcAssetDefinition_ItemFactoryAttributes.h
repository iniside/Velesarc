// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Factory/ArcItemFactoryAttributes.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ItemFactoryAttributes.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ItemFactoryAttributes"

UCLASS()
class UArcAssetDefinition_ItemFactoryAttributes : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Item Factory Attributes");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(230, 150, 50));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcItemFactoryAttributes::StaticClass();
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
