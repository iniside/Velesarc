// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcGunInstanceBase.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcGunInstancePreset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcGunInstancePreset"

UCLASS()
class UArcAssetDefinition_ArcGunInstancePreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()
public:
	virtual FText GetAssetDisplayName() const override { return LOCTEXT("DisplayName", "Arc Gun Instance Preset"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(190,75,85)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UArcGunInstancePreset::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Combat", "Combat"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
