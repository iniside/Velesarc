// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "QuickBar/ArcMassQuickBarConfig.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassQuickBarConfig.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassQuickBarConfig"

UCLASS()
class UArcAssetDefinition_ArcMassQuickBarConfig : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass QuickBar Config");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(120, 200, 220));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassQuickBarConfig::StaticClass();
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
