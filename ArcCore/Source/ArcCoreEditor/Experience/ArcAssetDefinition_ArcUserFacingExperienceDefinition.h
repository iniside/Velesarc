// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameMode/ArcUserFacingExperienceDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcUserFacingExperienceDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcUserFacingExperienceDefinition"

UCLASS()
class UArcAssetDefinition_ArcUserFacingExperienceDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc User Facing Experience Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(50, 170, 190));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcUserFacingExperienceDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Experience", "Experience"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
