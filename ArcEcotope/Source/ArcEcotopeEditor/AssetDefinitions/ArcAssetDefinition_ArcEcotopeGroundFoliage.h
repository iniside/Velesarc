// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Foliage/ArcEcotopeGroundFoliage.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcEcotopeGroundFoliage.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcEcotopeGroundFoliage"

UCLASS()
class UArcAssetDefinition_ArcEcotopeGroundFoliage : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Ecotope Ground Foliage");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(110, 180, 70));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcEcotopeGroundFoliage::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = { FAssetCategoryPath(LOCTEXT("ArcEcotope", "Arc Ecotope")) };
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
