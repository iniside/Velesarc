// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"
#include "Species/ArcEcotopeSpeciesModel.h"

#include "ArcAssetDefinition_ArcEcotopeTreeSpeciesModel.generated.h"

UCLASS()
class UArcAssetDefinition_ArcEcotopeTreeSpeciesModel : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(70, 130, 180));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcEcotopeTreeSpeciesModel::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;

	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
