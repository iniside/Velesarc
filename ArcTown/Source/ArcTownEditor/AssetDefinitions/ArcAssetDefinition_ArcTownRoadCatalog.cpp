// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcAssetDefinition_ArcTownRoadCatalog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAssetDefinition_ArcTownRoadCatalog)

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcTownRoadCatalog"

FText UArcAssetDefinition_ArcTownRoadCatalog::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Arc Town Road Catalog");
}

TConstArrayView<FAssetCategoryPath> UArcAssetDefinition_ArcTownRoadCatalog::GetAssetCategories() const
{
	static FAssetCategoryPath Categories[] = {
		FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Town", "Town"), ECategoryMenuType::Section)
	};
	return Categories;
}

#undef LOCTEXT_NAMESPACE
