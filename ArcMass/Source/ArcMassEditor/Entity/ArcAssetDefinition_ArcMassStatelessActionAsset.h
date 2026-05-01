// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMass/Actions/ArcMassActionAsset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassStatelessActionAsset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassStatelessActionAsset"

UCLASS()
class UArcAssetDefinition_ArcMassStatelessActionAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Stateless Action");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(80, 180, 120));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassStatelessActionAsset::StaticClass();
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
