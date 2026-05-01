// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMass/Actions/ArcMassActionAsset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassStatefulActionAsset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassStatefulActionAsset"

UCLASS()
class UArcAssetDefinition_ArcMassStatefulActionAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Stateful Action");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(70, 170, 130));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassStatefulActionAsset::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Entity", "Entity"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
