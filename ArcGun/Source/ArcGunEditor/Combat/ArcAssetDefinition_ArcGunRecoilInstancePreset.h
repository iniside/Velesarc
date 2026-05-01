// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcGunRecoilInstance.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcGunRecoilInstancePreset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcGunRecoilInstancePreset"

UCLASS()
class UArcAssetDefinition_ArcGunRecoilInstancePreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()
public:
	virtual FText GetAssetDisplayName() const override { return LOCTEXT("DisplayName", "Arc Gun Recoil Instance Preset"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(200,85,75)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UArcGunRecoilInstancePreset::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Combat", "Combat"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
