// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Factory/ArcItemFactoryStats.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ItemFactoryStats.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ItemFactoryStats"

UCLASS()
class UArcAssetDefinition_ItemFactoryStats : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Item Factory Stats");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(210, 135, 55));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcItemFactoryStats::StaticClass();
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
