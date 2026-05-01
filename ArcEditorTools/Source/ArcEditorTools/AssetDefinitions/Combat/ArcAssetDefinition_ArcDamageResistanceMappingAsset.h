// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDamageSystem/ArcDamageResistanceMappingAsset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcDamageResistanceMappingAsset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcDamageResistanceMappingAsset"

UCLASS()
class UArcAssetDefinition_ArcDamageResistanceMappingAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Damage Resistance Mapping");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(215, 95, 65));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcDamageResistanceMappingAsset::StaticClass();
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
