// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "TerrainTool/Graph/ArcHeightmapGraph.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcHeightmapGraph.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcHeightmapGraph"

UCLASS()
class UArcAssetDefinition_ArcHeightmapGraph : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Heightmap Graph");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(80, 130, 60));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcHeightmapGraph::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Terrain", "Terrain"), ECategoryMenuType::Section)
		};
		return Categories;
	}

	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

#undef LOCTEXT_NAMESPACE
