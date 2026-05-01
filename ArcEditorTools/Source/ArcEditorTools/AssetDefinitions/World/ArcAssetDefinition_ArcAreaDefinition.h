// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcAreaDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcAreaDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcAreaDefinition"

UCLASS()
class UArcAssetDefinition_ArcAreaDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Area Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(80, 140, 200));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcAreaDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("World", "World"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
