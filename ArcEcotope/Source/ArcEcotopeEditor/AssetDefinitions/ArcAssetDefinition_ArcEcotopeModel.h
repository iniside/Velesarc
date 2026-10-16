// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Ecotope/ArcEcotopeModel.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcEcotopeModel.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcEcotopeModel"

UCLASS()
class UArcAssetDefinition_ArcEcotopeModel : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Ecotope Model");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(90, 160, 90));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcEcotopeModel::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = { FAssetCategoryPath(LOCTEXT("ArcEcotope", "Arc Ecotope")) };
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
