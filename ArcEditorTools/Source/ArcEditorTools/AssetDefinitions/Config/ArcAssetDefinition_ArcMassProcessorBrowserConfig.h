// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassProcessorBrowser/ArcMassProcessorBrowserConfig.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassProcessorBrowserConfig.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassProcessorBrowserConfig"

UCLASS()
class UArcAssetDefinition_ArcMassProcessorBrowserConfig : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Processor Browser Config");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(130, 130, 155));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassProcessorBrowserConfig::StaticClass();
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
