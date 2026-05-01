// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Factory/ArcItemFactoryGrantedPassiveAbilities.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ItemFactoryGrantedPassiveAbilities.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ItemFactoryGrantedPassiveAbilities"

UCLASS()
class UArcAssetDefinition_ItemFactoryGrantedPassiveAbilities : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Item Factory Granted Passive Abilities");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(240, 155, 45));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcItemFactoryGrantedPassiveAbilities::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Items", "Items"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
