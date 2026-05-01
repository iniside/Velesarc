// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTQSQueryDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcTQSQueryDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcTQSQueryDefinition"

UCLASS()
class UArcAssetDefinition_ArcTQSQueryDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc TQS Query Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(170, 110, 190));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcTQSQueryDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Knowledge", "Knowledge"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
