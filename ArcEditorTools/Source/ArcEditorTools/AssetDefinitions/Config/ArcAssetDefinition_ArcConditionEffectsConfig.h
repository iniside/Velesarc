// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcConditionEffects/ArcConditionEffectsConfig.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcConditionEffectsConfig.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcConditionEffectsConfig"

UCLASS()
class UArcAssetDefinition_ArcConditionEffectsConfig : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Condition Effects Config");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(140, 140, 160));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcConditionEffectsConfig::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Config", "Config"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
