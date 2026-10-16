// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcAssetDefinition_ArcMassEntityConfigTemplate.generated.h"

UCLASS()
class UArcAssetDefinition_ArcMassEntityConfigTemplate : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(120, 80, 220));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassEntityConfigTemplate::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;

	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
