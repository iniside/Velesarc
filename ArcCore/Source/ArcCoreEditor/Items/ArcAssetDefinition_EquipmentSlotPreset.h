// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Equipment/ArcEquipmentComponent.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_EquipmentSlotPreset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_EquipmentSlotPreset"

UCLASS()
class UArcAssetDefinition_EquipmentSlotPreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Equipment Slot Preset");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(205, 140, 75));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcEquipmentSlotPreset::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Items", "Items"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
