// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Loot/ArcLootTable.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_LootTable.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_LootTable"

UCLASS()
class UArcAssetDefinition_LootTable : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Loot Table");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(245, 165, 35));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcLootTable::StaticClass();
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
