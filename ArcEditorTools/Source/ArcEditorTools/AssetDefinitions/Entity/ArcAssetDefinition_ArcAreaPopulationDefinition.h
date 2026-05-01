// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PopulationSpawner/ArcAreaPopulationTypes.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcAreaPopulationDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcAreaPopulationDefinition"

UCLASS()
class UArcAssetDefinition_ArcAreaPopulationDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Area Population Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(90, 185, 110));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcAreaPopulationDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Entity", "Entity"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
