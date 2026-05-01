// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcGunFireMode.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcGunFireModePreset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcGunFireModePreset"

UCLASS()
class UArcAssetDefinition_ArcGunFireModePreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()
public:
	virtual FText GetAssetDisplayName() const override { return LOCTEXT("DisplayName", "Arc Gun Fire Mode Preset"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(180,70,90)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UArcGunFireModePreset::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Combat", "Combat"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
