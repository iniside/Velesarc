// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AbilitySystem/Fragments/ArcItemFragment_AbilityActions.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_AbilityActionsPreset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_AbilityActionsPreset"

UCLASS()
class UArcAssetDefinition_AbilityActionsPreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Ability Actions Preset");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(210, 90, 70));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcAbilityActionsPreset::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Combat", "Combat"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
