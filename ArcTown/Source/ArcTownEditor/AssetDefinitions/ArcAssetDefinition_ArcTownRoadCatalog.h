// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"
#include "Catalog/ArcTownRoadCatalog.h"

#include "ArcAssetDefinition_ArcTownRoadCatalog.generated.h"

UCLASS()
class UArcAssetDefinition_ArcTownRoadCatalog : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(90, 100, 110));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcTownRoadCatalog::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};
