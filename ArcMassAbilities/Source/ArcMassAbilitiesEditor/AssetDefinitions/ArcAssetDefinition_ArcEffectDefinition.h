// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Effects/ArcEffectDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcEffectDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcEffectDefinition"

UCLASS()
class UArcAssetDefinition_ArcEffectDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Effect Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(200, 80, 80));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcEffectDefinition::StaticClass();
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
