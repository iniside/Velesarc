// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AbilitySystem/Targeting/ArcTargetingObject.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_TargetingObject.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_TargetingObject"

UCLASS()
class UArcAssetDefinition_TargetingObject : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Targeting Object");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(200, 80, 80));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcTargetingObject::StaticClass();
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
