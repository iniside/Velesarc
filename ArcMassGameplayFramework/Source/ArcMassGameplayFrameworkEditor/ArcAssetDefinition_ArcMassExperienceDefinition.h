// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Experience/ArcMassExperienceDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassExperienceDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassExperienceDefinition"

UCLASS()
class UArcAssetDefinition_ArcMassExperienceDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Experience Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(110, 90, 200));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassExperienceDefinition::StaticClass();
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
