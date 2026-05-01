// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcKnowledgeEntryDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcKnowledgeEntryDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcKnowledgeEntryDefinition"

UCLASS()
class UArcAssetDefinition_ArcKnowledgeEntryDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Knowledge Entry Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(160, 100, 200));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcKnowledgeEntryDefinition::StaticClass();
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
