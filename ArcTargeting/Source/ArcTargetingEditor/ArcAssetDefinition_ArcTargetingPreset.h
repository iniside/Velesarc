// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTargetingPreset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcTargetingPreset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcTargetingPreset"

UCLASS()
class UArcAssetDefinition_ArcTargetingPreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Targeting Preset");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(220, 120, 40));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcTargetingPreset::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Gameplay", "Gameplay"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
