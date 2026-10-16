// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"
#include "Rocks/ArcEcotopeRockModel.h"

#include "ArcAssetDefinition_ArcEcotopeRockModel.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcEcotopeRockModel"

UCLASS()
class UArcAssetDefinition_ArcEcotopeRockModel : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Ecotope Rock Model");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(140, 132, 120));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcEcotopeRockModel::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = { FAssetCategoryPath(LOCTEXT("ArcEcotope", "Arc Ecotope")) };
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
