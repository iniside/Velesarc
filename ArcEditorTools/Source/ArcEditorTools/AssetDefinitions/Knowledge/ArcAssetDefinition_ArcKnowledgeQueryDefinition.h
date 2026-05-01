// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcKnowledgeQueryDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcKnowledgeQueryDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcKnowledgeQueryDefinition"

UCLASS()
class UArcAssetDefinition_ArcKnowledgeQueryDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Knowledge Query Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(150, 90, 210));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcKnowledgeQueryDefinition::StaticClass();
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
