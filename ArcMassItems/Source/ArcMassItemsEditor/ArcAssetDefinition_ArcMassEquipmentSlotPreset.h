// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Equipment/ArcMassEquipmentSlotPreset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassEquipmentSlotPreset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassEquipmentSlotPreset"

UCLASS()
class UArcAssetDefinition_ArcMassEquipmentSlotPreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Equipment Slot Preset");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(95, 175, 220));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassEquipmentSlotPreset::StaticClass();
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
