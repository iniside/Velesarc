// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"
#include "Catalog/ArcTownBuildingCatalog.h"

#include "ArcAssetDefinition_ArcTownBuildingCatalog.generated.h"

UCLASS()
class UArcAssetDefinition_ArcTownBuildingCatalog : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(150, 110, 60));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcTownBuildingCatalog::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};
