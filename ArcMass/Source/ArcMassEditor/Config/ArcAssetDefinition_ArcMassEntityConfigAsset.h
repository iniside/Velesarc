// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcAssetDefinition_ArcMassEntityConfigAsset.generated.h"

UCLASS()
class UArcAssetDefinition_ArcMassEntityConfigAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(0, 157, 255));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassEntityConfigAsset::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;

	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
