// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"
#include "Catalog/ArcTownRoadDefinition.h"

#include "ArcAssetDefinition_ArcTownRoadDefinition.generated.h"

UCLASS()
class UArcAssetDefinition_ArcTownRoadDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(120, 130, 140));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcTownRoadDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};
