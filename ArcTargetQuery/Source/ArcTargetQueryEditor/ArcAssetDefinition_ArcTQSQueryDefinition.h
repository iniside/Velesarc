// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTQSQueryDefinition.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcTQSQueryDefinition.generated.h"

UCLASS()
class UArcAssetDefinition_ArcTQSQueryDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(170, 110, 190));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcTQSQueryDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;

	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
