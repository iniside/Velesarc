// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcSingleBotSpawner.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcBotData.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcBotData"

UCLASS()
class UArcAssetDefinition_ArcBotData : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Bot Data");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(75, 175, 125));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcBotData::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Entity", "Entity"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
