// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcCraft/Recipe/ArcRandomModifierEntry.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcRandomModifierEntry.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcRandomModifierEntry"

UCLASS()
class UArcAssetDefinition_ArcRandomModifierEntry : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Random Modifier Entry");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(210, 185, 50));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcRandomModifierEntry::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Economy", "Economy"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
