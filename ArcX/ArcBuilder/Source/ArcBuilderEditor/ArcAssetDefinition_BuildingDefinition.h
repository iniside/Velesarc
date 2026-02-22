// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcBuildingDefinition.h"
#include "AssetDefinitionDefault.h"

#include "ArcAssetDefinition_BuildingDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_BuildingDefinition"

UCLASS()
class UArcAssetDefinition_BuildingDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("BuildingDefinition", "Arc Building Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(191, 145, 72)); // Warm amber, distinct from recipe teal and item orange
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcBuildingDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath AssetPaths[] = {
			FAssetCategoryPath(LOCTEXT("ArcBuilder", "Arc Builder"))
		};
		return AssetPaths;
	}
};

#undef LOCTEXT_NAMESPACE
