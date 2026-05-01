// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Data/ArcSettlementNeedDataAsset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcSettlementNeedDataAsset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcSettlementNeedDataAsset"

UCLASS()
class UArcAssetDefinition_ArcSettlementNeedDataAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Settlement Need Data Asset");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(190, 170, 70));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcSettlementNeedDataAsset::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Economy", "Economy"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
