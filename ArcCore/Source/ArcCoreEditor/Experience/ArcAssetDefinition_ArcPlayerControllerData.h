// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameMode/ArcExperienceDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcPlayerControllerData.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcPlayerControllerData"

UCLASS()
class UArcAssetDefinition_ArcPlayerControllerData : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Player Controller Data");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(60, 180, 180));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcPlayerControllerData::StaticClass();
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
