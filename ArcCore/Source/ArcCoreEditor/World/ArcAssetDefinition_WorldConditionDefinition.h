// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Conditions/ArcWorldConditionDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_WorldConditionDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_WorldConditionDefinition"

UCLASS()
class UArcAssetDefinition_WorldConditionDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc World Condition Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(70, 130, 210));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcWorldConditionDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("World", "World"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
