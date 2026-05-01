// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Equipment/ArcEquipmentComponent.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_EquipmentSlotDefaultItems.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_EquipmentSlotDefaultItems"

UCLASS()
class UArcAssetDefinition_EquipmentSlotDefaultItems : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Equipment Slot Default Items");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(230, 155, 55));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcEquipmentSlotDefaultItems::StaticClass();
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
