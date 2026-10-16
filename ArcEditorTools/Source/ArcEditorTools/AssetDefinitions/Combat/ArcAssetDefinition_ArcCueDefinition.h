// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcCues/ArcCueDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcCueDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcCueDefinition"

UCLASS()
class UArcAssetDefinition_ArcCueDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Cue Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(215, 95, 65));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcCueDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Combat", "Combat"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
