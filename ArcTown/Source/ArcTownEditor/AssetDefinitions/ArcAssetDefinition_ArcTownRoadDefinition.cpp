// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcAssetDefinition_ArcTownRoadDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAssetDefinition_ArcTownRoadDefinition)

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcTownRoadDefinition"

FText UArcAssetDefinition_ArcTownRoadDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Arc Town Road Definition");
}

TConstArrayView<FAssetCategoryPath> UArcAssetDefinition_ArcTownRoadDefinition::GetAssetCategories() const
{
	static FAssetCategoryPath Categories[] = {
		FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Town", "Town"), ECategoryMenuType::Section)
	};
	return Categories;
}

#undef LOCTEXT_NAMESPACE
