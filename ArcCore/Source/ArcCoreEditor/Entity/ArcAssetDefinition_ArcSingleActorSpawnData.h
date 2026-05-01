// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcSingleActorSpawner.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcSingleActorSpawnData.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcSingleActorSpawnData"

UCLASS()
class UArcAssetDefinition_ArcSingleActorSpawnData : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Single Actor Spawn Data");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(85, 190, 115));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcSingleActorSpawnData::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Entity", "Entity"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
