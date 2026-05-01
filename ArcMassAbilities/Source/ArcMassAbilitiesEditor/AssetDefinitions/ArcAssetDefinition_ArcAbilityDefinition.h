// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Abilities/ArcAbilityDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcAbilityDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcAbilityDefinition"

UCLASS()
class UArcAssetDefinition_ArcAbilityDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Ability Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(80, 160, 200));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcAbilityDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(LOCTEXT("Gameplay", "Gameplay"))
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
