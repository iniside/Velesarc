// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Attributes/ArcAttributeHandlerConfig.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcAttributeHandlerConfig.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcAttributeHandlerConfig"

UCLASS()
class UArcAssetDefinition_ArcAttributeHandlerConfig : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Attribute Handler Config");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(200, 160, 80));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcAttributeHandlerConfig::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(LOCTEXT("Gameplay", "Gameplay"))
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
