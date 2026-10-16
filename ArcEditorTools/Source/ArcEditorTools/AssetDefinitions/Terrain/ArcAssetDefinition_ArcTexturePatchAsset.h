// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "TerrainTool/ArcTexturePatchAsset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcTexturePatchAsset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcTexturePatchAsset"

UCLASS()
class UArcAssetDefinition_ArcTexturePatchAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Texture Patch");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(80, 130, 60));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcTexturePatchAsset::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Terrain", "Terrain"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
