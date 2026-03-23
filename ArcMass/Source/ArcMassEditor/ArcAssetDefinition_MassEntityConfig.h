// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "MassEntityConfigAsset.h"
#include "ArcAssetDefinition_MassEntityConfig.generated.h"

UCLASS()
class UArcAssetDefinition_MassEntityConfig : public UAssetDefinitionDefault
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
        return UMassEntityConfigAsset::StaticClass();
    }

    virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;

    virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
