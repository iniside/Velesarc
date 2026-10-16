// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcAssetDefinition_ArcTownBuildingCatalog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAssetDefinition_ArcTownBuildingCatalog)

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcTownBuildingCatalog"

FText UArcAssetDefinition_ArcTownBuildingCatalog::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Arc Town Building Catalog");
}

TConstArrayView<FAssetCategoryPath> UArcAssetDefinition_ArcTownBuildingCatalog::GetAssetCategories() const
{
	static FAssetCategoryPath Categories[] = {
		FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Town", "Town"), ECategoryMenuType::Section)
	};
	return Categories;
}

#undef LOCTEXT_NAMESPACE
