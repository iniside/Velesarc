// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Abilities/ArcMassAbilitySet.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassAbilitySet.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassAbilitySet"

UCLASS()
class UArcAssetDefinition_ArcMassAbilitySet : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Ability Set");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(80, 200, 160));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassAbilitySet::StaticClass();
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
