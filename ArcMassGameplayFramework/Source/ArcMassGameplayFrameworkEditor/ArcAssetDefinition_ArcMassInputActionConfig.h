// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Input/ArcMassInputActionConfig.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassInputActionConfig.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassInputActionConfig"

UCLASS()
class UArcAssetDefinition_ArcMassInputActionConfig : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Input Action Config");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(225, 200, 75));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassInputActionConfig::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Input", "Input"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
