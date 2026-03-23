// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAssetDefinition_MassEntityConfig.h"
#include "ArcAssetEditor_MassEntityConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAssetDefinition_MassEntityConfig)

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_MassEntityConfig"

FText UArcAssetDefinition_MassEntityConfig::GetAssetDisplayName() const
{
    return LOCTEXT("DisplayName", "Mass Entity Config");
}

TConstArrayView<FAssetCategoryPath> UArcAssetDefinition_MassEntityConfig::GetAssetCategories() const
{
    static FAssetCategoryPath AssetPaths[] = {
    	FAssetCategoryPath(LOCTEXT("Gameplay", "Gameplay")),
        FAssetCategoryPath(LOCTEXT("MassCategory", "Arc Mass"))
    };
    return AssetPaths;
}

EAssetCommandResult UArcAssetDefinition_MassEntityConfig::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
    for (UMassEntityConfigAsset* ConfigAsset : OpenArgs.LoadObjects<UMassEntityConfigAsset>())
    {
        FArcAssetEditor_MassEntityConfig::CreateEditor(
            OpenArgs.GetToolkitMode(),
            OpenArgs.ToolkitHost,
            ConfigAsset);
    }
    return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
