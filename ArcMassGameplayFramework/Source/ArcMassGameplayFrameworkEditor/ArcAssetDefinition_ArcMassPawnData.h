// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Pawn/ArcMassPawnData.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassPawnData.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassPawnData"

UCLASS()
class UArcAssetDefinition_ArcMassPawnData : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Pawn Data");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(70, 150, 110));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassPawnData::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("GameFramework", "Game Framework"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
