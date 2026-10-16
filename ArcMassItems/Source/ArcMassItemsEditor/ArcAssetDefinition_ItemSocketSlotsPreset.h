// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Fragments/ArcItemFragment_SocketSlots.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ItemSocketSlotsPreset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ItemSocketSlotsPreset"

UCLASS()
class UArcAssetDefinition_ItemSocketSlotsPreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Item Socket Slots Preset");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(215, 130, 70));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcItemSocketSlotsPreset::StaticClass();
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
