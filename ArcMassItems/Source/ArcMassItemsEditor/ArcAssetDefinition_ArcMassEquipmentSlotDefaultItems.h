// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Equipment/ArcMassEquipmentSlotPreset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassEquipmentSlotDefaultItems.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassEquipmentSlotDefaultItems"

UCLASS()
class UArcAssetDefinition_ArcMassEquipmentSlotDefaultItems : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Equipment Slot Default Items");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(60, 145, 205));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassEquipmentSlotDefaultItems::StaticClass();
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
