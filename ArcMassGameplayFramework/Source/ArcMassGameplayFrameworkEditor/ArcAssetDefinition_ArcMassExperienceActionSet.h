// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Experience/ArcMassExperienceActionSet.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassExperienceActionSet.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassExperienceActionSet"

UCLASS()
class UArcAssetDefinition_ArcMassExperienceActionSet : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Experience Action Set");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(150, 105, 200));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassExperienceActionSet::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("GameFramework", "Game Framework"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
