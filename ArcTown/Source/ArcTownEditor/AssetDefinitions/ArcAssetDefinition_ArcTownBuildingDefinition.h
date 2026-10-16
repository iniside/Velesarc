// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"
#include "Catalog/ArcTownBuildingDefinition.h"

#include "ArcAssetDefinition_ArcTownBuildingDefinition.generated.h"

UCLASS()
class UArcAssetDefinition_ArcTownBuildingDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(200, 150, 80));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcTownBuildingDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;

	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
